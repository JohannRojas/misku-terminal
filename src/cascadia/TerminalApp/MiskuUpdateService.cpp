// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include "pch.h"
#include "MiskuUpdateService.h"

#include <appxpackaging.h>
#include <shlwapi.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Security.Cryptography.Core.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Web.Http.Filters.h>
#include <winrt/Windows.Web.Http.Headers.h>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Web::Http;
using namespace winrt::Windows::System::Threading;

namespace Misku::Updates
{
    hstring StatusText(const Snapshot& snapshot)
    {
        switch (snapshot.State)
        {
        case Status::Unsupported: return RS_(L"MiskuUpdateUnsupported");
        case Status::Checking: return RS_(L"MiskuUpdateChecking");
        case Status::NoUpdates: return RS_(L"MiskuUpdateNone");
        case Status::Available:
            return hstring{ std::wstring{ RS_(L"MiskuUpdateAvailable") } + L" " + std::wstring{ to_hstring(snapshot.Update->PackageVersion.Display()) } };
        case Status::Downloading:
            return hstring{ std::wstring{ RS_(L"MiskuUpdateDownloading") } + L" " + std::to_wstring(snapshot.Progress) + L"%" };
        case Status::Ready: return RS_(L"MiskuUpdateReady");
        case Status::Error: return RS_(snapshot.Update ? L"MiskuUpdateInstallError" : L"MiskuUpdateCheckError");
        default: return RS_(L"MiskuUpdateIdle");
        }
    }

    namespace
    {
        constexpr uint32_t FeedLimit = 16 * 1024;

        int64_t Now() noexcept
        {
            return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }

        template<typename T>
        auto CancelAfter(const T& operation, const std::chrono::seconds timeout)
        {
            auto timer = ThreadPoolTimer::CreateTimer([operation](auto&&) { operation.Cancel(); }, timeout);
            return wil::scope_exit([timer] { timer.Cancel(); });
        }

        HttpClient Client()
        {
            Filters::HttpBaseProtocolFilter filter;
            filter.AllowUI(false);
            filter.CacheControl().ReadBehavior(Filters::HttpCacheReadBehavior::MostRecent);
            HttpClient client{ filter };
            client.DefaultRequestHeaders().UserAgent().ParseAdd(L"MiskuTerminal-Updater/1.0");
            return client;
        }

        // An exclusive file handle coalesces checks/installations across windows,
        // elevated instances and separate processes sharing the same app data.
        wil::unique_hfile AcquireUpdateLock()
        {
            const auto folder = ApplicationData::Current().LocalCacheFolder();
            const auto path = std::wstring{ folder.Path() } + L"\\misku-update.lock";
            return wil::unique_hfile{ CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                                 FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr) };
        }

        void ValidatePackage(const StorageFile& file, const Release& release)
        {
            com_ptr<IStream> stream;
            check_hresult(SHCreateStreamOnFileEx(file.Path().c_str(), STGM_READ | STGM_SHARE_DENY_WRITE, 0, FALSE, nullptr, stream.put()));
            com_ptr<IAppxFactory> factory;
            check_hresult(CoCreateInstance(__uuidof(AppxFactory), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IAppxFactory), factory.put_void()));
            com_ptr<IAppxPackageReader> reader;
            check_hresult(factory->CreatePackageReader(stream.get(), reader.put()));
            com_ptr<IAppxManifestReader> manifest;
            check_hresult(reader->GetManifest(manifest.put()));
            com_ptr<IAppxManifestPackageId> identity;
            check_hresult(manifest->GetPackageId(identity.put()));
            wil::unique_cotaskmem_string name, publisher;
            UINT64 version{};
            APPX_PACKAGE_ARCHITECTURE architecture{};
            check_hresult(identity->GetName(name.put()));
            check_hresult(identity->GetPublisher(publisher.put()));
            check_hresult(identity->GetVersion(&version));
            check_hresult(identity->GetArchitecture(&architecture));
            if (std::wstring_view{ name.get() } != L"MiskuTerminal" || std::wstring_view{ publisher.get() } != L"CN=Misku Terminal" ||
                architecture != APPX_PACKAGE_ARCHITECTURE_X64 || version != release.PackageVersion.Number())
            {
                throw hresult_invalid_argument(L"Unexpected update package identity");
            }
        }
    }

    Service& Service::Current()
    {
        static Service service;
        return service;
    }

    Service::Service() noexcept
    {
        try
        {
            const auto id = winrt::Windows::ApplicationModel::Package::Current().Id();
            if (id.Name() != L"MiskuTerminal" || id.Publisher() != L"CN=Misku Terminal" ||
                id.Architecture() != winrt::Windows::System::ProcessorArchitecture::X64) { return; }
            const auto version = id.Version();
            _installedVersion = Version{ { version.Major, version.Minor, version.Build, version.Revision } }.Number();
            _settings = ApplicationData::Current().LocalSettings().Values();
            _snapshot.State = Status::Idle;
            _snapshot.Automatic = unbox_value_or<bool>(_settings.TryLookup(L"MiskuUpdates.Automatic"), true);
            _snapshot.Preview = unbox_value_or<bool>(_settings.TryLookup(L"MiskuUpdates.Preview"), false);
            const auto cached = unbox_value_or<hstring>(_settings.TryLookup(Key(L"Feed", _snapshot.Preview)), L"");
            if (const auto update = ParseFeed(cached, _snapshot.Preview); update && update->PackageVersion.Number() > _installedVersion)
            {
                _snapshot.Update = update;
                const auto pending = unbox_value_or<hstring>(_settings.TryLookup(L"MiskuUpdates.Pending"), L"");
                _snapshot.State = pending == to_hstring(update->PackageVersion.String()) ? Status::Ready : Status::Available;
                _snapshot.Dismissed = unbox_value_or<hstring>(_settings.TryLookup(Key(L"Dismissed", _snapshot.Preview)), L"") == to_hstring(update->PackageVersion.String());
            }
        }
        catch (...) { LOG_CAUGHT_EXCEPTION(); }
    }

    hstring Service::Key(std::wstring_view suffix, const bool preview) const
    {
        return hstring{ std::wstring{ preview ? L"MiskuUpdates.Preview." : L"MiskuUpdates.Stable." } + std::wstring{ suffix } };
    }

    Snapshot Service::GetSnapshot() const
    {
        const std::scoped_lock lock{ _mutex };
        return _snapshot;
    }

    void Service::Notify() noexcept
    {
        try { Changed(nullptr, nullptr); } CATCH_LOG();
    }

    void Service::SetState(const Status status, const bool manual, const int32_t error)
    {
        {
            const std::scoped_lock lock{ _mutex };
            _snapshot.State = status;
            _snapshot.Manual = manual;
            _snapshot.ErrorCode = error;
        }
        Notify();
    }

    void Service::Start()
    {
        {
            const std::scoped_lock lock{ _mutex };
            if (_started || _snapshot.State == Status::Unsupported) { return; }
            _started = true;
        }
        _timer = ThreadPoolTimer::CreatePeriodicTimer([](auto&&) { Current().Check(); }, std::chrono::hours{ 24 });
        Check();
    }

    std::optional<Release> Service::ParseFeed(const hstring& json, const bool preview)
    {
        try
        {
            if (json.size() > FeedLimit) { return std::nullopt; }
            const auto root = winrt::Windows::Data::Json::JsonObject::Parse(json);
            if (root.GetNamedNumber(L"schemaVersion") != 1 || root.GetNamedString(L"packageName") != L"MiskuTerminal" ||
                root.GetNamedString(L"publisher") != L"CN=Misku Terminal" || root.GetNamedString(L"architecture") != L"x64" ||
                root.GetNamedBoolean(L"preview") != preview) { return std::nullopt; }
            const auto version = Version::Parse(to_string(root.GetNamedString(L"version")));
            if (!version) { return std::nullopt; }
            Release result{ *version, to_string(root.GetNamedString(L"packageUri")), to_string(root.GetNamedString(L"sha256")), preview };
            if (result.Valid()) { return result; }
        }
        catch (...) { }
        return std::nullopt;
    }

    fire_and_forget Service::Check(const bool manual)
    {
        try
        {
            co_await resume_background();
            bool preview;
            {
                const std::scoped_lock lock{ _mutex };
                if (_busy || !_settings || (!manual && !_snapshot.Automatic) || _snapshot.State == Status::Ready) { co_return; }
                _busy = true;
                preview = _snapshot.Preview;
            }
            auto clearBusy = wil::scope_exit([&] { const std::scoped_lock lock{ _mutex }; _busy = false; });
            auto fileLock = AcquireUpdateLock();
            if (!fileLock) { co_return; }
            const auto now = Now();
            if (!manual && !CheckDue(now, unbox_value_or<int64_t>(_settings.TryLookup(Key(L"LastCheck", preview)), 0))) { co_return; }
            _settings.Insert(Key(L"LastCheck", preview), box_value(now));
            SetState(Status::Checking, manual);
            auto client = Client();
            const Uri feedUri{ to_hstring(std::string{ Repository } + (preview ? "/releases/download/misku-update-preview/update.json" : "/releases/download/misku-update-stable/update.json")) };
            const auto request = client.GetAsync(feedUri, HttpCompletionOption::ResponseHeadersRead);
            auto requestTimeout = CancelAfter(request, std::chrono::seconds{ 20 });
            const auto response = co_await request;
            requestTimeout.reset();
            if (response.RequestMessage().RequestUri().SchemeName() != L"https") { throw hresult_invalid_argument(L"Insecure update redirect"); }
            if (response.StatusCode() == HttpStatusCode::NotFound)
            {
                // No published channel yet. This is not an application error.
                { const std::scoped_lock lock{ _mutex }; _snapshot.Update.reset(); }
                SetState(Status::NoUpdates, manual);
                co_return;
            }
            response.EnsureSuccessStatusCode();
            const auto stream = co_await response.Content().ReadAsInputStreamAsync();
            Buffer buffer{ FeedLimit + 1 };
            const auto read = stream.ReadAsync(buffer, FeedLimit + 1, InputStreamOptions::None);
            auto readTimeout = CancelAfter(read, std::chrono::seconds{ 20 });
            const auto bytes = co_await read;
            readTimeout.reset();
            if (bytes.Length() > FeedLimit) { throw hresult_invalid_argument(L"Update feed too large"); }
            const auto json = winrt::Windows::Security::Cryptography::CryptographicBuffer::ConvertBinaryToString(winrt::Windows::Security::Cryptography::BinaryStringEncoding::Utf8, bytes);
            const auto update = ParseFeed(json, preview);
            if (!update) { throw hresult_invalid_argument(L"Invalid update feed"); }
            _settings.Insert(Key(L"Feed", preview), box_value(json));
            {
                const std::scoped_lock lock{ _mutex };
                _snapshot.Update = update->PackageVersion.Number() > _installedVersion ? update : std::nullopt;
                _snapshot.Dismissed = !manual && unbox_value_or<hstring>(_settings.TryLookup(Key(L"Dismissed", preview)), L"") == to_hstring(update->PackageVersion.String());
            }
            SetState(update->PackageVersion.Number() > _installedVersion ? Status::Available : Status::NoUpdates, manual);
        }
        catch (...) { SetState(Status::Error, manual, to_hresult()); }
    }

    fire_and_forget Service::Install()
    {
        StorageFile download{ nullptr };
        try
        {
            co_await resume_background();
            Release update;
            {
                const std::scoped_lock lock{ _mutex };
                if (_busy || !_settings || !_snapshot.Update || _snapshot.State == Status::Ready) { co_return; }
                update = *_snapshot.Update;
                if (!update.Valid() || update.PackageVersion.Number() <= _installedVersion) { co_return; }
                _busy = true;
                _snapshot.Dismissed = false;
                _snapshot.Progress = 0;
            }
            auto clearBusy = wil::scope_exit([&] { const std::scoped_lock lock{ _mutex }; _busy = false; });
            auto fileLock = AcquireUpdateLock();
            if (!fileLock) { throw hresult_error(HRESULT_FROM_WIN32(ERROR_BUSY)); }
            SetState(Status::Downloading, true);
            auto client = Client();
            const auto request = client.GetAsync(Uri{ to_hstring(update.PackageUri) }, HttpCompletionOption::ResponseHeadersRead);
            auto requestTimeout = CancelAfter(request, std::chrono::seconds{ 30 });
            const auto response = co_await request;
            requestTimeout.reset();
            if (response.RequestMessage().RequestUri().SchemeName() != L"https") { throw hresult_invalid_argument(L"Insecure update redirect"); }
            response.EnsureSuccessStatusCode();
            const auto size = response.Content().Headers().ContentLength();
            if (size && size.Value() > MaximumPackageBytes) { throw hresult_invalid_argument(L"Update package too large"); }
            download = co_await ApplicationData::Current().LocalCacheFolder().CreateFileAsync(L"misku-update.msix", CreationCollisionOption::GenerateUniqueName);
            const auto input = co_await response.Content().ReadAsInputStreamAsync();
            const auto output = co_await download.OpenAsync(FileAccessMode::ReadWrite);
            auto closeStreams = wil::scope_exit([&] { output.Close(); input.Close(); });
            const auto hash = winrt::Windows::Security::Cryptography::Core::HashAlgorithmProvider::OpenAlgorithm(L"SHA256").CreateHash();
            Buffer buffer{ 64 * 1024 };
            uint64_t received{};
            uint32_t lastProgress{};
            const auto started = std::chrono::steady_clock::now();
            for (;;)
            {
                if (std::chrono::steady_clock::now() - started > std::chrono::minutes{ 10 }) { throw hresult_error(HRESULT_FROM_WIN32(ERROR_TIMEOUT)); }
                const auto read = input.ReadAsync(buffer, buffer.Capacity(), InputStreamOptions::Partial);
                auto readTimeout = CancelAfter(read, std::chrono::seconds{ 30 });
                const auto bytes = co_await read;
                readTimeout.reset();
                if (!bytes.Length()) { break; }
                received += bytes.Length();
                if (received > MaximumPackageBytes) { throw hresult_invalid_argument(L"Update package too large"); }
                hash.Append(bytes);
                co_await output.WriteAsync(bytes);
                const auto progress = size && size.Value() ? static_cast<uint32_t>(std::min<uint64_t>(90, received * 90 / size.Value())) : 0u;
                if (progress != lastProgress)
                {
                    lastProgress = progress;
                    { const std::scoped_lock lock{ _mutex }; _snapshot.Progress = progress; }
                    Notify();
                }
            }
            co_await output.FlushAsync();
            closeStreams.reset();
            auto digest = to_string(winrt::Windows::Security::Cryptography::CryptographicBuffer::EncodeToHexString(hash.GetValueAndReset()));
            for (auto& ch : digest) { if (ch >= 'A' && ch <= 'F') { ch += 'a' - 'A'; } }
            if (received == 0 || digest != update.Sha256) { throw hresult_error(TRUST_E_BAD_DIGEST); }
            ValidatePackage(download, update);
            { const std::scoped_lock lock{ _mutex }; _snapshot.Progress = 95; }
            Notify();
            winrt::Windows::Management::Deployment::AddPackageOptions options;
            options.DeferRegistrationWhenPackagesAreInUse(true);
            options.ForceAppShutdown(false);
            options.ForceTargetAppShutdown(false);
            options.ForceUpdateFromAnyVersion(false);
            winrt::Windows::Management::Deployment::PackageManager manager;
            const auto result = co_await manager.AddPackageByUriAsync(Uri{ download.Path() }, options);
            check_hresult(result.ExtendedErrorCode());
            _settings.Insert(L"MiskuUpdates.Pending", box_value(to_hstring(update.PackageVersion.String())));
            { const std::scoped_lock lock{ _mutex }; _snapshot.Progress = 100; }
            // Windows has staged and verified the signed package. Registration is
            // deferred until ALL processes using Misku close; no shell is killed.
            SetState(Status::Ready, true);
        }
        catch (...) { SetState(Status::Error, true, to_hresult()); }
        if (download)
        {
            try { co_await download.DeleteAsync(StorageDeleteOption::PermanentDelete); } CATCH_LOG();
        }
    }

    void Service::Dismiss()
    {
        try
        {
            {
                const std::scoped_lock lock{ _mutex };
                _snapshot.Dismissed = true;
                if (_settings && _snapshot.Update)
                {
                    _settings.Insert(Key(L"Dismissed", _snapshot.Preview), box_value(to_hstring(_snapshot.Update->PackageVersion.String())));
                }
            }
            Notify();
        }
        CATCH_LOG();
    }

    void Service::Preferences(const bool automatic, const bool preview)
    {
        try
        {
            {
                const std::scoped_lock lock{ _mutex };
                if (_busy || !_settings || _snapshot.State == Status::Ready) { return; }
                _settings.Insert(L"MiskuUpdates.Automatic", box_value(automatic));
                _settings.Insert(L"MiskuUpdates.Preview", box_value(preview));
                _snapshot.Automatic = automatic;
                if (_snapshot.Preview != preview)
                {
                    _snapshot.Preview = preview;
                    _snapshot.Update.reset();
                    _snapshot.Dismissed = false;
                    _snapshot.State = Status::Idle;
                }
            }
            Notify();
        }
        CATCH_LOG();
    }
}

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once

#include "MiskuUpdateModel.h"
#include <mutex>
#include <winrt/Windows.System.Threading.h>

namespace Misku::Updates
{
    winrt::hstring StatusText(const Snapshot& snapshot);
    class Service
    {
    public:
        static Service& Current();
        Snapshot GetSnapshot() const;
        void Start();
        winrt::fire_and_forget Check(bool manual = false);
        winrt::fire_and_forget Install();
        void Dismiss();
        void Preferences(bool automatic, bool preview);
        static std::optional<Release> ParseFeed(const winrt::hstring& json, bool preview);

        winrt::event<winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>> Changed;

    private:
        Service() noexcept;
        void Notify() noexcept;
        winrt::hstring Key(std::wstring_view suffix, bool preview) const;
        void SetState(Status status, bool manual, int32_t error = 0);
        winrt::Windows::Foundation::Collections::IPropertySet _settings{ nullptr };
        winrt::Windows::System::Threading::ThreadPoolTimer _timer{ nullptr };
        uint64_t _installedVersion{};
        mutable std::mutex _mutex;
        Snapshot _snapshot;
        bool _started{};
        bool _busy{};
    };
}

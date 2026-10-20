
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AboutDialog.h"
#include "MiskuUpdateService.h"
#include "AboutDialog.g.cpp"

#include <WtExeUtils.h>

#include "../../types/inc/utils.hpp"
#include "Utils.h"

using namespace winrt;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal;
using namespace ::TerminalApp;
using namespace std::chrono_literals;

namespace winrt
{
    namespace WUX = Windows::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace winrt::TerminalApp::implementation
{
    AboutDialog::AboutDialog()
    {
        InitializeComponent();
        _queueUpdateCheck();
    }

    winrt::hstring AboutDialog::ApplicationDisplayName()
    {
        return CascadiaSettings::ApplicationDisplayName();
    }

    winrt::hstring AboutDialog::ApplicationVersion()
    {
        return CascadiaSettings::ApplicationVersion();
    }

    void AboutDialog::_SendFeedbackOnClick(const IInspectable& /*sender*/, const Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs& /*eventArgs*/)
    {
#if defined(WT_BRANDING_RELEASE)
        ShellExecute(nullptr, nullptr, L"https://github.com/JohannRojas/misku-terminal/issues", nullptr, nullptr, SW_SHOW);
#else
        ShellExecute(nullptr, nullptr, L"https://github.com/JohannRojas/misku-terminal/issues", nullptr, nullptr, SW_SHOW);
#endif
    }

    void AboutDialog::_ThirdPartyNoticesOnClick(const IInspectable& /*sender*/, const Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/)
    {
        std::filesystem::path currentPath{ wil::GetModuleFileNameW<std::wstring>(nullptr) };
        currentPath.replace_filename(L"NOTICE.html");
        ShellExecute(nullptr, nullptr, currentPath.c_str(), nullptr, nullptr, SW_SHOW);
    }

    AboutDialog::~AboutDialog()
    {
        Misku::Updates::Service::Current().Changed.remove(_updateChanged);
    }

    safe_void_coroutine AboutDialog::_queueUpdateCheck()
    {
        const auto weak = get_weak();
        auto& service = Misku::Updates::Service::Current();
        _updateChanged = service.Changed.add([weak](auto&&, auto&&) {
            if (auto self = weak.get())
            {
                self->Dispatcher().RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, [weak] {
                    if (auto current = weak.get()) { current->_RefreshUpdates(); }
                });
            }
        });
        _RefreshUpdates();
        service.Start();
        co_return;
    }

    void AboutDialog::_RefreshUpdates()
    {
        using Misku::Updates::Status;
        const auto state = Misku::Updates::Service::Current().GetSnapshot();
        const auto supported = state.State != Status::Unsupported;
        const auto busy = state.State == Status::Checking || state.State == Status::Downloading;
        const auto ready = state.State == Status::Ready;
        _refreshingUpdates = true;
        auto reset = wil::scope_exit([&] { _refreshingUpdates = false; });
        MiskuUpdateStatus().Text(Misku::Updates::StatusText(state));
        MiskuCheckUpdatesButton().IsEnabled(supported && !busy && !ready);
        MiskuInstallUpdateButton().Visibility(state.Update && !ready ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed);
        MiskuInstallUpdateButton().IsEnabled(!busy);
        MiskuReleaseNotesButton().Visibility(state.Update ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed);
        MiskuAutomaticUpdates().IsOn(state.Automatic);
        MiskuPreviewUpdates().IsOn(state.Preview);
        MiskuAutomaticUpdates().IsEnabled(supported && !busy && !ready);
        MiskuPreviewUpdates().IsEnabled(supported && !busy && !ready);
        MiskuUpdateProgress().IsActive(busy);
        MiskuUpdateProgress().Visibility(busy ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed);
    }

    void AboutDialog::_CheckUpdates(const IInspectable&, const Windows::UI::Xaml::RoutedEventArgs&)
    {
        Misku::Updates::Service::Current().Check(true);
    }

    void AboutDialog::_InstallUpdate(const IInspectable&, const Windows::UI::Xaml::RoutedEventArgs&)
    {
        Misku::Updates::Service::Current().Install();
    }

    void AboutDialog::_UpdateNotes(const IInspectable&, const Windows::UI::Xaml::RoutedEventArgs&)
    {
        if (const auto state = Misku::Updates::Service::Current().GetSnapshot(); state.Update)
        {
            ShellExecute(nullptr, L"open", to_hstring(state.Update->NotesUri()).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    }

    void AboutDialog::_UpdatePreferences(const IInspectable&, const Windows::UI::Xaml::RoutedEventArgs&)
    {
        if (!_refreshingUpdates)
        {
            auto& service = Misku::Updates::Service::Current();
            service.Preferences(MiskuAutomaticUpdates().IsOn(), MiskuPreviewUpdates().IsOn());
            service.Check();
        }
    }

}

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Implementation of the TitlebarControl class
//

#include "pch.h"

#include "TitlebarControl.h"
#include "../../types/inc/ColorFix.hpp"

#include "TitlebarControl.g.cpp"

using namespace winrt::Windows::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    static constexpr double expandedChromeHeight{ 40.0 };
    static constexpr double collapsedChromeHeight{ 3.0 };
    static constexpr auto chromeHideDelay{ std::chrono::milliseconds{ 180 } };

    TitlebarControl::TitlebarControl(uint64_t handle) :
        _window{ reinterpret_cast<HWND>(handle) }
    {
        InitializeComponent();

        _chromePinned = Microsoft::Terminal::Settings::Model::ApplicationState::SharedInstance().MiskuTitlebarPinned();
        PinChromeButton().IsChecked(_chromePinned);
        _hideTimer.Interval(chromeHideDelay);
        _hideTimer.Tick([weakThis = get_weak()](auto&&, auto&&) {
            if (const auto self{ weakThis.get() })
            {
                self->_hideTimer.Stop();
                if (!self->_chromePinned && !self->_xamlPointerOver && !self->_nonClientPointerOver && !self->_keyboardFocusWithin)
                {
                    self->_setChromeVisible(false);
                }
            }
        });

        // Collapse the chrome to a narrow reveal target. Unlike opacity-only
        // hiding, this returns the unused titlebar space to the terminal.
        _setChromeVisible(_chromePinned);

        // Register our event handlers on the MMC buttons.
        MinMaxCloseControl().MinimizeClick({ this, &TitlebarControl::Minimize_Click });
        MinMaxCloseControl().MaximizeClick({ this, &TitlebarControl::Maximize_Click });
        MinMaxCloseControl().CloseClick({ this, &TitlebarControl::Close_Click });

        // Listen for changes to the Background. If the Background changes,
        // we'll want to manually adjust the RequestedTheme of our caption
        // buttons, so the foreground stands out against whatever BG color was
        // selected for us.
        //
        // This is how you register a PropertyChanged event for the Background
        // property of a Grid. The Background property is defined in the base
        // class Panel.
        const auto bgProperty{ winrt::Windows::UI::Xaml::Controls::Panel::BackgroundProperty() };
        RegisterPropertyChangedCallback(bgProperty, [weakThis = get_weak(), bgProperty](auto& /*sender*/, auto& e) {
            if (auto self{ weakThis.get() })
            {
                if (e == bgProperty)
                {
                    self->_backgroundChanged(self->Background());
                }
            }
        });
    }

    float TitlebarControl::CaptionButtonWidth()
    {
        // Divide by three, since we know there are only three buttons. When
        // Windows 12 comes along and adds another, we can update this /s
        const auto minMaxCloseWidth = MinMaxCloseControl().ActualWidth();
        return static_cast<float>(minMaxCloseWidth) / 3.0f;
    }

    float TitlebarControl::AutoHideRevealHeight()
    {
        return static_cast<float>(collapsedChromeHeight);
    }

    bool TitlebarControl::ChromeVisible()
    {
        return _chromeVisible;
    }

    bool TitlebarControl::ChromePinned()
    {
        return _chromePinned;
    }

    void TitlebarControl::PinChrome_Click(const IInspectable&, const Windows::UI::Xaml::RoutedEventArgs&)
    {
        _chromePinned = PinChromeButton().IsChecked().Value();
        Microsoft::Terminal::Settings::Model::ApplicationState::SharedInstance().MiskuTitlebarPinned(_chromePinned);
        ChromePinnedChanged.raise(*this, nullptr);
        _updateAutoHideState();
    }

    bool TitlebarControl::Focused()
    {
        return MinMaxCloseControl().Focused();
    }

    void TitlebarControl::Focused(bool focused)
    {
        MinMaxCloseControl().Focused(focused);

        if (!focused)
        {
            _xamlPointerOver = false;
            _nonClientPointerOver = false;
            _keyboardFocusWithin = false;
            _updateAutoHideState();
        }
    }

    IInspectable TitlebarControl::Content()
    {
        return ContentRoot().Content();
    }

    void TitlebarControl::Content(IInspectable content)
    {
        ContentRoot().Content(content);
    }

    void TitlebarControl::Root_SizeChanged(const IInspectable& /*sender*/,
                                           const Windows::UI::Xaml::SizeChangedEventArgs& /*e*/)
    {
        const auto windowWidth = ActualWidth();
        const auto minMaxCloseWidth = MinMaxCloseControl().ActualWidth();
        const auto dragBarMinWidth = DragBar().MinWidth();
        const auto maxWidth = windowWidth - minMaxCloseWidth - dragBarMinWidth - PinChromeButton().ActualWidth();
        // Only set our MaxWidth if it's greater than 0. Setting it to a
        // negative value will cause a crash.
        if (maxWidth >= 0)
        {
            ContentRoot().MaxWidth(maxWidth);
        }
    }

    void TitlebarControl::Root_PointerEntered(const IInspectable& /*sender*/,
                                              const Windows::UI::Xaml::Input::PointerRoutedEventArgs& /*e*/)
    {
        _xamlPointerOver = true;
        _updateAutoHideState();
    }

    void TitlebarControl::Root_PointerExited(const IInspectable& /*sender*/,
                                             const Windows::UI::Xaml::Input::PointerRoutedEventArgs& /*e*/)
    {
        _xamlPointerOver = false;
        _updateAutoHideState();
    }

    void TitlebarControl::Root_GotFocus(const IInspectable& /*sender*/,
                                        const Windows::UI::Xaml::RoutedEventArgs& e)
    {
        // Pointer focus should not pin the titlebar open after the mouse leaves.
        // Keep keyboard navigation discoverable without changing hover behavior.
        const auto focusedControl{ e.OriginalSource().try_as<Controls::Control>() };
        _keyboardFocusWithin = focusedControl && focusedControl.FocusState() == FocusState::Keyboard;
        _updateAutoHideState();
    }

    void TitlebarControl::Root_LostFocus(const IInspectable& /*sender*/,
                                         const Windows::UI::Xaml::RoutedEventArgs& /*e*/)
    {
        _keyboardFocusWithin = false;
        _updateAutoHideState();
    }

    void TitlebarControl::SetNonClientPointerOver(bool pointerOver)
    {
        _nonClientPointerOver = pointerOver;
        _updateAutoHideState();
    }

    void TitlebarControl::_updateAutoHideState()
    {
        const auto shouldShow = _chromePinned || _xamlPointerOver || _nonClientPointerOver || _keyboardFocusWithin;
        if (shouldShow)
        {
            _hideTimer.Stop();
            _setChromeVisible(true);
        }
        else if (_chromeVisible)
        {
            // Debounce the handoff between the native drag region and XAML
            // tabs so crossing their boundary does not flicker the chrome.
            _hideTimer.Stop();
            _hideTimer.Start();
        }
    }

    void TitlebarControl::_setChromeVisible(bool visible)
    {
        if (_chromeVisible == visible)
        {
            return;
        }

        _chromeVisible = visible;
        if (visible)
        {
            Height(expandedChromeHeight);
            ChromeRoot().Opacity(1.0);
            ChromeRoot().IsHitTestVisible(true);
        }
        else
        {
            ChromeRoot().IsHitTestVisible(false);
            ChromeRoot().Opacity(0.0);
            Height(collapsedChromeHeight);
        }
    }

    void TitlebarControl::FullscreenChanged(const bool fullscreen)
    {
        MinMaxCloseControl().Visibility(fullscreen ? Visibility::Collapsed : Visibility::Visible);
    }

    void TitlebarControl::_OnMaximizeOrRestore(byte flag)
    {
        POINT point1 = {};
        ::GetCursorPos(&point1);
        const auto lParam = MAKELPARAM(point1.x, point1.y);
        WINDOWPLACEMENT placement = { sizeof(placement) };
        ::GetWindowPlacement(_window, &placement);
        if (placement.showCmd == SW_SHOWNORMAL)
        {
            ::PostMessage(_window, WM_SYSCOMMAND, SC_MAXIMIZE | flag, lParam);
        }
        else if (placement.showCmd == SW_SHOWMAXIMIZED)
        {
            ::PostMessage(_window, WM_SYSCOMMAND, SC_RESTORE | flag, lParam);
        }
    }

    void TitlebarControl::Maximize_Click(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& /*e*/)
    {
        _OnMaximizeOrRestore(HTMAXBUTTON);
    }

    void TitlebarControl::DragBar_DoubleTapped(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::Input::DoubleTappedRoutedEventArgs& /*e*/)
    {
        _OnMaximizeOrRestore(HTCAPTION);
    }

    void TitlebarControl::Minimize_Click(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& /*e*/)
    {
        if (_window)
        {
            ::PostMessage(_window, WM_SYSCOMMAND, SC_MINIMIZE | HTMINBUTTON, 0);
        }
    }

    void TitlebarControl::Close_Click(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& /*e*/)
    {
        ::PostMessage(_window, WM_SYSCOMMAND, SC_CLOSE, 0);
    }

    void TitlebarControl::SetWindowVisualState(WindowVisualState visualState)
    {
        MinMaxCloseControl().SetWindowVisualState(visualState);
    }

    // GH#9443: HoverButton, PressButton, ClickButton and ReleaseButtons are all
    // used to manually interact with the buttons, in the same way that XAML
    // would normally send events.

    void TitlebarControl::HoverButton(CaptionButton button)
    {
        MinMaxCloseControl().HoverButton(button);
    }
    void TitlebarControl::PressButton(CaptionButton button)
    {
        MinMaxCloseControl().PressButton(button);
    }
    safe_void_coroutine TitlebarControl::ClickButton(CaptionButton button)
    {
        // GH#8587: Handle this on the _next_ pass of the UI thread. If we
        // handle this immediately, then we'll accidentally leave the button in
        // the "Hovered" state when we minimize. This will leave the button
        // visibly hovered in the taskbar preview for our window.
        auto weakThis{ get_weak() };
        co_await MinMaxCloseControl().Dispatcher();
        if (auto self{ weakThis.get() })
        {
            // Just handle this in the same way we would if the button were
            // clicked normally.
            switch (button)
            {
            case CaptionButton::Minimize:
                Minimize_Click(nullptr, nullptr);
                break;
            case CaptionButton::Maximize:
                Maximize_Click(nullptr, nullptr);
                break;
            case CaptionButton::Close:
                Close_Click(nullptr, nullptr);
                break;
            }
        }
    }
    void TitlebarControl::ReleaseButtons()
    {
        MinMaxCloseControl().ReleaseButtons();
    }

    void TitlebarControl::_backgroundChanged(winrt::Windows::UI::Xaml::Media::Brush brush)
    {
        // Loosely cribbed from TerminalPage::_SetNewTabButtonColor
        til::color c;
        if (auto acrylic = brush.try_as<winrt::Windows::UI::Xaml::Media::AcrylicBrush>())
        {
            c = acrylic.TintColor();
        }
        else if (auto solidColor = brush.try_as<winrt::Windows::UI::Xaml::Media::SolidColorBrush>())
        {
            c = solidColor.Color();
        }
        else
        {
            return;
        }

        constexpr auto lightnessThreshold = 0.6f;
        const auto isBrightColor = ColorFix::GetLightness(c) >= lightnessThreshold;
        MinMaxCloseControl().RequestedTheme(isBrightColor ? winrt::Windows::UI::Xaml::ElementTheme::Light :
                                                            winrt::Windows::UI::Xaml::ElementTheme::Dark);
    }

}

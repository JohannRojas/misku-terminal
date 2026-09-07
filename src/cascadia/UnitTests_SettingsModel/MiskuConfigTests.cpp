// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include "pch.h"
#include "../TerminalSettingsModel/MiskuConfig.h"
#include "JsonTestClass.h"
#include <til/io.h>

using namespace WEX::TestExecution;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;

namespace SettingsModelUnitTests
{
    class MiskuConfigTests : public JsonTestClass
    {
        TEST_CLASS(MiskuConfigTests);
        TEST_METHOD(ValidConfigAndReloadAction);
        TEST_METHOD(InvalidNumericValuesAreRejected);
        TEST_METHOD(InvalidConfigKeepsLastValidAndDeletionDisablesOverlay);
    };

    void MiskuConfigTests::ValidConfigAndReloadAction()
    {
        const auto result = ParseMiskuConfig(L"config.misku",
                                             "font-family = Cascadia Mono\nfont-size = 14\nopacity = 0.9\n"
                                             "keybind = ctrl+shift+r=reload_config\nscrollback-lines = 4000\n");
        VERIFY_IS_TRUE(result.json.has_value());
        VERIFY_IS_FALSE(result.error.has_value());
        const auto json = VerifyParseSucceeded(*result.json);
        VERIFY_ARE_EQUAL(14.0, json["profiles"]["defaults"]["fontSize"].asDouble());
        VERIFY_ARE_EQUAL(90, json["profiles"]["defaults"]["opacity"].asInt());
        VERIFY_ARE_EQUAL(4000, json["profiles"]["defaults"]["historySize"].asInt());
        VERIFY_ARE_EQUAL(std::string{ "Terminal.ReloadSettings" }, json["keybindings"][0]["id"].asString());
    }

    void MiskuConfigTests::InvalidNumericValuesAreRejected()
    {
        for (const auto value : { "font-size = nan", "font-size = inf", "font-size = -1", "opacity = nan", "opacity = 101", "scrollback-lines = -1", "background = #gggggg", "foreground = blue", "cursor-style = typo" })
        {
            const auto result = ParseMiskuConfig(L"config.misku", value);
            VERIFY_IS_FALSE(result.json.has_value());
            VERIFY_IS_TRUE(result.error.has_value());
            VERIFY_IS_TRUE(result.error->find(L":1:") != std::wstring::npos);
        }
    }

    void MiskuConfigTests::InvalidConfigKeepsLastValidAndDeletionDisablesOverlay()
    {
        GUID id;
        THROW_IF_FAILED(CoCreateGuid(&id));
        const auto root = std::filesystem::temp_directory_path() / winrt::to_hstring(id).c_str();
        std::filesystem::create_directories(root);
        const auto config = root / L"config.misku";
        const auto readOptionalEnvironment = [](const wchar_t* name) {
            std::wstring value(GetEnvironmentVariableW(name, nullptr, 0), L'\0');
            if (!value.empty())
            {
                value.resize(GetEnvironmentVariableW(name, value.data(), static_cast<DWORD>(value.size())));
            }
            return value;
        };
        const auto previousConfig = readOptionalEnvironment(L"MISKU_CONFIG");
        const auto previousTheme = readOptionalEnvironment(L"MISKU_THEME");
        const auto previousUser = wil::GetEnvironmentVariableW<std::wstring>(L"USERPROFILE");
        const auto previousLocal = wil::GetEnvironmentVariableW<std::wstring>(L"LOCALAPPDATA");
        const auto restore = wil::scope_exit([&]() {
            SetEnvironmentVariableW(L"MISKU_CONFIG", previousConfig.empty() ? nullptr : previousConfig.c_str());
            SetEnvironmentVariableW(L"MISKU_THEME", previousTheme.empty() ? nullptr : previousTheme.c_str());
            SetEnvironmentVariableW(L"USERPROFILE", previousUser.c_str());
            SetEnvironmentVariableW(L"LOCALAPPDATA", previousLocal.c_str());
            std::error_code ignored;
            std::filesystem::remove_all(root, ignored);
        });
        SetEnvironmentVariableW(L"MISKU_CONFIG", config.c_str());
        SetEnvironmentVariableW(L"MISKU_THEME", nullptr);
        SetEnvironmentVariableW(L"USERPROFILE", root.c_str());
        SetEnvironmentVariableW(L"LOCALAPPDATA", root.c_str());

        constexpr std::string_view base{ R"({"profiles":{"defaults":{"fontSize":12}}})" };
        winrt::hstring error;
        // An absent optional file clears any prior in-process cache.
        VERIFY_ARE_EQUAL(std::string{ base }, ApplyMiskuConfigOverlay(base, error));
        til::io::write_utf8_string_to_file(config, "font-size = 18");
        const auto valid = ApplyMiskuConfigOverlay(base, error);
        VERIFY_IS_TRUE(error.empty());
        VERIFY_ARE_EQUAL(18.0, VerifyParseSucceeded(valid)["profiles"]["defaults"]["fontSize"].asDouble());

        til::io::write_utf8_string_to_file(config, "font-size = invalid");
        VERIFY_ARE_EQUAL(valid, ApplyMiskuConfigOverlay(base, error));
        VERIFY_IS_FALSE(error.empty());

        til::io::write_utf8_string_to_file(config, "font-size = 16");
        VERIFY_ARE_EQUAL(16.0, VerifyParseSucceeded(ApplyMiskuConfigOverlay(base, error))["profiles"]["defaults"]["fontSize"].asDouble());
        VERIFY_IS_TRUE(error.empty());

        // A partial config must not erase other defaults or legacy profile lists.
        til::io::write_utf8_string_to_file(config, "working-directory = C:/");
        const auto partial = VerifyParseSucceeded(ApplyMiskuConfigOverlay(base, error));
        VERIFY_ARE_EQUAL(12, partial["profiles"]["defaults"]["fontSize"].asInt());
        const auto legacy = VerifyParseSucceeded(ApplyMiskuConfigOverlay(R"({"profiles":[{"name":"keep-me"}]})", error));
        VERIFY_ARE_EQUAL(std::string{ "keep-me" }, legacy["profiles"]["list"][0]["name"].asString());

        std::filesystem::remove(config);
        VERIFY_ARE_EQUAL(std::string{ base }, ApplyMiskuConfigOverlay(base, error));
        VERIFY_IS_TRUE(error.empty());
    }
}

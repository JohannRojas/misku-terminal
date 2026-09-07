// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include "pch.h"
#include "MiskuConfig.h"
#include "FileUtils.h"
#include <array>
#include <cctype>
#include <cmath>
#include <mutex>
#include <sstream>
#include <til/io.h>

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    static constexpr std::wstring_view MiskuConfigEnvVar{ L"MISKU_CONFIG" };
    static constexpr std::wstring_view MiskuThemeEnvVar{ L"MISKU_THEME" };

    static std::string_view miskuTrim(std::string_view value)
    {
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        {
            value.remove_prefix(1);
        }
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        {
            value.remove_suffix(1);
        }
        return value;
    }

    static std::string miskuUnquote(std::string_view value)
    {
        value = miskuTrim(value);
        if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\'')))
        {
            value.remove_prefix(1);
            value.remove_suffix(1);
        }
        return std::string{ value };
    }

    static std::string miskuLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return value;
    }

    static bool miskuParseBool(std::string_view value, bool& result)
    {
        const auto lowered = miskuLower(miskuUnquote(value));
        if (lowered == "true" || lowered == "1" || lowered == "yes" || lowered == "on")
        {
            result = true;
            return true;
        }
        if (lowered == "false" || lowered == "0" || lowered == "no" || lowered == "off")
        {
            result = false;
            return true;
        }
        return false;
    }

    static bool miskuParseInt(std::string_view value, int& result)
    {
        try
        {
            size_t parsed = 0;
            const auto text = miskuUnquote(value);
            const auto number = std::stoi(text, &parsed);
            if (parsed == text.size())
            {
                result = number;
                return true;
            }
        }
        CATCH_LOG()
        return false;
    }

    static bool miskuParseDouble(std::string_view value, double& result)
    {
        try
        {
            size_t parsed = 0;
            const auto text = miskuUnquote(value);
            const auto number = std::stod(text, &parsed);
            if (parsed == text.size())
            {
                result = number;
                return true;
            }
        }
        CATCH_LOG()
        return false;
    }

    static Json::Value miskuKeybindingFromAction(std::string_view keys, std::string_view action)
    {
        static constexpr std::array mappings{
            std::pair{ std::string_view{ "new_tab" }, std::string_view{ "Terminal.OpenNewTab" } },
            std::pair{ std::string_view{ "new_session" }, std::string_view{ "Terminal.CreateSession" } },
            std::pair{ std::string_view{ "toggle_sidebar" }, std::string_view{ "Terminal.ToggleSessionSidebar" } },
            std::pair{ std::string_view{ "close_tab" }, std::string_view{ "Terminal.CloseTab" } },
            std::pair{ std::string_view{ "split_right" }, std::string_view{ "Terminal.SplitPaneRight" } },
            std::pair{ std::string_view{ "split_down" }, std::string_view{ "Terminal.SplitPaneDown" } },
            std::pair{ std::string_view{ "open_config" }, std::string_view{ "Terminal.OpenSettingsFile" } },
            std::pair{ std::string_view{ "reload_config" }, std::string_view{ "Terminal.ReloadSettings" } },
            std::pair{ std::string_view{ "copy" }, std::string_view{ "Terminal.CopyToClipboard" } },
            std::pair{ std::string_view{ "paste" }, std::string_view{ "Terminal.PasteFromClipboard" } },
            std::pair{ std::string_view{ "toggle_fullscreen" }, std::string_view{ "Terminal.ToggleFullscreen" } },
        };

        Json::Value binding{ Json::ValueType::objectValue };
        binding["keys"] = miskuUnquote(keys);

        const auto normalizedAction = miskuLower(miskuUnquote(action));
        for (const auto& [name, id] : mappings)
        {
            if (normalizedAction == name)
            {
                binding["id"] = std::string{ id };
                return binding;
            }
        }

        // Advanced escape hatch: allow binding directly to an existing Terminal.* command id.
        binding["id"] = miskuUnquote(action);
        return binding;
    }

    static Json::Value miskuThemeJson(const std::string& name, std::optional<bool> useMica)
    {
        Json::Value theme{ Json::ValueType::objectValue };
        theme["name"] = name;
        theme["window"]["applicationTheme"] = "dark";
        if (useMica.has_value())
        {
            theme["window"]["useMica"] = *useMica;
        }
        theme["tab"]["background"] = "terminalBackground";
        theme["tab"]["unfocusedBackground"] = "#00000000";
        theme["tabRow"]["unfocusedBackground"] = "#101214FF";
        return theme;
    }

    static std::optional<std::filesystem::path> miskuConfigPathFromEnvironment()
    {
        auto size = GetEnvironmentVariableW(MiskuConfigEnvVar.data(), nullptr, 0);
        if (size == 0)
        {
            return std::nullopt;
        }

        std::wstring buffer(size, L'\0');
        size = GetEnvironmentVariableW(MiskuConfigEnvVar.data(), buffer.data(), size);
        if (size == 0)
        {
            return std::nullopt;
        }
        buffer.resize(size);
        return std::filesystem::path{ buffer };
    }

    static std::optional<std::string> miskuThemeFromEnvironment()
    {
        auto size = GetEnvironmentVariableW(MiskuThemeEnvVar.data(), nullptr, 0);
        if (size == 0)
        {
            return std::nullopt;
        }

        std::wstring buffer(size, L'\0');
        size = GetEnvironmentVariableW(MiskuThemeEnvVar.data(), buffer.data(), size);
        if (size == 0)
        {
            return std::nullopt;
        }
        buffer.resize(size);
        return til::u16u8(buffer);
    }

    std::vector<std::filesystem::path> MiskuConfigSearchPaths()
    {
        std::vector<std::filesystem::path> paths;

        if (auto envPath = miskuConfigPathFromEnvironment())
        {
            paths.emplace_back(std::move(*envPath));
        }

        paths.emplace_back(wil::ExpandEnvironmentStringsW<std::wstring>(LR"(%USERPROFILE%\.config\misku\config.misku)"));
        paths.emplace_back(wil::ExpandEnvironmentStringsW<std::wstring>(LR"(%LOCALAPPDATA%\MiskuTerminal\config.misku)"));
        return paths;
    }

    MiskuConfigOverlay ParseMiskuConfig(const std::filesystem::path& path, const std::string_view content)
    {
        Json::Value root{ Json::ValueType::objectValue };
        Json::Value& profileDefaults = root["profiles"]["defaults"];
        profileDefaults = Json::Value{ Json::ValueType::objectValue };
        std::optional<bool> useMica;
        std::string themeName = "Misku Dark";
        bool themeWasSet = false;
        bool themeNeedsDefinition = false;

        if (const auto envTheme = miskuThemeFromEnvironment())
        {
            themeName = *envTheme;
            root["theme"] = themeName;
            themeWasSet = true;
        }

        std::istringstream stream{ std::string{ content } };
        std::string line;
        size_t lineNumber = 0;
        while (std::getline(stream, line))
        {
            ++lineNumber;
            auto view = miskuTrim(line);
            if (view.empty() || view.front() == '#' || (view.size() >= 2 && view.substr(0, 2) == "//"))
            {
                continue;
            }

            const auto separator = view.find('=');
            if (separator == std::string_view::npos)
            {
                return { std::nullopt, fmt::format(FMT_COMPILE(L"{}:{}: expected key = value"), path.native(), lineNumber) };
            }

            const auto key = miskuLower(miskuUnquote(view.substr(0, separator)));
            const auto value = view.substr(separator + 1);

            if (key == "font-family")
            {
                profileDefaults["fontFace"] = miskuUnquote(value);
            }
            else if (key == "font-size")
            {
                double fontSize = 0;
                if (!miskuParseDouble(value, fontSize) || !std::isfinite(fontSize) || fontSize <= 0)
                {
                    return { std::nullopt, fmt::format(FMT_COMPILE(L"{}:{}: invalid font-size"), path.native(), lineNumber) };
                }
                profileDefaults["fontSize"] = fontSize;
            }
            else if (key == "theme")
            {
                themeName = miskuUnquote(value);
                root["theme"] = themeName;
                themeWasSet = true;
            }
            else if (key == "background" || key == "foreground")
            {
                const auto color = miskuUnquote(value);
                if ((color.size() != 4 && color.size() != 7) || color.front() != '#' ||
                    !std::all_of(color.begin() + 1, color.end(), [](unsigned char ch) { return std::isxdigit(ch); }))
                {
                    return { std::nullopt, fmt::format(FMT_COMPILE(L"{}:{}: invalid color (expected #rgb or #rrggbb)"), path.native(), lineNumber) };
                }
                profileDefaults[key] = color;
            }
            else if (key == "opacity")
            {
                double opacity = 0;
                if (!miskuParseDouble(value, opacity) || !std::isfinite(opacity) || opacity < 0 || opacity > 100)
                {
                    return { std::nullopt, fmt::format(FMT_COMPILE(L"{}:{}: invalid opacity"), path.native(), lineNumber) };
                }
                profileDefaults["opacity"] = opacity <= 1.0 ? static_cast<int>(opacity * 100.0 + 0.5) : static_cast<int>(opacity + 0.5);
            }
            else if (key == "mica")
            {
                bool mica = false;
                if (!miskuParseBool(value, mica))
                {
                    return { std::nullopt, fmt::format(FMT_COMPILE(L"{}:{}: invalid mica value"), path.native(), lineNumber) };
                }
                useMica = mica;
                root["theme"] = themeName;
                themeNeedsDefinition = true;
            }
            else if (key == "default-profile")
            {
                root["defaultProfile"] = miskuUnquote(value);
            }
            else if (key == "working-directory")
            {
                profileDefaults["startingDirectory"] = miskuUnquote(value);
            }
            else if (key == "scrollback-lines")
            {
                int historySize = 0;
                if (!miskuParseInt(value, historySize) || historySize < 0)
                {
                    return { std::nullopt, fmt::format(FMT_COMPILE(L"{}:{}: invalid scrollback-lines"), path.native(), lineNumber) };
                }
                profileDefaults["historySize"] = historySize;
            }
            else if (key == "cursor-style")
            {
                const auto cursor = miskuUnquote(value);
                if (cursor != "bar" && cursor != "vintage" && cursor != "underscore" &&
                    cursor != "filledBox" && cursor != "emptyBox" && cursor != "doubleUnderscore")
                {
                    return { std::nullopt, fmt::format(FMT_COMPILE(L"{}:{}: invalid cursor-style"), path.native(), lineNumber) };
                }
                profileDefaults["cursorShape"] = cursor;
            }
            else if (key == "keybind")
            {
                const auto binding = miskuTrim(value);
                const auto bindingSeparator = binding.find('=');
                if (bindingSeparator == std::string_view::npos)
                {
                    return { std::nullopt, fmt::format(FMT_COMPILE(L"{}:{}: invalid keybind, expected keybind = keys=action"), path.native(), lineNumber) };
                }
                root["keybindings"].append(miskuKeybindingFromAction(binding.substr(0, bindingSeparator), binding.substr(bindingSeparator + 1)));
            }
            else
            {
                return { std::nullopt, fmt::format(FMT_COMPILE(L"{}:{}: unknown Misku config key '{}'"), path.native(), lineNumber, til::u8u16(key)) };
            }
        }

        // A command-line theme takes precedence regardless of file key order.
        if (const auto envTheme = miskuThemeFromEnvironment())
        {
            themeName = *envTheme;
            root["theme"] = themeName;
        }
        if (themeWasSet || themeNeedsDefinition)
        {
            root["themes"].append(miskuThemeJson(themeName, useMica));
        }

        Json::StreamWriterBuilder writer;
        writer.settings_["enableYAMLCompatibility"] = true;
        writer.settings_["indentation"] = "";
        return { Json::writeString(writer, root), std::nullopt };
    }

    static MiskuConfigOverlay loadMiskuConfigOverlay()
    {
        for (const auto& path : MiskuConfigSearchPaths())
        {
            try
            {
                if (!std::filesystem::exists(path))
                {
                    continue;
                }
                const auto content = til::io::read_file_as_utf8_string_if_exists(path);
                return ParseMiskuConfig(path, content);
            }
            catch (const winrt::hresult_error& e)
            {
                return { std::nullopt, fmt::format(FMT_COMPILE(L"{}: {}"), path.native(), e.message()) };
            }
            catch (const std::exception& e)
            {
                return { std::nullopt, fmt::format(FMT_COMPILE(L"{}: {}"), path.native(), til::u8u16(e.what())) };
            }
        }

        if (miskuThemeFromEnvironment())
        {
            return ParseMiskuConfig({}, {});
        }

        return {};
    }

    static void miskuWriteConfigError(const std::wstring& error) noexcept
    {
        try
        {
            const std::filesystem::path logPath{ wil::ExpandEnvironmentStringsW<std::wstring>(LR"(%LOCALAPPDATA%\MiskuTerminal\config.misku.error.log)") };
            std::filesystem::create_directories(logPath.parent_path());
            til::io::write_utf8_string_to_file(logPath, til::u16u8(L"Misku Terminal ignored config.misku:\r\n" + error + L"\r\n"));
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    static Json::Value parseJsonForMiskuMerge(const std::string_view content)
    {
        Json::Value json;
        std::string errors;
        const std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder{}.newCharReader() };
        if (!reader->parse(content.data(), content.data() + content.size(), &json, &errors))
        {
            throw winrt::hresult_error(WEB_E_INVALID_JSON_STRING, winrt::to_hstring(errors));
        }
        return json;
    }

    static void mergeMiskuJson(Json::Value& target, const Json::Value& overlay)
    {
        if (!target.isObject() || !overlay.isObject())
        {
            target = overlay;
            return;
        }

        for (const auto& key : overlay.getMemberNames())
        {
            const auto& overlayValue = overlay[key];
            auto& targetValue = target[key];
            if (targetValue.isObject() && overlayValue.isObject())
            {
                mergeMiskuJson(targetValue, overlayValue);
            }
            else if (targetValue.isArray() && overlayValue.isArray())
            {
                for (const auto& item : overlayValue)
                {
                    targetValue.append(item);
                }
            }
            else
            {
                targetValue = overlayValue;
            }
        }
    }

    static std::optional<std::string> tryMergeMiskuConfigOverlay(std::string_view userSettings, const std::string& overlay)
    {
        try
        {
            auto baseJson = parseJsonForMiskuMerge(userSettings.empty() ? std::string_view{ "{}" } : userSettings);
            auto overlayJson = parseJsonForMiskuMerge(overlay);
            if (baseJson["profiles"].isArray())
            {
                // Older settings use a profile array; preserve it when adding defaults.
                auto profiles = std::move(baseJson["profiles"]);
                baseJson["profiles"] = Json::Value{ Json::ValueType::objectValue };
                baseJson["profiles"]["list"] = std::move(profiles);
            }
            mergeMiskuJson(baseJson, overlayJson);

            Json::StreamWriterBuilder writer;
            writer.settings_["enableYAMLCompatibility"] = true;
            writer.settings_["indentation"] = "";
            return Json::writeString(writer, baseJson);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return std::nullopt;
        }
    }

    std::string ApplyMiskuConfigOverlay(std::string_view userSettings, winrt::hstring& error)
    {
        static std::mutex mutex;
        const std::scoped_lock lock{ mutex };
        static std::optional<std::string> lastValidOverlay;
        error = {};

        auto overlay = loadMiskuConfigOverlay();
        if (!overlay.json.has_value())
        {
            if (!overlay.error.has_value())
            {
                // Removing the optional config disables it, instead of keeping a stale override.
                lastValidOverlay.reset();
                return std::string{ userSettings };
            }
            if (overlay.error.has_value())
            {
                error = *overlay.error;
                OutputDebugStringW((L"Misku Terminal ignored config.misku: " + *overlay.error + L"\n").c_str());
                miskuWriteConfigError(*overlay.error);
            }

            if (lastValidOverlay.has_value())
            {
                if (auto merged = tryMergeMiskuConfigOverlay(userSettings, *lastValidOverlay))
                {
                    return *merged;
                }
            }

            return std::string{ userSettings };
        }

        if (auto merged = tryMergeMiskuConfigOverlay(userSettings, *overlay.json))
        {
            lastValidOverlay = *overlay.json;
            return *merged;
        }

        if (lastValidOverlay.has_value())
        {
            if (auto merged = tryMergeMiskuConfigOverlay(userSettings, *lastValidOverlay))
            {
                return *merged;
            }
        }

        return std::string{ userSettings };
    }

}

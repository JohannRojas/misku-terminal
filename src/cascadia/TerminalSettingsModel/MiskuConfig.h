// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct MiskuConfigOverlay
    {
        std::optional<std::string> json;
        std::optional<std::wstring> error;
    };

    MiskuConfigOverlay ParseMiskuConfig(const std::filesystem::path& path, std::string_view content);
    std::vector<std::filesystem::path> MiskuConfigSearchPaths();
    std::string ApplyMiskuConfigOverlay(std::string_view userSettings, winrt::hstring& error);
}

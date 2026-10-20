// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once

#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Misku::Updates
{
    inline constexpr std::string_view Repository = "https://github.com/JohannRojas/misku-terminal";
    inline constexpr int64_t CheckIntervalSeconds = 24 * 60 * 60;
    inline constexpr uint64_t MaximumPackageBytes = 512ull * 1024 * 1024;

    struct Version
    {
        std::array<uint16_t, 4> Parts{};

        static std::optional<Version> Parse(std::string_view text) noexcept
        {
            Version result;
            for (size_t i = 0; i < result.Parts.size(); ++i)
            {
                const auto end = text.find('.');
                const auto part = text.substr(0, end);
                unsigned value{};
                const auto parsed = std::from_chars(part.data(), part.data() + part.size(), value);
                if (part.empty() || parsed.ec != std::errc{} || parsed.ptr != part.data() + part.size() || value > 65535 ||
                    (part.size() > 1 && part.front() == '0') || ((i == 3) != (end == std::string_view::npos)))
                {
                    return std::nullopt;
                }
                result.Parts[i] = static_cast<uint16_t>(value);
                if (i != 3) { text.remove_prefix(end + 1); }
            }
            return result;
        }

        uint64_t Number() const noexcept
        {
            return (uint64_t{ Parts[0] } << 48) | (uint64_t{ Parts[1] } << 32) | (uint64_t{ Parts[2] } << 16) | Parts[3];
        }

        std::string Display() const
        {
            return std::to_string(Parts[0]) + "." + std::to_string(Parts[1]) + "." + std::to_string(Parts[2]);
        }

        std::string String() const { return Display() + "." + std::to_string(Parts[3]); }
    };

    struct Release
    {
        Version PackageVersion;
        std::string PackageUri;
        std::string Sha256;
        bool Preview{};

        std::string NotesUri() const { return std::string{ Repository } + "/releases/tag/misku-v" + PackageVersion.Display(); }

        bool Valid() const
        {
            if (PackageVersion.Parts[3] != 0 || Sha256.size() != 64) { return false; }
            for (const auto ch : Sha256)
            {
                if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) { return false; }
            }
            const auto version = PackageVersion.Display();
            return PackageUri == std::string{ Repository } + "/releases/download/misku-v" + version + "/MiskuTerminal-" + version + "-x64.msix";
        }
    };

    inline bool CheckDue(const int64_t now, const int64_t lastCheck) noexcept
    {
        return lastCheck <= 0 || now < lastCheck || now - lastCheck >= CheckIntervalSeconds;
    }

    enum class Status { Unsupported, Idle, Checking, NoUpdates, Available, Downloading, Ready, Error };

    struct Snapshot
    {
        Status State{ Status::Unsupported };
        std::optional<Release> Update;
        bool Automatic{ true };
        bool Preview{};
        bool Dismissed{};
        bool Manual{};
        uint32_t Progress{};
        int32_t ErrorCode{};
    };

    inline bool ShowNotice(const Snapshot& state) noexcept
    {
        return !state.Dismissed && (state.State == Status::Available || state.State == Status::Downloading ||
                                   state.State == Status::Ready || (state.State == Status::Error && state.Manual));
    }
}

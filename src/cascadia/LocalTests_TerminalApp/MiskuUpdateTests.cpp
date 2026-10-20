// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include "pch.h"
#include <WexTestClass.h>
#include "../TerminalApp/MiskuUpdateService.h"
#include <winrt/Windows.Data.Json.h>

using namespace WEX::Common;
using namespace WEX::TestExecution;
using namespace Misku::Updates;
using namespace winrt::Windows::Data::Json;

namespace TerminalAppLocalTests
{
    class MiskuUpdateTests
    {
        TEST_CLASS(MiskuUpdateTests);

        static JsonObject Feed()
        {
            return JsonObject::Parse(LR"({"schemaVersion":1,"packageName":"MiskuTerminal","publisher":"CN=Misku Terminal","architecture":"x64","version":"0.3.10.0","preview":false,"packageUri":"https://github.com/JohannRojas/misku-terminal/releases/download/misku-v0.3.10/MiskuTerminal-0.3.10-x64.msix","sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"})");
        }

        TEST_METHOD(CanonicalVersion)
        {
            const auto version = Version::Parse("1.2.3.0");
            VERIFY_IS_TRUE(version.has_value());
            VERIFY_ARE_EQUAL(uint64_t{ 0x0001000200030000 }, version->Number());
            VERIFY_IS_TRUE(version->Display() == "1.2.3");
        }

        TEST_METHOD(RejectMalformedVersions)
        {
            for (const auto value : { "", "1", "1.2.3", "1.2.3.4.5", "01.2.3.0", "1.-1.3.0", "1.2.3.65536", "65536.0.0.0", "1.2.3.0x", " 1.2.3.0", "1.2..0", "1.2.3.+0" })
            {
                VERIFY_IS_FALSE(Version::Parse(value).has_value());
            }
        }

        TEST_METHOD(NumericVersionOrdering)
        {
            VERIFY_IS_TRUE(Version::Parse("0.3.10.0")->Number() > Version::Parse("0.3.9.0")->Number());
            VERIFY_IS_TRUE(Version::Parse("0.3.0.0")->Number() > Version::Parse("0.2.65535.0")->Number());
            VERIFY_IS_TRUE(Version::Parse("65535.65535.65535.65535")->Number() == UINT64_MAX);
        }

        TEST_METHOD(ValidFeed)
        {
            const auto update = Service::ParseFeed(Feed().Stringify(), false);
            VERIFY_IS_TRUE(update.has_value());
            VERIFY_IS_TRUE(update->PackageVersion.String() == "0.3.10.0");
            VERIFY_IS_TRUE(update->NotesUri() == "https://github.com/JohannRojas/misku-terminal/releases/tag/misku-v0.3.10");
        }

        TEST_METHOD(ChannelIsolation)
        {
            auto feed = Feed();
            VERIFY_IS_FALSE(Service::ParseFeed(feed.Stringify(), true).has_value());
            feed.SetNamedValue(L"preview", JsonValue::CreateBooleanValue(true));
            VERIFY_IS_TRUE(Service::ParseFeed(feed.Stringify(), true).has_value());
            VERIFY_IS_FALSE(Service::ParseFeed(feed.Stringify(), false).has_value());
        }

        TEST_METHOD(RejectOtherPackageIdentities)
        {
            for (const auto key : { L"packageName", L"publisher", L"architecture" })
            {
                auto feed = Feed();
                feed.SetNamedValue(key, JsonValue::CreateStringValue(L"Other"));
                VERIFY_IS_FALSE(Service::ParseFeed(feed.Stringify(), false).has_value());
            }
        }

        TEST_METHOD(RejectUnexpectedSchema)
        {
            auto feed = Feed();
            feed.SetNamedValue(L"schemaVersion", JsonValue::CreateNumberValue(2));
            VERIFY_IS_FALSE(Service::ParseFeed(feed.Stringify(), false).has_value());
            feed.SetNamedValue(L"schemaVersion", JsonValue::CreateStringValue(L"1"));
            VERIFY_IS_FALSE(Service::ParseFeed(feed.Stringify(), false).has_value());
        }

        TEST_METHOD(RejectUntrustedDownloadLocations)
        {
            for (const auto url : { L"http://github.com/JohannRojas/misku-terminal/releases/download/misku-v0.3.10/MiskuTerminal-0.3.10-x64.msix",
                                    L"https://github.com/attacker/misku-terminal/releases/download/misku-v0.3.10/MiskuTerminal-0.3.10-x64.msix",
                                    L"https://github.com/JohannRojas/misku-terminal/releases/download/misku-v0.3.9/MiskuTerminal-0.3.9-x64.msix",
                                    L"file:///C:/another-app.msix", L"https://example.com/update.msix" })
            {
                auto feed = Feed();
                feed.SetNamedValue(L"packageUri", JsonValue::CreateStringValue(url));
                VERIFY_IS_FALSE(Service::ParseFeed(feed.Stringify(), false).has_value());
            }
        }

        TEST_METHOD(RequireSha256)
        {
            for (const auto value : { L"", L"1234", L"gggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg" })
            {
                auto feed = Feed();
                feed.SetNamedValue(L"sha256", JsonValue::CreateStringValue(value));
                VERIFY_IS_FALSE(Service::ParseFeed(feed.Stringify(), false).has_value());
            }
        }

        TEST_METHOD(RejectMalformedOrOversizedFeed)
        {
            VERIFY_IS_FALSE(Service::ParseFeed(L"{", false).has_value());
            VERIFY_IS_FALSE(Service::ParseFeed(L"{}", false).has_value());
            VERIFY_IS_FALSE(Service::ParseFeed(winrt::hstring{ std::wstring(17000, L' ') }, false).has_value());
        }

        TEST_METHOD(ThrottleAndClockChanges)
        {
            VERIFY_IS_TRUE(CheckDue(100000, 0));
            VERIFY_IS_FALSE(CheckDue(100000, 99999));
            VERIFY_IS_FALSE(CheckDue(186399, 100000));
            VERIFY_IS_TRUE(CheckDue(186400, 100000));
            VERIFY_IS_TRUE(CheckDue(90000, 100000));
        }

        TEST_METHOD(BackgroundChecksStayQuiet)
        {
            Snapshot state;
            for (const auto status : { Status::Unsupported, Status::Idle, Status::Checking, Status::NoUpdates, Status::Error })
            {
                state.State = status;
                VERIFY_IS_FALSE(ShowNotice(state));
            }
            state.Manual = true;
            VERIFY_IS_TRUE(ShowNotice(state));
        }

        TEST_METHOD(DismissedAndPreparedUpdates)
        {
            Snapshot state;
            for (const auto status : { Status::Available, Status::Downloading, Status::Ready })
            {
                state.State = status;
                state.Dismissed = false;
                VERIFY_IS_TRUE(ShowNotice(state));
                state.Dismissed = true;
                VERIFY_IS_FALSE(ShowNotice(state));
            }
        }
    };
}

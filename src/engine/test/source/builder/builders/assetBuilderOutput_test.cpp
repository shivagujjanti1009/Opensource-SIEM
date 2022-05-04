/* Copyright (C) 2015-2021, Wazuh Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include "testUtils.hpp"
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "assetBuilderOutput.hpp"
#include "combinatorBuilderBroadcast.hpp"
#include "combinatorBuilderChain.hpp"
#include "opBuilderCondition.hpp"
#include "opBuilderFileOutput.hpp"
#include "opBuilderHelperFilter.hpp"
#include "stageBuilderCheck.hpp"
#include "stageBuilderOutputs.hpp"

using namespace builder::internals::builders;

TEST(AssetBuilderOutput, BuildsAllNonRegistered)
{
    Document doc {R"({
        "name": "test",
        "check": [
            {"field": "value"}
        ],
        "outputs": [
            {"file":
                {"path": "/tmp/stageOutputsTest1.txt"}
            },
            {"file":
                {"path": "/tmp/stageOutputsTest2.txt"}
            }
        ]
    })"};

    ASSERT_THROW(builders::assetBuilderOutput(doc),
                 std::_Nested_exception<std::runtime_error>);
}

TEST(AssetBuilderOutput, Builds)
{
    BuilderVariant c = opBuilderHelperExists;
    Registry::registerBuilder("helper.exists", c);
    c = opBuilderHelperNotExists;
    Registry::registerBuilder("helper.not_exists", c);
    c = middleBuilderCondition;
    Registry::registerBuilder("middle.condition", c);
    c = opBuilderCondition;
    Registry::registerBuilder("condition", c);
    c = combinatorBuilderChain;
    Registry::registerBuilder("combinator.chain", c);
    c = opBuilderFileOutput;
    Registry::registerBuilder("file", c);
    c = combinatorBuilderBroadcast;
    Registry::registerBuilder("combinator.broadcast", c);
    c = stageBuilderOutputs;
    Registry::registerBuilder("outputs", c);
    c = stageBuilderCheck;
    Registry::registerBuilder("check", c);

    Document doc {R"({
        "name": "test",
        "check": [
            {"field": "value"}
        ],
        "outputs": [
            {"file":
                {"path": "/tmp/stageOutputsTest1.txt"}
            },
            {"file":
                {"path": "/tmp/stageOutputsTest2.txt"}
            }
        ]
    })"};

    ASSERT_NO_THROW(builders::assetBuilderOutput(doc));
}

TEST(AssetBuilderOutput, BuildsOperates)
{
    Document doc {R"({
        "name": "test",
        "check": [
            {"field": "value"}
        ],
        "outputs": [
            {"file":
                {"path": "/tmp/stageOutputsTest1.txt"}
            },
            {"file":
                {"path": "/tmp/stageOutputsTest2.txt"}
            }
        ]
    })"};

    auto input = observable<>::create<Event>(
                     [=](auto s)
                     {
                         s.on_next(std::make_shared<json::Document>(R"(
                {"field":"value"}
            )"));
                         s.on_next(std::make_shared<json::Document>(R"(
                {"field1":"value"}
            )"));
                         s.on_next(std::make_shared<json::Document>(R"(
                {"field":"value"}
            )"));
                         s.on_next(std::make_shared<json::Document>(R"(
                {"field":"value1"}
            )"));
                         s.on_next(std::make_shared<json::Document>(R"(
                {"field":"value"}
            )"));
                         s.on_next(std::make_shared<json::Document>(R"(
                {"field":"value"}
            )"));
                         s.on_completed();
                     })
                     .publish();
    ConnectableT conn = assetBuilderOutput(doc);
    Observable output = conn.connect(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    input.connect();
    ASSERT_EQ(expected.size(), 8);

    const string expectedWrite =
        R"({"field":"value"}
{"field":"value"}
{"field":"value"}
{"field":"value"}
)";

    string filepath1 {"/tmp/stageOutputsTest1.txt"};
    string filepath2 {"/tmp/stageOutputsTest2.txt"};
    std::ifstream ifs(filepath1);
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    const string got1 = buffer.str();

    ifs.open(filepath2);
    buffer << ifs.rdbuf();
    const string got2 = buffer.str();
    // cerr << got << std::endl;
    // cerr << expectedWrite << std::endl;
    std::filesystem::remove(filepath1);
    std::filesystem::remove(filepath2);

    ASSERT_EQ(got1, expectedWrite);
    ASSERT_EQ(got2, expectedWrite);
}

/* Copyright (C) 2015-2022, Wazuh Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */


#include <gtest/gtest.h>
#include <testUtils.hpp>
#include <vector>

#include "OpBuilderHelperFilter.hpp"

using namespace builder::internals::builders;

// Build ok
TEST(opBuilderHelperString_eq, Builds)
{
    Document doc{R"({
        "check":
            {"field2check": "+s_eq/test_value"}
    })"};
    ASSERT_NO_THROW(opBuilderHelperString_eq(*doc.get("/check")));
}

// Build incorrect number of arguments
TEST(opBuilderHelperString_eq, BuildsIncorrectNumberOfArguments)
{
    Document doc{R"({
        "check":
            {"field2check": "+s_eq/test_value/test_value2"}
    })"};
    ASSERT_THROW(opBuilderHelperString_eq(*doc.get("/check")), std::runtime_error);
}

// Test ok: static values
TEST(opBuilderHelperString_eq, staticStringOk)
{
    Document doc{R"({
        "check":
            {"field2check": "+s_eq/test_value"}
    })"};

    Observable input = observable<>::create<Event>(
        [=](auto s)
        {
            s.on_next(Event{R"(
                {"field2check":"not_test_value"}
            )"});
            s.on_next(Event{R"(
                {"field2check":"test_value"}
            )"});
            s.on_next(Event{R"(
                {"otherfield":"value"}
            )"});
            s.on_next(Event{R"(
                {"otherfield":"test_value"}
            )"});
            s.on_next(Event{R"(
                {"field2check":"test_value"}
            )"});
            s.on_completed();
        });

    Lifter lift = opBuilderHelperString_eq(*doc.get("/check"));
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 2);
    ASSERT_STREQ(expected[0].get("/field2check")->GetString(), "test_value");
    ASSERT_STREQ(expected[1].get("/field2check")->GetString(), "test_value");
}

// Test ok: static values (numbers, compare as string)
TEST(opBuilderHelperString_eq, staticNumberOk)
{
    Document doc{R"({
        "check":
            {"field2check": "+s_eq/11"}
    })"};

    Observable input = observable<>::create<Event>(
        [=](auto s)
        {
            s.on_next(Event{R"(
                {"field2check":"not_11"}
            )"});
            s.on_next(Event{R"(
                {"field2check":"11"}
            )"});
            s.on_next(Event{R"(
                {"otherfield":"11"}
            )"});
            s.on_next(Event{R"(
                {"otherfield":11}
            )"});
            s.on_next(Event{R"(
                {"field2check":"11"}
            )"});
            s.on_completed();
        });

    Lifter lift = opBuilderHelperString_eq(*doc.get("/check"));
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 2);
    ASSERT_STREQ(expected[0].get("/field2check")->GetString(), "11");
    ASSERT_STREQ(expected[1].get("/field2check")->GetString(), "11");
}

// Test ok: dynamic values (string)
TEST(opBuilderHelperString_eq, dynamicsStringOk) {
    Document doc{R"({
        "check":
            {"field2check": "+s_eq/$ref_key"}
    })"};

    Observable input = observable<>::create<Event>(
        [=](auto s)
        {
            s.on_next(Event{R"(
                {
                    "field2check":"not_test_value",
                    "ref_key":"test_value"
                }
            )"});
            s.on_next(Event{R"(
                {
                    "field2check":"test_value",
                    "ref_key":"test_value"
                }
            )"});
            s.on_next(Event{R"(
                {
                    "otherfield":"value",
                    "ref_key":"test_value"
                }
            )"});
            s.on_next(Event{R"(
                {
                    "otherfield":"test_value",
                    "ref_key":"test_value"
                }
            )"});
            s.on_next(Event{R"(
                {
                    "field2check":"test_value",
                    "ref_key":"test_value"
                }
            )"});
            s.on_completed();
        });

    Lifter lift = opBuilderHelperString_eq(*doc.get("/check"));
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 2);
    ASSERT_STREQ(expected[0].get("/field2check")->GetString(), "test_value");
    ASSERT_STREQ(expected[1].get("/field2check")->GetString(), "test_value");
}

// Test ok: dynamic values (number)
TEST(opBuilderHelperString_eq, dynamicsNumberOk) {
    Document doc{R"({
        "check":
            {"field2check": "+s_eq/$ref_key"}
    })"};

    Observable input = observable<>::create<Event>(
        [=](auto s)
        {
            s.on_next(Event{R"(
                {
                    "field2check":11,
                    "ref_key":11
                }
            )"});
            s.on_next(Event{R"(
                {
                    "field2check":"11",
                    "ref_key":11
                }
            )"});
            s.on_next(Event{R"(
                {
                    "otherfield":11,
                    "ref_key":11
                }
            )"});
            s.on_next(Event{R"(
                {
                    "otherfield":11,
                    "ref_key":11
                }
            )"});
            s.on_next(Event{R"(
                {
                    "field2check":11,
                    "ref_key":"11"
                }
            )"});
            s.on_next(Event{R"(
                {
                    "field2check":"11",
                    "not_ref_key":"11"
                }
            )"});
            s.on_completed();
        });

    Lifter lift = opBuilderHelperString_eq(*doc.get("/check"));
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 0);
}




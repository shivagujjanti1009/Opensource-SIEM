#include <gtest/gtest.h>

#include "hlp_test.hpp"

auto constexpr NAME = "textParser";
auto constexpr TARGET = "TargetField";

INSTANTIATE_TEST_SUITE_P(ByteBuild,
                         HlpBuildTest,
                         ::testing::Values(BuildT(SUCCESS, getByteParser, {NAME, TARGET, {}, {}}),
                                           BuildT(FAILURE, getByteParser, {NAME, TARGET, {}, {"unexpected"}})));

INSTANTIATE_TEST_SUITE_P(
    ByteParse,
    HlpParseTest,
    ::testing::Values(
        ParseT(SUCCESS, "42", j(fmt::format(R"({{"{}": 42}})", TARGET)), 2, getByteParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "-42", j(fmt::format(R"({{"{}": -42}})", TARGET)), 3, getByteParser, {NAME, TARGET, {}, {}}),
        ParseT(FAILURE, "128", {}, 3, getByteParser, {NAME, TARGET, {}, {}}),
        ParseT(FAILURE, "-129", {}, 4, getByteParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "42    ", j(fmt::format(R"({{"{}": 42}})", TARGET)), 2, getByteParser, {NAME, TARGET, {}, {}}),
        ParseT(
            SUCCESS, "-42    ", j(fmt::format(R"({{"{}": -42}})", TARGET)), 3, getByteParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "42#####", j(fmt::format(R"({{"{}": 42}})", TARGET)), 2, getByteParser, {NAME, TARGET, {}, {}}),
        ParseT(
            SUCCESS, "-42####", j(fmt::format(R"({{"{}": -42}})", TARGET)), 3, getByteParser, {NAME, TARGET, {}, {}})));

INSTANTIATE_TEST_SUITE_P(LongBuild,
                         HlpBuildTest,
                         ::testing::Values(BuildT(SUCCESS, getLongParser, {NAME, TARGET, {}, {}}),
                                           BuildT(FAILURE, getLongParser, {NAME, TARGET, {}, {"unexpected"}})));

INSTANTIATE_TEST_SUITE_P(
    LongParse,
    HlpParseTest,
    ::testing::Values(
        ParseT(SUCCESS, "42", j(fmt::format(R"({{"{}": 42}})", TARGET)), 2, getLongParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "-42", j(fmt::format(R"({{"{}": -42}})", TARGET)), 3, getLongParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "128", j(fmt::format(R"({{"{}": 128}})", TARGET)), 3, getLongParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "-129", j(fmt::format(R"({{"{}": -129}})", TARGET)), 4, getLongParser, {NAME, TARGET, {}, {}}),
        ParseT(FAILURE, "9223372036854775808", {}, 19, getLongParser, {NAME, TARGET, {}, {}}),
        ParseT(FAILURE, "-9223372036854775809", {}, 20, getLongParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "42    ", j(fmt::format(R"({{"{}": 42}})", TARGET)), 2, getLongParser, {NAME, TARGET, {}, {}}),
        ParseT(
            SUCCESS, "-42    ", j(fmt::format(R"({{"{}": -42}})", TARGET)), 3, getLongParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "42#####", j(fmt::format(R"({{"{}": 42}})", TARGET)), 2, getLongParser, {NAME, TARGET, {}, {}}),
        ParseT(
            SUCCESS, "-42####", j(fmt::format(R"({{"{}": -42}})", TARGET)), 3, getLongParser, {NAME, TARGET, {}, {}})));

INSTANTIATE_TEST_SUITE_P(FloatBuild,
                         HlpBuildTest,
                         ::testing::Values(BuildT(SUCCESS, getFloatParser, {NAME, TARGET, {}, {}}),
                                           BuildT(FAILURE, getFloatParser, {NAME, TARGET, {}, {"unexpected"}})));

INSTANTIATE_TEST_SUITE_P(
    FloatParse,
    HlpParseTest,
    ::testing::Values(
        ParseT(SUCCESS, "42", j(fmt::format(R"({{"{}": 42}})", TARGET)), 2, getFloatParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "42.0", j(fmt::format(R"({{"{}": 42}})", TARGET)), 4, getFloatParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "-42", j(fmt::format(R"({{"{}": -42}})", TARGET)), 3, getFloatParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "-42.0", j(fmt::format(R"({{"{}": -42}})", TARGET)), 5, getFloatParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "128", j(fmt::format(R"({{"{}": 128}})", TARGET)), 3, getFloatParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "-129", j(fmt::format(R"({{"{}": -129}})", TARGET)), 4, getFloatParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS,
               "9223372036854775808",
               j(fmt::format(R"({{"{}": 9223372036854775808}})", TARGET)),
               19,
               getFloatParser,
               {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS,
               "-9223372036854775809",
               j(fmt::format(R"({{"{}": -9223372036854775809}})", TARGET)),
               20,
               getFloatParser,
               {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS,
               "3.40282e+38",
               []()
               {
                   json::Json expected {};
                   expected.setFloat(float_t(3.40282e+38), json::Json::formatJsonPath(TARGET));
                   return expected;
               }(),
               11,
               getFloatParser,
               {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS,
               "-3.40282e+38",
               []()
               {
                   json::Json expected {};
                   expected.setFloat(float_t(-3.40282e+38), json::Json::formatJsonPath(TARGET));
                   return expected;
               }(),
               12,
               getFloatParser,
               {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "42    ", j(fmt::format(R"({{"{}": 42}})", TARGET)), 2, getFloatParser, {NAME, TARGET, {}, {}}),
        ParseT(
            SUCCESS, "-42    ", j(fmt::format(R"({{"{}": -42}})", TARGET)), 3, getFloatParser, {NAME, TARGET, {}, {}}),
        ParseT(
            SUCCESS, "42#####", j(fmt::format(R"({{"{}": 42}})", TARGET)), 2, getFloatParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS,
               "-42####",
               j(fmt::format(R"({{"{}": -42}})", TARGET)),
               3,
               getFloatParser,
               {NAME, TARGET, {}, {}})));


INSTANTIATE_TEST_SUITE_P(DoubleBuild,
                         HlpBuildTest,
                         ::testing::Values(BuildT(SUCCESS, getDoubleParser, {NAME, TARGET, {}, {}}),
                                           BuildT(FAILURE, getDoubleParser, {NAME, TARGET, {}, {"unexpected"}})));

INSTANTIATE_TEST_SUITE_P(
    DoubleParse,
    HlpParseTest,
    ::testing::Values(
        ParseT(SUCCESS, "42", j(fmt::format(R"({{"{}": 42}})", TARGET)), 2, getDoubleParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "42.0", j(fmt::format(R"({{"{}": 42}})", TARGET)), 4, getDoubleParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "-42", j(fmt::format(R"({{"{}": -42}})", TARGET)), 3, getDoubleParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "-42.0", j(fmt::format(R"({{"{}": -42}})", TARGET)), 5, getDoubleParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "128", j(fmt::format(R"({{"{}": 128}})", TARGET)), 3, getDoubleParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "-129", j(fmt::format(R"({{"{}": -129}})", TARGET)), 4, getDoubleParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS,
               "9223372036854775808",
               j(fmt::format(R"({{"{}": 9223372036854775808}})", TARGET)),
               19,
               getDoubleParser,
               {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS,
               "-9223372036854775809",
               j(fmt::format(R"({{"{}": -9223372036854775809}})", TARGET)),
               20,
               getDoubleParser,
               {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS,
               "3.40282e+38",
               []()
               {
                   json::Json expected {};
                   expected.setDouble(double_t(3.40282e+38), json::Json::formatJsonPath(TARGET));
                   return expected;
               }(),
               11,
               getDoubleParser,
               {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS,
               "-3.40282e+38",
               []()
               {
                   json::Json expected {};
                   expected.setDouble(double_t(-3.40282e+38), json::Json::formatJsonPath(TARGET));
                   return expected;
               }(),
               12,
               getDoubleParser,
               {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS,
               "1.79769e+308",
               []()
               {
                   json::Json expected {};
                   expected.setDouble(double_t(1.79769e+308), json::Json::formatJsonPath(TARGET));
                   return expected;
               }(),
               12,
               getDoubleParser,
               {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS,
               "-1.79769e+308",
               []()
               {
                   json::Json expected {};
                   expected.setDouble(double_t(-1.79769e+308), json::Json::formatJsonPath(TARGET));
                   return expected;
               }(),
               13,
               getDoubleParser,
               {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "42    ", j(fmt::format(R"({{"{}": 42}})", TARGET)), 2, getDoubleParser, {NAME, TARGET, {}, {}}),
        ParseT(
            SUCCESS, "-42    ", j(fmt::format(R"({{"{}": -42}})", TARGET)), 3, getDoubleParser, {NAME, TARGET, {}, {}}),
        ParseT(
            SUCCESS, "42#####", j(fmt::format(R"({{"{}": 42}})", TARGET)), 2, getDoubleParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS,
               "-42####",
               j(fmt::format(R"({{"{}": -42}})", TARGET)),
               3,
               getDoubleParser,
               {NAME, TARGET, {}, {}})));

INSTANTIATE_TEST_SUITE_P(ScaledFloatBuild,
                         HlpBuildTest,
                         ::testing::Values(BuildT(SUCCESS, getScaledFloatParser, {NAME, TARGET, {}, {}}),
                                           BuildT(FAILURE, getScaledFloatParser, {NAME, TARGET, {}, {"unexpected"}})));

INSTANTIATE_TEST_SUITE_P(
    ScaledFloatParse,
    HlpParseTest,
    ::testing::Values(
        ParseT(SUCCESS, "42", j(fmt::format(R"({{"{}": 42}})", TARGET)), 2, getScaledFloatParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "42.0", j(fmt::format(R"({{"{}": 42}})", TARGET)), 4, getScaledFloatParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "-42", j(fmt::format(R"({{"{}": -42}})", TARGET)), 3, getScaledFloatParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "-42.0", j(fmt::format(R"({{"{}": -42}})", TARGET)), 5, getScaledFloatParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "128", j(fmt::format(R"({{"{}": 128}})", TARGET)), 3, getScaledFloatParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "-129", j(fmt::format(R"({{"{}": -129}})", TARGET)), 4, getScaledFloatParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS,
               "9223372036854775808",
               j(fmt::format(R"({{"{}": 9223372036854775808}})", TARGET)),
               19,
               getScaledFloatParser,
               {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS,
               "-9223372036854775809",
               j(fmt::format(R"({{"{}": -9223372036854775809}})", TARGET)),
               20,
               getScaledFloatParser,
               {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS,
               "3.40282e+38",
               []()
               {
                   json::Json expected {};
                   expected.setDouble(double_t(3.40282e+38), json::Json::formatJsonPath(TARGET));
                   return expected;
               }(),
               11,
               getScaledFloatParser,
               {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS,
               "-3.40282e+38",
               []()
               {
                   json::Json expected {};
                   expected.setDouble(double_t(-3.40282e+38), json::Json::formatJsonPath(TARGET));
                   return expected;
               }(),
               12,
               getScaledFloatParser,
               {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS,
               "1.79769e+308",
               []()
               {
                   json::Json expected {};
                   expected.setDouble(double_t(1.79769e+308), json::Json::formatJsonPath(TARGET));
                   return expected;
               }(),
               12,
               getScaledFloatParser,
               {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS,
               "-1.79769e+308",
               []()
               {
                   json::Json expected {};
                   expected.setDouble(double_t(-1.79769e+308), json::Json::formatJsonPath(TARGET));
                   return expected;
               }(),
               13,
               getScaledFloatParser,
               {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS, "42    ", j(fmt::format(R"({{"{}": 42}})", TARGET)), 2, getScaledFloatParser, {NAME, TARGET, {}, {}}),
        ParseT(
            SUCCESS, "-42    ", j(fmt::format(R"({{"{}": -42}})", TARGET)), 3, getScaledFloatParser, {NAME, TARGET, {}, {}}),
        ParseT(
            SUCCESS, "42#####", j(fmt::format(R"({{"{}": 42}})", TARGET)), 2, getScaledFloatParser, {NAME, TARGET, {}, {}}),
        ParseT(SUCCESS,
               "-42####",
               j(fmt::format(R"({{"{}": -42}})", TARGET)),
               3,
               getScaledFloatParser,
               {NAME, TARGET, {}, {}})));

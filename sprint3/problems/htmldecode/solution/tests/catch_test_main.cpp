#include <catch2/catch_test_macros.hpp>

#include "../src/leap_year.h"

TEST_CASE("Leap year testing") {
    CHECK(IsLeapYear(2020));
    CHECK(!IsLeapYear(2021));
    CHECK(!IsLeapYear(2022));
    CHECK(!IsLeapYear(2023));
    CHECK(IsLeapYear(2024));
    CHECK(!IsLeapYear(1900));
    CHECK(IsLeapYear(2000));
}

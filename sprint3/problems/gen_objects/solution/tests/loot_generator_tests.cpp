#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include "loot_generator.h"

using namespace std::chrono_literals;

TEST_CASE("No looters or no shortage means no loot", "[loot_generator]") {
    loot_gen::LootGenerator generator{1s, 1.0};
    CHECK(generator.Generate(1s, 0, 0) == 0);
    CHECK(generator.Generate(1s, 3, 3) == 0);
    CHECK(generator.Generate(1s, 5, 3) == 0);
}

TEST_CASE("Probability zero never produces loot", "[loot_generator]") {
    loot_gen::LootGenerator generator{1s, 0.0};
    CHECK(generator.Generate(1s, 0, 100) == 0);
    CHECK(generator.Generate(100s, 0, 100) == 0);
}

TEST_CASE("Probability one fills only the shortage", "[loot_generator]") {
    loot_gen::LootGenerator generator{1s, 1.0};
    CHECK(generator.Generate(1s, 2, 5) == 3);
    CHECK(generator.Generate(1s, 5, 5) == 0);
}

TEST_CASE("Time accumulates until loot is generated and then resets", "[loot_generator]") {
    loot_gen::LootGenerator generator{1s, 0.5};
    CHECK(generator.Generate(500ms, 0, 1) == 0);
    CHECK(generator.Generate(500ms, 0, 1) == 1);
    CHECK(generator.Generate(500ms, 0, 1) == 0);
    CHECK(generator.Generate(500ms, 0, 1) == 1);
}

TEST_CASE("Long intervals use the supplied generator formula", "[loot_generator]") {
    loot_gen::LootGenerator generator{1s, 0.5};
    CHECK(generator.Generate(2s, 0, 8) == 6);
}

TEST_CASE("Injected random multiplier is used", "[loot_generator]") {
    SECTION("Zero multiplier") {
        loot_gen::LootGenerator generator{1s, 1.0, [] { return 0.0; }};
        CHECK(generator.Generate(100s, 0, 8) == 0);
    }
    SECTION("Fractional multiplier") {
        loot_gen::LootGenerator generator{1s, 1.0, [] { return 0.25; }};
        CHECK(generator.Generate(1s, 0, 8) == 2);
    }
}

TEST_CASE("Generated loot never exceeds shortage", "[loot_generator]") {
    for (unsigned dogs = 0; dogs < 20; ++dogs) {
        for (unsigned loot = 0; loot < 25; ++loot) {
            for (double probability : {0.0, 0.1, 0.5, 1.0}) {
                loot_gen::LootGenerator generator{1s, probability};
                const unsigned count = generator.Generate(10s, loot, dogs);
                CHECK(count <= (dogs > loot ? dogs - loot : 0));
            }
        }
    }
}

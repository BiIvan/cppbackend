#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <chrono>
#include <set>
#include "player.h"

using namespace std::chrono_literals;

namespace {
model::Map MakeMap() {
    model::Map map{model::Map::Id{"test"}, "Test"};
    map.SetLootTypesCount(7);
    map.AddRoad(model::Road{model::Road::HORIZONTAL, {40, 0}, -10});
    map.AddRoad(model::Road{model::Road::VERTICAL, {5, 30}, -20});
    return map;
}
}

TEST_CASE("Empty session contains no loot and no dogs", "[model]") {
    auto map = MakeMap();
    model::GameSession session{&map, false, {1s, 1.0}, [] { return 1.0; }, 42};
    session.Tick(10s);
    CHECK(session.GetDogs().empty());
    CHECK(session.GetLostObjects().empty());
}

TEST_CASE("Session generates loot and respects the dog count", "[model]") {
    auto map = MakeMap();
    model::GameSession session{&map, false, {1s, 1.0}, [] { return 1.0; }, 42};
    session.AddDog("First");
    session.Tick(1s);
    REQUIRE(session.GetLostObjects().size() == 1);
    const auto initial = session.GetLostObjects().front();
    session.Tick(10s);
    REQUIRE(session.GetLostObjects().size() == 1);
    CHECK(session.GetLostObjects().front().id == initial.id);
    CHECK(session.GetLostObjects().front().type == initial.type);
    CHECK(session.GetLostObjects().front().position.x == initial.position.x);
    CHECK(session.GetLostObjects().front().position.y == initial.position.y);
    session.AddDog("Second");
    session.Tick(1s);
    REQUIRE(session.GetLostObjects().size() == 2);
    CHECK(session.GetLostObjects()[0].id != session.GetLostObjects()[1].id);
}

TEST_CASE("Session forwards precise accumulated milliseconds", "[model]") {
    auto map = MakeMap();
    model::GameSession session{&map, false, {1s, 0.5}, [] { return 1.0; }, 42};
    session.AddDog("Dog");
    session.Tick(499ms);
    CHECK(session.GetLostObjects().empty());
    session.Tick(501ms);
    CHECK(session.GetLostObjects().size() == 1);
}

TEST_CASE("Objects have valid types, unique IDs and positions on roads", "[model]") {
    auto map = MakeMap();
    model::GameSession session{&map, false, {1s, 1.0}, [] { return 1.0; }, 42};
    for (int i = 0; i < 300; ++i) {
        session.AddDog("Dog");
    }
    session.Tick(1s);
    REQUIRE(session.GetLostObjects().size() == 300);
    std::set<std::uint64_t> ids;
    for (const auto& loot : session.GetLostObjects()) {
        CHECK(loot.type < map.GetLootTypesCount());
        CHECK(ids.insert(loot.id).second);
        const auto pos = loot.position;
        const bool horizontal = pos.y == 0.0 && pos.x >= -10.0 && pos.x <= 40.0;
        const bool vertical = pos.x == 5.0 && pos.y >= -20.0 && pos.y <= 30.0;
        CHECK((horizontal || vertical));
    }
}

TEST_CASE("Single loot type and zero length road are supported", "[model]") {
    model::Map map{model::Map::Id{"point"}, "Point"};
    map.SetLootTypesCount(1);
    map.AddRoad(model::Road{model::Road::HORIZONTAL, {3, 4}, 3});
    model::GameSession session{&map, false, {1s, 1.0}};
    session.AddDog("Dog");
    session.Tick(1s);
    REQUIRE(session.GetLostObjects().size() == 1);
    const auto& loot = session.GetLostObjects().front();
    CHECK(loot.type == 0);
    CHECK(loot.position.x == 3.0);
    CHECK(loot.position.y == 4.0);
}

TEST_CASE("Sessions keep independent generation timers", "[model]") {
    auto map = MakeMap();
    model::GameSession first{&map, false, {1s, 0.5}};
    model::GameSession second{&map, false, {1s, 0.5}};
    first.AddDog("First");
    second.AddDog("Second");
    first.Tick(500ms);
    second.Tick(500ms);
    CHECK(first.GetLostObjects().empty());
    CHECK(second.GetLostObjects().empty());
    first.Tick(500ms);
    CHECK(first.GetLostObjects().size() == 1);
    CHECK(second.GetLostObjects().empty());
    second.Tick(500ms);
    CHECK(second.GetLostObjects().size() == 1);
}

TEST_CASE("Loot generation does not break dog movement", "[model]") {
    auto map = MakeMap();
    model::GameSession session{&map, false, {1s, 1.0}};
    auto& dog = session.AddDog("Dog");
    dog.SetMove(model::Direction::WEST, 2.0);
    session.Tick(500ms);
    CHECK(dog.GetPosition().x == Catch::Approx(39.0));
    CHECK(dog.GetPosition().y == 0.0);
    CHECK(session.GetLostObjects().size() == 1);
}

TEST_CASE("Zero and negative ticks", "[model]") {
    auto map = MakeMap();
    model::GameSession session{&map, false, {1s, 1.0}};
    session.AddDog("Dog");
    session.Tick(0ms);
    CHECK(session.GetLostObjects().empty());
    CHECK_THROWS_AS(session.Tick(-1ms), std::invalid_argument);
}

TEST_CASE("Invalid map and generator configuration are rejected", "[model]") {
    auto map = MakeMap();
    CHECK_THROWS_AS(map.SetLootTypesCount(0), std::invalid_argument);
    CHECK_THROWS_AS((model::GameSession{nullptr, false}), std::invalid_argument);
    model::Map no_roads{model::Map::Id{"empty"}, "Empty"};
    no_roads.SetLootTypesCount(1);
    CHECK_THROWS_AS((model::GameSession{&no_roads, false}), std::invalid_argument);
    CHECK_THROWS_AS((model::GameSession{&map, false, {0ms, 0.5}}), std::invalid_argument);
    CHECK_THROWS_AS((model::GameSession{&map, false, {1s, 1.5}}), std::invalid_argument);
}

TEST_CASE("Zero probability is honored by the session", "[model]") {
    auto map = MakeMap();
    model::GameSession session{&map, false, {1s, 0.0}};
    session.AddDog("Dog");
    session.Tick(100s);
    CHECK(session.GetLostObjects().empty());
}

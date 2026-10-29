#include <catch2/catch_test_macros.hpp>
#include "player.h"
using namespace std::chrono_literals;
namespace {
model::Map ScoreMap() {
    model::Map map{model::Map::Id{"score"}, "Score"};
    map.SetLootTypesCount(2);
    map.SetLootValue(0, 10);
    map.SetLootValue(1, 30);
    map.AddRoad(model::Road{model::Road::HORIZONTAL, {0, 0}, 40});
    map.AddOffice(model::Office{model::Office::Id{"office"}, {6, 0}, {5, 0}});
    return map;
}
}
TEST_CASE("Score starts at zero and changes only on return", "[score]") {
    auto map = ScoreMap();
    model::GameSession session{&map, false, {1s, 0.0}};
    auto& dog = session.AddDog("Dog");
    CHECK(dog.GetScore() == 0);
    session.AddLostObject(0, {2, 0});
    session.AddLostObject(1, {4, 0});
    dog.SetMove(model::Direction::EAST, 5);
    session.Tick(1s);
    REQUIRE(dog.GetBag().size() == 2);
    CHECK(dog.GetScore() == 0);
    session.Tick(1s);
    CHECK(dog.GetBag().empty());
    CHECK(dog.GetScore() == 40);
    dog.ReturnLoot(map);
    CHECK(dog.GetScore() == 40);
}
TEST_CASE("Score accumulates and remains isolated per dog", "[score]") {
    auto map = ScoreMap();
    model::GameSession session{&map, false, {1s, 0.0}};
    auto& first = session.AddDog("First");
    auto& second = session.AddDog("Second");
    REQUIRE(first.TryCollect({100, 1, {}}, 3));
    first.ReturnLoot(map);
    REQUIRE(first.TryCollect({101, 0, {}}, 3));
    first.ReturnLoot(map);
    CHECK(first.GetScore() == 40);
    CHECK(second.GetScore() == 0);
}
TEST_CASE("Return awards only items preceding office in same tick", "[score]") {
    auto map = ScoreMap();
    map.SetBagCapacity(1);
    model::GameSession session{&map, false, {1s, 0.0}};
    auto& dog = session.AddDog("Dog");
    session.AddLostObject(0, {2, 0});
    session.AddLostObject(1, {4, 0});
    const auto last = session.AddLostObject(1, {8, 0}).id;
    dog.SetMove(model::Direction::EAST, 10);
    session.Tick(1s);
    CHECK(dog.GetScore() == 10);
    REQUIRE(dog.GetBag().size() == 1);
    CHECK(dog.GetBag()[0].id == last);
    REQUIRE(session.GetLostObjects().size() == 1);
    CHECK(session.GetLostObjects()[0].type == 1);
}
TEST_CASE("Score overflow does not partially return bag", "[score]") {
    auto map = ScoreMap();
    map.SetLootValue(0, std::numeric_limits<std::uint64_t>::max());
    model::Dog dog{model::DogId{0}, "Dog", {}};
    REQUIRE(dog.TryCollect({0, 0, {}}, 3));
    dog.ReturnLoot(map);
    REQUIRE(dog.TryCollect({1, 1, {}}, 3));
    CHECK_THROWS_AS(dog.ReturnLoot(map), std::overflow_error);
    CHECK(dog.GetScore() == std::numeric_limits<std::uint64_t>::max());
    CHECK(dog.GetBag().size() == 1);
}

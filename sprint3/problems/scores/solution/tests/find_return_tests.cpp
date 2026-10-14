#include <catch2/catch_test_macros.hpp>
#include "player.h"
using namespace std::chrono_literals;
namespace {
model::Map CollectionMap(std::size_t capacity = 3) {
    model::Map map{model::Map::Id{"collect"}, "Collect"};
    map.SetLootTypesCount(5);
    map.SetBagCapacity(capacity);
    map.AddRoad(model::Road{model::Road::HORIZONTAL, {0, 0}, 40});
    return map;
}
}
TEST_CASE("Pickup skip return pickup in one tick", "[find-return]") {
    auto map = CollectionMap(1);
    map.AddOffice(model::Office{model::Office::Id{"office"}, {6, 0}, {5, 0}});
    model::GameSession session{&map, false, {1s, 0.0}};
    auto& dog = session.AddDog("Dog");
    session.AddLostObject(1, {2, 0});
    const auto skipped = session.AddLostObject(2, {4, 0}).id;
    const auto last = session.AddLostObject(3, {8, 0}).id;
    dog.SetMove(model::Direction::EAST, 10);
    session.Tick(1s);
    REQUIRE(dog.GetBag().size() == 1);
    CHECK(dog.GetBag()[0].id == last);
    CHECK(dog.GetBag()[0].type == 3);
    REQUIRE(session.GetLostObjects().size() == 1);
    CHECK(session.GetLostObjects()[0].id == skipped);
}
TEST_CASE("Global time beats player iteration order and distance", "[find-return]") {
    auto map = CollectionMap();
    model::GameSession session{&map, false, {1s, 0.0}};
    auto& slower = session.AddDog("Slower");
    slower.SetPosition({4, 0});
    slower.SetMove(model::Direction::EAST, 2);
    auto& faster = session.AddDog("Faster");
    faster.SetMove(model::Direction::EAST, 20);
    const auto id = session.AddLostObject(4, {5, 0}).id;
    session.Tick(1s);
    CHECK(slower.GetBag().empty());
    REQUIRE(faster.GetBag().size() == 1);
    CHECK(faster.GetBag()[0].id == id);
    CHECK(session.GetLostObjects().empty());
}
TEST_CASE("Full earlier dog leaves loot for later dog", "[find-return]") {
    auto map = CollectionMap(1);
    model::GameSession session{&map, false, {1s, 0.0}};
    auto& earlier = session.AddDog("Earlier");
    REQUIRE(earlier.TryCollect({100, 0, {}}, 1));
    earlier.SetMove(model::Direction::EAST, 20);
    auto& later = session.AddDog("Later");
    later.SetPosition({4, 0});
    later.SetMove(model::Direction::EAST, 2);
    const auto id = session.AddLostObject(2, {5, 0}).id;
    session.Tick(1s);
    REQUIRE(later.GetBag().size() == 1);
    CHECK(later.GetBag()[0].id == id);
    CHECK(earlier.GetBag()[0].id == 100);
}
TEST_CASE("Bag preserves pickup order not insertion order", "[find-return]") {
    auto map = CollectionMap();
    model::GameSession session{&map, false, {1s, 0.0}};
    auto& dog = session.AddDog("Dog");
    const auto far = session.AddLostObject(4, {8, 0}).id;
    const auto near = session.AddLostObject(2, {2, 0}).id;
    dog.SetMove(model::Direction::EAST, 10);
    session.Tick(1s);
    REQUIRE(dog.GetBag().size() == 2);
    CHECK(dog.GetBag()[0].id == near);
    CHECK(dog.GetBag()[1].id == far);
    CHECK(dog.GetBag()[0].type == 2);
    CHECK(dog.GetBag()[1].type == 4);
}
TEST_CASE("Road edge misses loot but reaches office", "[find-return]") {
    auto map = CollectionMap();
    map.AddOffice(model::Office{model::Office::Id{"o"}, {6, 0}, {20, 0}});
    model::GameSession session{&map, false, {1s, 0.0}};
    auto& dog = session.AddDog("Dog");
    dog.SetPosition({0, 0.4});
    REQUIRE(dog.TryCollect({100, 1, {}}, 3));
    session.AddLostObject(2, {2, 0});
    dog.SetMove(model::Direction::EAST, 10);
    session.Tick(1s);
    CHECK(dog.GetBag().empty());
    CHECK(session.GetLostObjects().size() == 1);
}
TEST_CASE("Stationary dogs and zero capacity do not collect", "[find-return]") {
    auto map = CollectionMap();
    SECTION("Stationary") {
        model::GameSession session{&map, false, {1s, 0.0}};
        auto& dog = session.AddDog("Dog");
        session.AddLostObject(0, {0, 0});
        session.Tick(1s);
        CHECK(dog.GetBag().empty());
        CHECK(session.GetLostObjects().size() == 1);
    }
    SECTION("Zero capacity") {
        map.SetBagCapacity(0);
        model::GameSession session{&map, false, {1s, 0.0}};
        auto& dog = session.AddDog("Dog");
        session.AddLostObject(0, {5, 0});
        dog.SetMove(model::Direction::EAST, 10);
        session.Tick(1s);
        CHECK(dog.GetBag().empty());
        CHECK(session.GetLostObjects().size() == 1);
    }
}
TEST_CASE("Collection uses clamped actual endpoint", "[find-return]") {
    auto map = CollectionMap();
    model::GameSession session{&map, false, {1s, 0.0}};
    auto& dog = session.AddDog("Dog");
    session.AddLostObject(0, {30, 0});
    const auto beyond = session.AddLostObject(1, {45, 0}).id;
    dog.SetMove(model::Direction::EAST, 100);
    session.Tick(1s);
    REQUIRE(dog.GetBag().size() == 1);
    REQUIRE(session.GetLostObjects().size() == 1);
    CHECK(session.GetLostObjects()[0].id == beyond);
}

TEST_CASE("Pickup includes exact radius boundary", "[find-return]") {
    auto map = CollectionMap();
    model::GameSession session{&map, false, {1s, 0.0}};
    auto& dog = session.AddDog("Dog");
    session.AddLostObject(1, {5, 0.3});
    session.AddLostObject(2, {7, 0.300001});
    dog.SetMove(model::Direction::EAST, 10);
    session.Tick(1s);
    REQUIRE(dog.GetBag().size() == 1);
    CHECK(dog.GetBag()[0].type == 1);
    REQUIRE(session.GetLostObjects().size() == 1);
    CHECK(session.GetLostObjects()[0].type == 2);
}
TEST_CASE("Detector includes combined office radius boundary", "[find-return]") {
    class Provider final : public collision_detector::ItemGathererProvider {
    public:
        double y = 0.55;
        std::size_t ItemsCount() const override { return 1; }
        collision_detector::Item GetItem(std::size_t) const override {
            return {{5, 0}, 0.25};
        }
        std::size_t GatherersCount() const override { return 1; }
        collision_detector::Gatherer GetGatherer(std::size_t) const override {
            return {{0, y}, {10, y}, 0.3};
        }
    } provider;
    CHECK(collision_detector::FindGatherEvents(provider).size() == 1);
    provider.y = 0.550001;
    CHECK(collision_detector::FindGatherEvents(provider).empty());
}

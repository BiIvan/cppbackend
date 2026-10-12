#define _USE_MATH_DEFINES

#include "../src/collision_detector.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <initializer_list>
#include <utility>
#include <vector>

namespace {

  namespace cd = collision_detector;

  class TestProvider final : public cd::ItemGathererProvider {
  public:
    TestProvider(std::vector<cd::Item> items,
               std::vector<cd::Gatherer> gatherers)
      : items_(std::move(items))
      , gatherers_(std::move(gatherers)) {
    }

    std::size_t ItemsCount() const override {
      return items_.size();
    }

    cd::Item GetItem(std::size_t idx) const override {
      return items_.at(idx);
    }

    std::size_t GatherersCount() const override {
      return gatherers_.size();
    }

    cd::Gatherer GetGatherer(std::size_t idx) const override {
      return gatherers_.at(idx);
    }

  private:
    std::vector<cd::Item> items_;
    std::vector<cd::Gatherer> gatherers_;
    };

    void CheckEvents(
    const TestProvider& provider,
    std::initializer_list<cd::GatheringEvent> expected) {
    const auto actual = cd::FindGatherEvents(provider);

    REQUIRE(actual.size() == expected.size());

    std::size_t index = 0;

    for (const auto& event : expected) {
      CAPTURE(index);

      const auto& result = actual.at(index);

      CHECK(result.item_id == event.item_id);
      CHECK(result.gatherer_id == event.gatherer_id);
      CHECK(result.sq_distance
            == Catch::Approx(event.sq_distance).margin(1e-10));
      CHECK(result.time
            == Catch::Approx(event.time).margin(1e-10));

      ++index;
    }
  }

}  // namespace

TEST_CASE("Empty input produces no gathering events", "[gather]") {
  SECTION("No items and no gatherers") {
    CheckEvents(TestProvider{{}, {}}, {});
  }
  SECTION("No items") {
    CheckEvents(
        TestProvider{
            {},
            {{{0.0, 0.0}, {10.0, 0.0}, 1.0}}},
        {});
  }
  SECTION("No gatherers") {
    CheckEvents(
        TestProvider{
            {{{5.0, 0.0}, 1.0}},
            {}},
        {});
  }
}

TEST_CASE("Stationary gatherers are ignored", "[gather]") {
  SECTION("An item is at the stationary gatherer's position") {
    CheckEvents(
      TestProvider{
        {{{5.0, 5.0}, 1.0}},
        {{{5.0, 5.0}, {5.0, 5.0}, 1.0}}},
      {});
  }
  SECTION("A stationary gatherer does not hide a moving gatherer") {
    CheckEvents(
      TestProvider{
        {{{5.0, 0.0}, 0.0}},
        {
          {{5.0, 0.0}, {5.0, 0.0}, 1.0},
          {{0.0, 0.0}, {10.0, 0.0}, 0.0},
        }},
      {{0, 1, 0.0, 0.5}});
  }
}

TEST_CASE("Items on the movement segment are collected", "[gather]") {
  SECTION("An item inside the segment") {
    CheckEvents(
      TestProvider{
        {{{3.0, 0.0}, 0.0}},
        {{{0.0, 0.0}, {10.0, 0.0}, 0.0}}},
      {{0, 0, 0.0, 0.3}});
  }
  SECTION("Both segment endpoints are included") {
    CheckEvents(
      TestProvider{
        {
          {{10.0, 0.0}, 0.0},
          {{0.0, 0.0}, 0.0},
        },
        {{{0.0, 0.0}, {10.0, 0.0}, 0.0}}},
      {
        {1, 0, 0.0, 0.0},
        {0, 0, 0.0, 1.0},
    });
  }
}

TEST_CASE("Projection outside the movement segment is rejected", "[gather]") {
  SECTION("Items before the start and after the end") {
    CheckEvents(
      TestProvider{
        {
          {{-1.0, 0.0}, 0.0},
          {{11.0, 0.0}, 0.0},
        },
        {{{0.0, 0.0}, {10.0, 0.0}, 0.0}}},
      {});
  }
  SECTION("A large radius does not extend the projection interval") {
    CheckEvents(
      TestProvider{
        {
          {{-1.0, 0.0}, 1.0},
          {{11.0, 0.0}, 1.0},
        },
        {{{0.0, 0.0}, {10.0, 0.0}, 10.0}}},
      {});
  }
}

TEST_CASE("Collection uses the sum of widths", "[gather]") {
SECTION("Only the gatherer has nonzero width") {
  CheckEvents(
    TestProvider{
      {{{5.0, 2.0}, 0.0}},
      {{{0.0, 0.0}, {10.0, 0.0}, 2.0}}},
    {{0, 0, 4.0, 0.5}});
}

SECTION("Only the item has nonzero width") {
  CheckEvents(
    TestProvider{
      {{{5.0, -2.0}, 2.0}},
      {{{0.0, 0.0}, {10.0, 0.0}, 0.0}}},
    {{0, 0, 4.0, 0.5}});
}

SECTION("Both widths contribute") {
  CheckEvents(
    TestProvider{
      {{{5.0, 2.5}, 1.0}},
      {{{0.0, 0.0}, {10.0, 0.0}, 2.0}}},
    {{0, 0, 6.25, 0.5}});
}

SECTION("The exact radius boundary is included") {
  CheckEvents(
    TestProvider{
      {{{5.0, -3.0}, 1.0}},
      {{{0.0, 0.0}, {10.0, 0.0}, 2.0}}},
    {{0, 0, 9.0, 0.5}});
}

SECTION("An item outside the combined radius is rejected") {
  CheckEvents(
    TestProvider{
      {{{5.0, 3.001}, 1.0}},
      {{{0.0, 0.0}, {10.0, 0.0}, 2.0}}},
    {});
}
}

TEST_CASE("Movement direction and position are respected", "[gather]") {
  SECTION("Vertical movement") {
    CheckEvents(
      TestProvider{
        {{{-2.0, 3.0}, 0.0}},
        {{{0.0, 0.0}, {0.0, 10.0}, 2.0}}},
      {{0, 0, 4.0, 0.3}});
  }

  SECTION("Reverse horizontal movement") {
    CheckEvents(
      TestProvider{
        {{{2.0, 0.0}, 0.0}},
        {{{10.0, 0.0}, {0.0, 0.0}, 0.0}}},
      {{0, 0, 0.0, 0.8}});
  }

  SECTION("Diagonal movement with nonzero distance") {
    // Projection of (4, 0) onto (0, 0)--(4, 4) is (2, 2).
    CheckEvents(
      TestProvider{
        {{{4.0, 0.0}, 0.0}},
        {{{0.0, 0.0}, {4.0, 4.0}, 3.0}}},
      {{0, 0, 8.0, 0.5}});
  }

  SECTION("Translated diagonal movement") {
    CheckEvents(
      TestProvider{
        {{{-3.0, -2.0}, 0.0}},
        {{{-5.0, -4.0}, {3.0, 4.0}, 0.0}}},
      {{0, 0, 0.0, 0.25}});
  }

  SECTION("Small nonzero movement is not stationary") {
    CheckEvents(
      TestProvider{
        {{{0.0000005, 0.0}, 0.0}},
        {{{0.0, 0.0}, {0.000001, 0.0}, 0.0}}},
      {{0, 0, 0.0, 0.5}});
  }
}

TEST_CASE("Events preserve original item indices", "[gather]") {
CheckEvents(
  TestProvider{
    {
      {{1.0, 10.0}, 0.0},
      {{8.0, 0.0}, 0.0},
      {{4.0, 10.0}, 0.0},
      {{2.0, 0.0}, 0.0},
    },
    {{{0.0, 0.0}, {10.0, 0.0}, 0.0}}},
  {
    {3, 0, 0.0, 0.2},
    {1, 0, 0.0, 0.8},
  });
}

TEST_CASE("Events are globally sorted by normalized time", "[gather]") {
// Item order and gatherer order differ from event time order.
CheckEvents(
  TestProvider{
    {
      {{8.0, 0.0}, 0.0},
      {{4.0, 10.0}, 0.0},
      {{2.0, 0.0}, 0.0},
      {{18.0, 10.0}, 0.0},
    },
    {
      {{0.0, 0.0}, {10.0, 0.0}, 0.0},
      {{0.0, 10.0}, {20.0, 10.0}, 0.0},
    }},
  {
    {2, 0, 0.0, 0.2},
    {1, 1, 0.0, 0.2},
    {0, 0, 0.0, 0.8},
    {3, 1, 0.0, 0.9},
  });
}

TEST_CASE("One item can produce events for multiple gatherers", "[gather]") {
CheckEvents(
  TestProvider{
    {{{2.0, 0.0}, 0.0}},
    {
      {{0.0, 0.0}, {10.0, 0.0}, 0.0},
      {{10.0, 0.0}, {0.0, 0.0}, 0.0},
    }},
  {
    {0, 0, 0.0, 0.2},
    {0, 1, 0.0, 0.8},
  });
}

TEST_CASE("Equal-time events are all returned", "[gather]") {
  const TestProvider provider{
    {
      {{5.0, 1.0}, 0.0},
      {{5.0, -1.0}, 0.0},
    },
    {{{0.0, 0.0}, {10.0, 0.0}, 1.0}}};
  const auto events = cd::FindGatherEvents(provider);
  REQUIRE(events.size() == 2);
  bool found_first = false;
  bool found_second = false;
  for (const auto& event : events) {
    CHECK(event.gatherer_id == 0);
    CHECK(event.sq_distance == Catch::Approx(1.0));
    CHECK(event.time == Catch::Approx(0.5));
    if (event.item_id == 0) {
      CHECK_FALSE(found_first);
      found_first = true;
    } else if (event.item_id == 1) {
      CHECK_FALSE(found_second);
      found_second = true;
    } else {
      FAIL("Unexpected item_id");
    }
  }
  CHECK(found_first);
  CHECK(found_second);
}

int main(int argc, char* argv[]) {
  return Catch::Session().run(argc, argv);
}
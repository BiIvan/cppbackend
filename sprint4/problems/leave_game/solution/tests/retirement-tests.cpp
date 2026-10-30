//2026.10.30 17:23

#include <filesystem>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "application.h"
#include "model_serialization.h"

using namespace std::chrono_literals;

namespace {
  
  model::Game MakeGame() {
    model::Game game;
    model::Map map{model::Map::Id{"map"}, "Map"};
    map.SetLootTypesCount(1);
    map.AddRoad(model::Road{model::Road::HORIZONTAL, {0,0}, 10});
    game.AddMap(std::move(map));
    game.SetDogRetirementTime(10);
    game.SetLootGeneratorConfig({5s, 0});
    return game;
  }
}

TEST_CASE("Idle retirement uses exact threshold, not tick end") {
  auto game = MakeGame();
  app::Application app{game, false};
  double recorded = -1;
  int records = 0;
  app.SetRecordWriter([&](const auto& dog) { recorded = dog.GetPlayTimeMs(); ++records; });
  const auto joined = app.JoinGame(model::Map::Id{"map"}, "Dog");
  app.Tick(9999);
  REQUIRE(app.FindPlayerByToken(joined.token));
  app.Tick(100001);
  CHECK_FALSE(app.FindPlayerByToken(joined.token));
  CHECK(recorded == 10000);
  app.Tick(10000);
  CHECK(records == 1);
}

TEST_CASE("Boundary stop starts idle period inside a long tick") {
  auto game = MakeGame();
  app::Application app{game, false};
  double recorded = -1;
  app.SetRecordWriter([&](const auto& dog) { recorded = dog.GetPlayTimeMs(); });
  auto joined = app.JoinGame(model::Map::Id{"map"}, "Dog");
  app.FindPlayerByToken(joined.token)->GetDog().SetMove(model::Direction::EAST, 2);
  app.Tick(20000);
  CHECK_FALSE(app.FindPlayerByToken(joined.token));
  CHECK(recorded == Catch::Approx(15200));
}

TEST_CASE("Repeated stop does not reset idle; movement does") {
  auto game = MakeGame();
  app::Application app{game, false};
  auto joined = app.JoinGame(model::Map::Id{"map"}, "Dog");
  app.Tick(8000);
  auto& dog = app.FindPlayerByToken(joined.token)->GetDog();
  dog.Stop();
  app.Tick(1000);
  dog.SetMove(model::Direction::EAST, 1);
  app.Tick(1000);
  dog.Stop();
  app.Tick(9999);
  REQUIRE(app.FindPlayerByToken(joined.token));
  app.Tick(1);
  CHECK_FALSE(app.FindPlayerByToken(joined.token));
}

TEST_CASE("Erasing middle dog preserves other player pointers") {
  auto game = MakeGame();
  app::Application app{game, false};
  auto a = app.JoinGame(model::Map::Id{"map"}, "A");
  auto b = app.JoinGame(model::Map::Id{"map"}, "B");
  auto c = app.JoinGame(model::Map::Id{"map"}, "C");
  auto* pa = &app.FindPlayerByToken(a.token)->GetDog();
  auto* pc = &app.FindPlayerByToken(c.token)->GetDog();
  pa->SetMove(model::Direction::EAST, 0.01);
  pc->SetMove(model::Direction::EAST, 0.01);
  app.Tick(10000);
  CHECK_FALSE(app.FindPlayerByToken(b.token));
  REQUIRE(app.FindPlayerByToken(a.token));
  REQUIRE(app.FindPlayerByToken(c.token));
  CHECK(&app.FindPlayerByToken(a.token)->GetDog() == pa);
  CHECK(&app.FindPlayerByToken(c.token)->GetDog() == pc);
  CHECK(pc->GetName() == "C");
}

TEST_CASE("State save restores idle and play timers") {
  auto game = MakeGame();
  app::Application app{game, false};
  auto joined = app.JoinGame(model::Map::Id{"map"}, "Dog");
  app.Tick(7500);
  auto path = std::filesystem::temp_directory_path() /
      (app.FindPlayerByToken(joined.token)->GetDog().GetRecordId() + ".json");
  serialization::StateSerializer::Save(app, path);
  app::Application restored{game, false};
  double recorded = -1;
  restored.SetRecordWriter([&](const auto& dog) { recorded = dog.GetPlayTimeMs(); });
  serialization::StateSerializer::Load(restored, path);
  std::filesystem::remove(path);
  restored.Tick(2499);
  REQUIRE(restored.FindPlayerByToken(joined.token));
  restored.Tick(1);
  CHECK_FALSE(restored.FindPlayerByToken(joined.token));
  CHECK(recorded == 10000);
}

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include "json_loader.h"

namespace {
struct TempConfig {
    std::filesystem::path path = std::filesystem::temp_directory_path()
        / ("loot-config-" + std::to_string(std::random_device{}()) + ".json");
    ~TempConfig() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
    void Write(const boost::json::value& value) const {
        std::ofstream out{path};
        out.exceptions(std::ios::failbit | std::ios::badbit);
        out << boost::json::serialize(value);
    }
};
boost::json::object Config() {
    return boost::json::parse(R"({
      "lootGeneratorConfig":{"period":2.5,"probability":0.25},
      "maps":[{
        "id":"map1","name":"Map 1",
        "roads":[{"x0":0,"y0":0,"x1":10}],
        "buildings":[],"offices":[],
        "lootTypes":[{"name":"key","custom":{"enabled":true,"values":[1,2]}},
                     {"name":"wallet","scale":0.01}]
      }]
    })").as_object();
}
}

TEST_CASE("Loader keeps game data separate from arbitrary frontend JSON", "[loader]") {
    TempConfig temp;
    const auto config = Config();
    temp.Write(config);
    extra_data::ExtraData extra;
    auto game = json_loader::LoadGame(temp.path, extra);
    const auto* map = game.FindMap(model::Map::Id{"map1"});
    REQUIRE(map != nullptr);
    CHECK(map->GetLootTypesCount() == 2);
    CHECK(game.GetLootGeneratorConfig().period == std::chrono::milliseconds{2500});
    CHECK(game.GetLootGeneratorConfig().probability == 0.25);
    CHECK(extra.GetLootTypes("map1") == config.at("maps").as_array()[0].as_object().at("lootTypes").as_array());
}

TEST_CASE("Loader rejects invalid loot configuration", "[loader]") {
    TempConfig temp;
    auto config = Config();
    SECTION("Empty lootTypes") {
        config.at("maps").as_array()[0].as_object()["lootTypes"] = boost::json::array{};
    }
    SECTION("Non object loot type") {
        config.at("maps").as_array()[0].as_object()["lootTypes"] = boost::json::array{42};
    }
    SECTION("Zero period") {
        config.at("lootGeneratorConfig").as_object()["period"] = 0;
    }
    SECTION("Negative probability") {
        config.at("lootGeneratorConfig").as_object()["probability"] = -0.1;
    }
    SECTION("Probability above one") {
        config.at("lootGeneratorConfig").as_object()["probability"] = 1.1;
    }
    temp.Write(config);
    extra_data::ExtraData extra;
    CHECK_THROWS(json_loader::LoadGame(temp.path, extra));
}

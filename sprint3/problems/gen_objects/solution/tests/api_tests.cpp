#include <catch2/catch_test_macros.hpp>
#include <optional>
#include <type_traits>
#include "request_handler.h"

namespace {
namespace http = boost::beast::http;
namespace json = boost::json;
using Response = http::response<http::string_body>;

Response Request(http_handler::RequestHandler& handler, boost::asio::io_context& ioc,
                 http::verb method, const char* target,
                 std::string body = {}, std::string token = {}) {
    http::request<http::string_body> request{method, target, 11};
    request.set(http::field::content_type, "application/json");
    if (!token.empty()) {
        request.set(http::field::authorization, "Bearer " + token);
    }
    request.body() = std::move(body);
    request.prepare_payload();
    std::optional<Response> result;
    ioc.restart();
    handler(std::move(request), [&result](auto&& response) {
        if constexpr (std::is_same_v<std::decay_t<decltype(response)>, Response>) {
            result.emplace(std::move(response));
        }
    });
    ioc.run();
    if (!result) {
        throw std::runtime_error("No JSON response");
    }
    return std::move(*result);
}

struct Fixture {
    model::Game game;
    extra_data::ExtraData extra;
    boost::asio::io_context ioc;
    std::unique_ptr<http_handler::RequestHandler> handler;

    Fixture() {
        game.SetLootGeneratorConfig({std::chrono::milliseconds{1000}, 1.0});
        for (const char* id : {"map1", "map2"}) {
            model::Map map{model::Map::Id{std::string{id}}, id};
            map.SetLootTypesCount(2);
            map.AddRoad(model::Road{model::Road::HORIZONTAL, {0, 0}, 40});
            game.AddMap(std::move(map));
            extra.SetLootTypes(id, json::array{
                json::object{{"name", "key"}, {"custom", json::object{{"flag", true}}}},
                json::object{{"name", "wallet"}, {"scale", 0.01}}});
        }
        handler = std::make_unique<http_handler::RequestHandler>(
            game, extra, std::filesystem::current_path(),
            boost::asio::make_strand(ioc), false, false);
    }

    std::string Join(const char* map_id) {
        const auto response = Request(*handler, ioc, http::verb::post,
            "/api/v1/game/join", json::serialize(json::object{
                {"userName", "Dog"}, {"mapId", map_id}}));
        if (response.result() != http::status::ok) {
            throw std::runtime_error("Join failed: " + response.body());
        }
        return json::value_to<std::string>(json::parse(response.body()).as_object().at("authToken"));
    }

    json::object State(const std::string& token) {
        const auto response = Request(*handler, ioc, http::verb::get,
            "/api/v1/game/state", {}, token);
        if (response.result() != http::status::ok) {
            throw std::runtime_error("State failed");
        }
        return json::parse(response.body()).as_object();
    }
};
}

TEST_CASE("Map endpoint preserves frontend lootTypes", "[api]") {
    Fixture fixture;
    const auto response = Request(*fixture.handler, fixture.ioc, http::verb::get, "/api/v1/maps/map1");
    REQUIRE(response.result() == http::status::ok);
    const auto object = json::parse(response.body()).as_object();
    CHECK(object.at("lootTypes").as_array() == fixture.extra.GetLootTypes("map1"));
}

TEST_CASE("State contains lostObjects before and after a REST tick", "[api]") {
    Fixture fixture;
    const auto token = fixture.Join("map1");
    CHECK(fixture.State(token).at("lostObjects").as_object().empty());
    const auto tick = Request(*fixture.handler, fixture.ioc, http::verb::post,
                             "/api/v1/game/tick", R"({"timeDelta":1000})");
    REQUIRE(tick.result() == http::status::ok);
    const auto state = fixture.State(token);
    CHECK(state.at("players").as_object().size() == 1);
    const auto& loot = state.at("lostObjects").as_object();
    REQUIRE(loot.size() == 1);
    REQUIRE(loot.contains("0"));
    const auto& object = loot.at("0").as_object();
    const auto type = json::value_to<unsigned>(object.at("type"));
    CHECK(type < 2);
    const auto& pos = object.at("pos").as_array();
    REQUIRE(pos.size() == 2);
    CHECK(json::value_to<double>(pos[0]) >= 0.0);
    CHECK(json::value_to<double>(pos[0]) <= 40.0);
    CHECK(json::value_to<double>(pos[1]) == 0.0);
}

TEST_CASE("State is isolated to the authorized player session", "[api]") {
    Fixture fixture;
    const auto first = fixture.Join("map1");
    const auto second = fixture.Join("map2");
    fixture.Join("map1");
    const auto tick = Request(*fixture.handler, fixture.ioc, http::verb::post,
                             "/api/v1/game/tick", R"({"timeDelta":1000})");
    REQUIRE(tick.result() == http::status::ok);
    CHECK(fixture.State(first).at("lostObjects").as_object().size() == 2);
    CHECK(fixture.State(second).at("lostObjects").as_object().size() == 1);
}

TEST_CASE("State still requires authorization", "[api]") {
    Fixture fixture;
    const auto response = Request(*fixture.handler, fixture.ioc, http::verb::get, "/api/v1/game/state");
    CHECK(response.result() == http::status::unauthorized);
}

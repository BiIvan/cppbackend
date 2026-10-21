#pragma once

#include <cmath>
#include <cctype>
#include <limits>
#include <string>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <utility>
#include <iterator>
#include <algorithm>
#include <stdexcept>
#include <filesystem>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <boost/json.hpp>

#include "application.h"

namespace serialization {

// All methods require exclusive access to the application.
class StateSerializer {
    using Object = boost::json::object;
    using Array = boost::json::array;
    using Value = boost::json::value;

    static std::uint64_t UInt(const Value& v) {
        if (v.is_uint64()) { return v.as_uint64(); }
        if (v.is_int64() && v.as_int64() >= 0) {
            return static_cast<std::uint64_t>(v.as_int64());
        }
        throw std::runtime_error("Expected unsigned integer in saved state");
    }

    static double Number(const Value& v) {
        const double result = boost::json::value_to<double>(v);
        if (!std::isfinite(result)) { throw std::runtime_error("Non-finite state coordinate"); }
        return result;
    }

    static std::string String(const Value& v) {
        return boost::json::value_to<std::string>(v);
    }

    static unsigned Type(const Value& v, const model::Map& map) {
        const auto type = UInt(v);
        if (type >= map.GetLootTypesCount()) { throw std::runtime_error("Invalid saved loot type"); }
        return static_cast<unsigned>(type);
    }

    template <typename Engine>
    static std::string EngineState(const Engine& engine) {
        std::ostringstream out;
        out << engine;
        return out.str();
    }

    template <typename Engine>
    static void RestoreEngine(Engine& engine, const Value& v) {
        std::istringstream input{String(v)};
        if (!(input >> engine)) { throw std::runtime_error("Invalid random engine state"); }
        input >> std::ws;
        if (!input.eof()) { throw std::runtime_error("Trailing random engine data"); }
    }

    static Object Capture(const app::Application& app) {
        Array sessions;
        for (const auto& [map_id, ptr] : app.sessions_) {
            const auto& session = *ptr;
            Array dogs;
            for (const auto& dog : session.dogs_) {
                Array bag;
                for (const auto& item : dog.bag_) {
                    bag.emplace_back(Object{{"id", item.id}, {"type", item.type}});
                }
                dogs.emplace_back(Object{
                    {"id", *dog.id_}, {"name", dog.name_},
                    {"x", dog.position_.x}, {"y", dog.position_.y},
                    {"vx", dog.speed_.x}, {"vy", dog.speed_.y},
                    {"direction", static_cast<unsigned>(dog.direction_)},
                    {"score", dog.score_}, {"bag", std::move(bag)}});
            }
            Array loot;
            for (const auto& item : session.lost_objects_) {
                loot.emplace_back(Object{{"id", item.id}, {"type", item.type},
                    {"x", item.position.x}, {"y", item.position.y}});
            }
            sessions.emplace_back(Object{
                {"mapId", map_id}, {"dogs", std::move(dogs)}, {"loot", std::move(loot)},
                {"nextDogId", session.next_dog_id_}, {"nextLootId", session.next_loot_id_},
                {"randomEngine", EngineState(session.random_generator_)},
                {"lootElapsed", session.loot_generator_.time_without_loot_.count()}});
        }
        Array players;
        for (const auto& [id, player] : app.players_) {
            players.emplace_back(Object{{"id", *id},
                {"mapId", *player.GetSession().GetMap().GetId()},
                {"dogId", *player.GetDog().GetId()}});
        }
        Array tokens;
        for (const auto& [token, player] : app.tokens_.token_to_player_) {
            tokens.emplace_back(Object{{"token", *token}, {"playerId", *player->GetId()}});
        }
        return Object{{"version", 1}, {"sessions", std::move(sessions)},
            {"players", std::move(players)}, {"tokens", std::move(tokens)},
            {"nextPlayerId", app.players_.next_player_id_}};
    }

    static void Restore(app::Application& app, const Object& root) {
        if (UInt(root.at("version")) != 1) { throw std::runtime_error("Unsupported state version"); }
        for (const auto& sv : root.at("sessions").as_array()) {
            const auto& s = sv.as_object();
            const auto map_id = String(s.at("mapId"));
            const auto* map = app.game_.FindMap(model::Map::Id{map_id});
            if (!map) { throw std::runtime_error("Saved map not found: " + map_id); }
            if (app.sessions_.contains(map_id)) { throw std::runtime_error("Duplicate saved session"); }
            auto& session = app.GetOrCreateSession(*map);
            session.next_dog_id_ = UInt(s.at("nextDogId"));
            session.next_loot_id_ = UInt(s.at("nextLootId"));
            RestoreEngine(session.random_generator_, s.at("randomEngine"));
            const auto elapsed = UInt(s.at("lootElapsed"));
            if (elapsed > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                throw std::runtime_error("Invalid loot generator elapsed time");
            }
            session.loot_generator_.time_without_loot_ = std::chrono::milliseconds{
                static_cast<std::int64_t>(elapsed)};
            std::unordered_set<std::uint64_t> dog_ids;
            std::unordered_set<std::uint64_t> loot_ids;
            for (const auto& dv : s.at("dogs").as_array()) {
                const auto& d = dv.as_object();
                const auto id = UInt(d.at("id"));
                if (id >= session.next_dog_id_ || !dog_ids.insert(id).second) {
                    throw std::runtime_error("Invalid or duplicate saved dog ID");
                }
                auto name = String(d.at("name"));
                if (name.empty()) { throw std::runtime_error("Empty saved dog name"); }
                auto& dog = session.dogs_.emplace_back(model::DogId{id}, std::move(name),
                    model::Position{Number(d.at("x")), Number(d.at("y"))});
                dog.speed_ = {Number(d.at("vx")), Number(d.at("vy"))};
                const auto direction = UInt(d.at("direction"));
                if (direction > static_cast<unsigned>(model::Direction::EAST)) {
                    throw std::runtime_error("Invalid saved dog direction");
                }
                dog.direction_ = static_cast<model::Direction>(direction);
                dog.score_ = UInt(d.at("score"));
                const auto& bag = d.at("bag").as_array();
                if (bag.size() > map->GetBagCapacity()) { throw std::runtime_error("Saved bag is too large"); }
                for (const auto& bv : bag) {
                    const auto& b = bv.as_object();
                    const auto loot_id = UInt(b.at("id"));
                    if (loot_id >= session.next_loot_id_ || !loot_ids.insert(loot_id).second) {
                        throw std::runtime_error("Invalid saved bag item ID");
                    }
                    dog.bag_.push_back({loot_id, Type(b.at("type"), *map)});
                }
            }
            for (const auto& lv : s.at("loot").as_array()) {
                const auto& l = lv.as_object();
                const auto id = UInt(l.at("id"));
                if (id >= session.next_loot_id_ || !loot_ids.insert(id).second) {
                    throw std::runtime_error("Invalid or duplicate saved loot ID");
                }
                session.lost_objects_.push_back({id, Type(l.at("type"), *map),
                    {Number(l.at("x")), Number(l.at("y"))}});
            }
        }
        app.players_.next_player_id_ = UInt(root.at("nextPlayerId"));
        std::unordered_map<std::string, std::unordered_set<std::uint64_t>> owned_dogs;
        for (const auto& pv : root.at("players").as_array()) {
            const auto& p = pv.as_object();
            const auto id = UInt(p.at("id"));
            const auto map_id = String(p.at("mapId"));
            const auto dog_id = UInt(p.at("dogId"));
            if (id >= app.players_.next_player_id_ || !app.sessions_.contains(map_id)) {
                throw std::runtime_error("Invalid saved player");
            }
            auto& session = *app.sessions_.at(map_id);
            auto dog = std::find_if(session.dogs_.begin(), session.dogs_.end(),
                [dog_id](const auto& d) { return *d.GetId() == dog_id; });
            if (dog == session.dogs_.end() || !owned_dogs[map_id].insert(dog_id).second) {
                throw std::runtime_error("Invalid saved player-dog relationship");
            }
            const model::PlayerId player_id{id};
            if (!app.players_.players_.emplace(player_id,
                    model::Player{player_id, session, *dog}).second) {
                throw std::runtime_error("Duplicate saved player ID");
            }
        }
        std::unordered_set<std::uint64_t> authorized;
        for (const auto& tv : root.at("tokens").as_array()) {
            const auto& t = tv.as_object();
            auto token = String(t.at("token"));
            if (token.size() != 32 || !std::all_of(token.begin(), token.end(),
                    [](unsigned char c) { return std::isxdigit(c) != 0; })) {
                throw std::runtime_error("Invalid saved token");
            }
            const auto id = UInt(t.at("playerId"));
            auto* player = app.players_.FindById(model::PlayerId{id});
            if (!player || !authorized.insert(id).second ||
                !app.tokens_.token_to_player_.emplace(model::Token{std::move(token)}, player).second) {
                throw std::runtime_error("Invalid or duplicate saved authorization");
            }
        }
        if (authorized.size() != app.players_.players_.size()) {
            throw std::runtime_error("Saved player has no token");
        }
        for (const auto& [map_id, session] : app.sessions_) {
            if (owned_dogs[map_id].size() != session->dogs_.size()) {
                throw std::runtime_error("Saved dog has no player");
            }
        }
    }

public:
    static void Save(const app::Application& app, const std::filesystem::path& path) {
        const auto content = boost::json::serialize(Capture(app));
        auto temp = path;
        temp += ".tmp";
        try {
            std::ofstream output;
            output.exceptions(std::ios::failbit | std::ios::badbit);
            output.open(temp, std::ios::binary | std::ios::trunc);
            output << content;
            output.flush();
            output.close();
            std::filesystem::rename(temp, path);
        } catch (...) {
            std::error_code ignored;
            std::filesystem::remove(temp, ignored);
            throw;
        }
    }

    static void Load(app::Application& app, const std::filesystem::path& path) {
        // Missing is the only condition treated as a fresh game.
        if (!std::filesystem::exists(path)) { return; }
        std::ifstream input{path, std::ios::binary};
        if (!input) { throw std::runtime_error("Cannot open saved state: " + path.string()); }
        const std::string content{std::istreambuf_iterator<char>{input},
                                  std::istreambuf_iterator<char>{}};
        if (input.bad()) { throw std::runtime_error("Cannot read saved state"); }
        const auto value = boost::json::parse(content);
        app::Application restored{app.game_, app.randomize_spawn_points_};
        Restore(restored, value.as_object());
        // The nodes and sessions keep their addresses, preserving all pointers.
        app.sessions_.swap(restored.sessions_);
        app.players_.players_.swap(restored.players_.players_);
        std::swap(app.players_.next_player_id_, restored.players_.next_player_id_);
        app.tokens_.token_to_player_.swap(restored.tokens_.token_to_player_);
        app.elapsed_since_save_ = std::chrono::milliseconds{0};
    }
};

}  // namespace serialization

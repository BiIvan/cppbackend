#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <cstdint>
#include <utility>
#include <optional>
#include <stdexcept>
#include <functional>
#include <unordered_map>

#include "model.h"
#include "player.h"
#include "literals.h"

using GS = model::GameSession;

namespace app {

  class Application {
    friend class serialization::StateSerializer;
    std::function<void()> save_state_;
    std::optional<std::chrono::milliseconds> save_period_;
    std::chrono::milliseconds elapsed_since_save_{0};
    GS& GetOrCreateSession(const model::Map& map) {
      const std::string map_id { *map.GetId()};
      if (auto it = sessions_.find(map_id);
        it != sessions_.end()) {
        return *it->second;
      }
      auto session = std::make_unique<GS>( &map, randomize_spawn_points_, game_.GetLootGeneratorConfig());
      GS* result = session.get();
      sessions_.emplace(map_id, std::move(session));
      return *result;
    }

    model::Game& game_;
    model::Players players_;
    model::PlayerTokens tokens_;
    bool randomize_spawn_points_ = false;
    std::unordered_map<std::string,std::unique_ptr<GS>> sessions_;

  public:
    explicit Application( model::Game& game, bool randomize_spawn_points)
      : game_(game)
      , randomize_spawn_points_(randomize_spawn_points) {
    }

    void ConfigureSaving( std::function<void()> save, std::optional<std::chrono::milliseconds> period) {
      if (period && period->count() < 0) {
        throw std::invalid_argument(
            "Save period must be nonnegative");
      }
      save_state_ = std::move(save);
      save_period_ = period;
      elapsed_since_save_ = std::chrono::milliseconds{0};
    }

    void Tick(std::int64_t delta_ms) {
      using namespace std::chrono;
      if (delta_ms < 0) {
        throw std::invalid_argument("Negative tick interval");
      }
      for (auto& [map_id, session] : sessions_) {
        session->Tick(milliseconds{delta_ms});
      }
      if (!save_state_ || !save_period_) {
        return;
      }
      const milliseconds period = *save_period_;
      const milliseconds remaining = period - elapsed_since_save_;
      if (delta_ms < remaining.count()) {
        elapsed_since_save_ += milliseconds{delta_ms};
        return;
      }
      // Период уже достигнут. При ошибке записи следующий тик
      // снова попытается сохранить состояние.
      elapsed_since_save_ = period;
      save_state_();
      // Сбрасываем счётчик только после успешной записи.
      elapsed_since_save_ = milliseconds{0};
    }

    struct JoinResult {
      model::PlayerId player_id;
      model::Token token;
    };

    JoinResult JoinGame( const model::Map::Id& map_id, std::string user_name) {
      const model::Map* map{ game_.FindMap(map_id)};
      if (map == nullptr) {
        throw std::out_of_range(std::string{NOMAPstr});
      }
      GS& session = GetOrCreateSession(*map);
      model::Dog& dog = session.AddDog(std::move(user_name));
      model::Player& player = players_.Add(dog, session);
      model::Token token = tokens_.AddPlayer(player);
      return {player.GetId(), std::move(token)};
    }

    model::Player* FindPlayerByToken( const model::Token& token) const noexcept {
      return tokens_.FindPlayerByToken(token);
    }

    const model::Players& GetPlayers() const noexcept {
      return players_;
    }
  };

}  // namespace app

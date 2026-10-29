#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include <pqxx/pqxx>

namespace database {
struct Record {
    std::string name;
    std::uint64_t score;
    double play_ms;
};
class Records {
    std::string url_;
public:
    explicit Records(std::string url) : url_(std::move(url)) {
        pqxx::connection connection{url_};
        pqxx::work tx{connection};
        tx.exec(R"(
            CREATE TABLE IF NOT EXISTS retired_players (
                id UUID PRIMARY KEY,
                name TEXT NOT NULL,
                score NUMERIC(20,0) NOT NULL CHECK (score >= 0),
                play_time DOUBLE PRECISION NOT NULL CHECK (play_time >= 0)
            );
            CREATE INDEX IF NOT EXISTS retired_players_ranking_idx
                ON retired_players (score DESC, play_time ASC, name COLLATE "C" ASC, id ASC);
        )");
        tx.commit();
    }
    void Save(const std::string& id, const Record& record) const {
        pqxx::connection connection{url_};
        pqxx::work tx{connection};
        tx.exec_params(R"(
            INSERT INTO retired_players (id, name, score, play_time)
            VALUES ($1::uuid, $2, $3::numeric, $4)
            ON CONFLICT (id) DO NOTHING
        )", id, record.name, std::to_string(record.score), record.play_ms);
        tx.commit();
    }
    std::vector<Record> Read(std::int64_t start, std::int64_t count) const {
        pqxx::connection connection{url_};
        pqxx::read_transaction tx{connection};
        auto rows = tx.exec_params(R"(
            SELECT name, score, play_time FROM retired_players
            ORDER BY score DESC, play_time ASC, name COLLATE "C" ASC, id ASC
            LIMIT $1 OFFSET $2
        )", count, start);
        std::vector<Record> result;
        result.reserve(rows.size());
        for (const auto& row : rows) {
            result.push_back({row[0].as<std::string>(), row[1].as<std::uint64_t>(), row[2].as<double>()});
        }
        return result;
    }
};
}

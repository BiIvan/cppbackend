#pragma once

#include <set>
#include <random>
#include <vector>
#include <cassert>
#include <cstddef>
#include <iomanip>
#include <utility>
#include <iostream>
#include <optional>

class SeabattleField {
  size_t width_;
  size_t height_;
  int weight_;
  
public:
  enum class State { UNKNOWN, EMPTY, KILLED, SHIP };
  enum class ShotResult { MISS = 0, HIT = 1, KILL = 2 };

  SeabattleField( size_t width, size_t height, State default_elem = State::UNKNOWN)
    : width_(width)
    , height_(height)
    , field_(width * height, default_elem)
    , weight_(0) {
    assert(width_ > 0);
    assert(height_ > 0);
  }
  
  size_t GetWidth() const {
    return width_;
  }
  
  size_t GetHeight() const {
    return height_;
  }
  
  template <class T>
  static SeabattleField GetRandomField(size_t width,size_t height,T&& random_engine) {
    std::optional<SeabattleField> result;
    do {
      result = TryGetRandomField(width, height, random_engine);
    } while (!result);
    return *result;
  }
  
  ShotResult Shoot(size_t x, size_t y) {
    assert(IsInside(x, y));
    if (Get(x, y) != State::SHIP) {
      return ShotResult::MISS;
    }
    Get(x, y) = State::KILLED;
    --weight_;
    return IsKilled(x, y) ? ShotResult::KILL : ShotResult::HIT;
  }
  
  void MarkMiss(size_t x, size_t y) {
    assert(IsInside(x, y));
    if (Get(x, y) != State::UNKNOWN) {
      return;
    }
    Get(x, y) = State::EMPTY;
  }
  
  void MarkHit(size_t x, size_t y) {
    assert(IsInside(x, y));
    if (Get(x, y) != State::UNKNOWN) {
      return;
    }
    --weight_;
    Get(x, y) = State::KILLED;
  }
  
  void MarkKill(size_t x, size_t y) {
    assert(IsInside(x, y));
    if (Get(x, y) != State::UNKNOWN) {
      return;
    }
    MarkHit(x, y);
    MarkKillInDirection(x, y, 1, 0);
    MarkKillInDirection(x, y, -1, 0);
    MarkKillInDirection(x, y, 0, 1);
    MarkKillInDirection(x, y, 0, -1);
  }
  
  State operator()(size_t x, size_t y) const {
    assert(IsInside(x, y));
    return Get(x, y);
  }
  
  bool IsKilled(size_t x, size_t y) const {
    return IsKilledInDirection(x, y, 1, 0)
      && IsKilledInDirection(x, y, -1, 0)
      && IsKilledInDirection(x, y, 0, 1)
      && IsKilledInDirection(x, y, 0, -1);
  }
  
  bool IsLoser() const {
    return weight_ == 0;
  }
  
  int GetWeight() const {
    return weight_;
  }
  
  void PrintDigitLine(std::ostream& out) const {
    out << "   ";
    for (size_t x = 0; x < width_; ++x) {
      out << std::setw(3) << (x + 1);
    }
  }
  
  void PrintLine(std::ostream& out, size_t y) const {
    assert(y < height_);
    const char row_char = static_cast<char>('A' + y);
    out << row_char << "  ";
    for (size_t x = 0; x < width_; ++x) {
      out << std::setw(3) << Repr((*this)(x, y));
    }
    out << "  " << row_char;
  }
  
private:
  template <class T>
  static std::optional<SeabattleField> TryGetRandomField( size_t width,size_t height,T&& random_engine) {
    SeabattleField result(width, height, State::EMPTY);
    const auto ship_sizes = GetShipSizes(width, height);
    result.weight_ = 0;
    for (const size_t ship_size : ship_sizes) {
      result.weight_ += static_cast<int>(ship_size);
    }
    std::set<std::pair<size_t, size_t>> available_cells;
    for (size_t y = 0; y < height; ++y) {
      for (size_t x = 0; x < width; ++x) {
        available_cells.insert({x, y});
      }
    }
    const auto is_inside = [width, height](int x, int y) {
      return x >= 0 && y >= 0
        && x < static_cast<int>(width)
        && y < static_cast<int>(height);
    };
    const auto mark_cell_as_used = [&](size_t x, size_t y) {
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          const int nx{ static_cast<int>(x) + dx};
          const int ny{ static_cast<int>(y) + dy};
          if (is_inside(nx, ny)) {
            available_cells.erase({
              static_cast<size_t>(nx),
              static_cast<size_t>(ny)
            });
          }
        }
      }
    };
    const auto direction_to_delta = [](size_t direction) {
      switch (direction) {
        case 0:
          return std::pair<int, int>{0, 1};
        case 1:
          return std::pair<int, int>{1, 0};
        case 2:
          return std::pair<int, int>{0, -1};
        default:
          return std::pair<int, int>{-1, 0};
      }
    };
    const auto check_ship_available =
      [&](size_t x, size_t y, size_t direction, size_t length) {
        const auto [dx, dy] = direction_to_delta(direction);
        for (size_t i = 0; i < length; ++i) {
          const int cx{ static_cast<int>(x) + dx * static_cast<int>(i)};
        const int cy{ static_cast<int>(y) + dy * static_cast<int>(i)};
          if (!is_inside(cx, cy)) {
            return false;
          }
          const auto cell = std::pair<size_t, size_t>{
            static_cast<size_t>(cx),
            static_cast<size_t>(cy)
          };
          if (available_cells.count(cell) == 0) {
            return false;
          }
        }
        return true;
      };
    const auto mark_ship =
      [&](size_t x, size_t y, size_t direction, size_t length) {
        const auto [dx, dy] = direction_to_delta(direction);
        for (size_t i = 0; i < length; ++i) {
          const size_t cx = static_cast<size_t>(
            static_cast<int>(x) + dx * static_cast<int>(i));
          const size_t cy = static_cast<size_t>(
            static_cast<int>(y) + dy * static_cast<int>(i));
          result.Get(cx, cy) = State::SHIP;
          mark_cell_as_used(cx, cy);
        }
      };
    using Distribution = std::uniform_int_distribution<size_t>;
    using Params = Distribution::param_type;
    constexpr int max_attempts{ 2'000};
    for (const size_t ship_size : ship_sizes) {
      std::pair<size_t, size_t> position;
      size_t direction{ 0};
      int attempts{ 0};
      Distribution distribution;
      do {
        if (attempts++ >= max_attempts || available_cells.empty()) {
          return std::nullopt;
        }
        const size_t index = distribution(
          random_engine, Params(0, available_cells.size() - 1));
        position = *std::next(available_cells.begin(), index);
        direction = distribution(
          random_engine, Params(0, 3));
      } while (!check_ship_available(
        position.first,
        position.second,
        direction,
        ship_size));
      mark_ship(position.first,position.second,direction,ship_size);
    }
    return result;
  }
  
  static std::vector<size_t> GetShipSizes(size_t width,size_t height) {
    const size_t area = width * height;
    if (area <= 100) {
      return {
        4,
        3, 3,
        2, 2, 2,
        1, 1, 1, 1
      };
    }
    if (area <= 144) {
      return {
        5,
        4, 4,
        3, 3, 3,
        2, 2, 2, 2,
        1, 1, 1, 1, 1
      };
    }
    return {
      5, 5,
      4, 4, 4,
      3, 3, 3, 3,
      2, 2, 2, 2, 2,
      1, 1, 1, 1, 1, 1
    };
  }
  
  bool IsInside(size_t x, size_t y) const {
    return x < width_ && y < height_;
  }
  
  bool IsKilledInDirection(size_t x,size_t y,int dx,int dy) const {
    int cx{ static_cast<int>(x)};
    int cy{ static_cast<int>(y)};
    while (cx >= 0 && cy >= 0 && cx < static_cast<int>(width_) && cy < static_cast<int>(height_)) {
      const State state = Get( static_cast<size_t>(cx), static_cast<size_t>(cy));
      if (state == State::EMPTY) {
        return true;
      }
      if (state != State::KILLED) {
        return false;
      }
      cx += dx;
      cy += dy;
    }
    return true;
  }
  
  void MarkKillInDirection(size_t x,size_t y,int dx,int dy) {
    int cx{ static_cast<int>(x)};
    int cy{ static_cast<int>(y)};
    while (cx >= 0 && cy >= 0 && cx < static_cast<int>(width_) && cy < static_cast<int>(height_)) {
      MarkCellEmpty(cx + dy, cy + dx);
      MarkCellEmpty(cx - dy, cy - dx);
      MarkCellEmpty(cx, cy);
      if (Get( static_cast<size_t>(cx), static_cast<size_t>(cy)) != State::KILLED) {
        return;
      }
      cx += dx;
      cy += dy;
    }
  }
  
  void MarkCellEmpty(int x, int y) {
    if (x < 0 || y < 0 || x >= static_cast<int>(width_) || y >= static_cast<int>(height_)) {
      return;
    }
    auto& cell{ Get( static_cast<size_t>(x), static_cast<size_t>(y))};
    if (cell == State::UNKNOWN) {
      cell = State::EMPTY;
    }
  }
  
  State& Get(size_t x, size_t y) {
    assert(IsInside(x, y));
    return field_[x + y * width_];
  }
  
  State Get(size_t x, size_t y) const {
    assert(IsInside(x, y));
    return field_[x + y * width_];
  }
  
  static char Repr(State state) {
    switch (state) {
      case State::UNKNOWN:
        return '?';
      case State::EMPTY:
        return '.';
      case State::KILLED:
        return 'x';
      case State::SHIP:
        return 'o';
    }
    return '?';
  }
  
  std::vector<State> field_;

};
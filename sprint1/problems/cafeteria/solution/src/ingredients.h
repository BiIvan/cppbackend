#pragma once

#include <optional>
#include <functional>

#include "clock.h"
#include "gascooker.h"

class Sausage : public std::enable_shared_from_this<Sausage> {
  int id_;
  GasCookerLock gas_cooker_lock_;
  std::optional<Clock::time_point> frying_start_time_;
  std::optional<Clock::time_point> frying_end_time_;
  
public:
  using Handler = std::function<void()>;

  explicit Sausage(int id)
    : id_{id} {
  }

  int GetId() const {
    return id_;
  }

  void StartFry(GasCooker& cooker, Handler handler) {
    if (frying_start_time_) {
      throw std::logic_error("Frying already started");
    }
    frying_start_time_ = Clock::now();
    gas_cooker_lock_ = GasCookerLock{cooker.shared_from_this()};
    cooker.UseBurner(
      [self = shared_from_this(), handler = std::move(handler)] {
        self->frying_start_time_ = Clock::now();
        handler();}
    );
  }

  void StopFry() {
    if (!frying_start_time_) {
      throw std::logic_error("Frying has not started");
    }
    if (frying_end_time_) {
      throw std::logic_error("Frying has already stopped");
    }
    frying_end_time_ = Clock::now();
    gas_cooker_lock_.Unlock();
  }

  bool IsCooked() const noexcept {
    return frying_start_time_.has_value() && frying_end_time_.has_value();
  }

  Clock::duration GetCookDuration() const {
    if (!frying_start_time_ || !frying_end_time_) {
      throw std::logic_error("Sausage has not been cooked");
    }
    return *frying_end_time_ - *frying_start_time_;
  }
};

class Bread : public std::enable_shared_from_this<Bread> {
  int id_;
  GasCookerLock gas_cooker_lock_;
  std::optional<Clock::time_point> baking_start_time_;
  std::optional<Clock::time_point> baking_end_time_;
  
public:
  using Handler = std::function<void()>;

  explicit Bread(int id)
    : id_{id} {
  }

  int GetId() const {
    return id_;
  }

  void StartBake(GasCooker& cooker, Handler handler) {
    if (baking_start_time_) {
      throw std::logic_error("Baking already started");
    }
    baking_start_time_ = Clock::now();
    gas_cooker_lock_ = GasCookerLock{cooker.shared_from_this()};
    cooker.UseBurner(
      [self = shared_from_this(), handler = std::move(handler)] {
        self->baking_start_time_ = Clock::now();
        handler();}
    );
  }

  void StopBaking() {
    if (!baking_start_time_) {
      throw std::logic_error("Baking has not started");
    }
    if (baking_end_time_) {
      throw std::logic_error("Baking has already stopped");
    }
    baking_end_time_ = Clock::now();
    gas_cooker_lock_.Unlock();
  }

  bool IsCooked() const noexcept {
    return baking_start_time_.has_value() && baking_end_time_.has_value();
  }

  Clock::duration GetBakingDuration() const {
    if (!baking_start_time_ || !baking_end_time_) {
      throw std::logic_error("Bread has not been baked");
    }
    return *baking_end_time_ - *baking_start_time_;
  }
};

class Store {
  int next_id_ = 0;
  
public:
  std::shared_ptr<Bread> GetBread() {
    return std::make_shared<Bread>(++next_id_);
  }

  std::shared_ptr<Sausage> GetSausage() {
    return std::make_shared<Sausage>(++next_id_);
  }
};

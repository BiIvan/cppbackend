#pragma once
//2026.10.30 13:14

#include <chrono>
#include <utility>
#include <functional>

namespace serialization { class StateSerializer; }

namespace loot_gen {

  using RandomGenerator = std::function<double()>;
  using TimeInterval = std::chrono::milliseconds;
  
  class LootGenerator {
    friend class serialization::StateSerializer;
    
    static double DefaultGenerator() noexcept {
      return 1.0;
    };
    
    TimeInterval base_interval_;
    double probability_;
    TimeInterval time_without_loot_{};
    RandomGenerator random_generator_;
    
  public:

    LootGenerator(TimeInterval base_interval, double probability,
                  RandomGenerator random_gen = DefaultGenerator)
      : base_interval_{base_interval}
      , probability_{probability}
      , random_generator_{std::move(random_gen)} {
    }

    unsigned Generate(TimeInterval time_delta, unsigned loot_count, unsigned looter_count);
  };
}  // namespace loot_gen

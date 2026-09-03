#pragma once

#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/strand.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/bind_executor.hpp>

#include <mutex>
#include <chrono>
#include <memory>
#include <utility>
#include <exception>
#include <stdexcept>
#include <functional>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;

using namespace std::chrono_literals;
using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

class Cafeteria {
  class Order : public std::enable_shared_from_this<Order> {
    void OnBreadStarted() {
      if (completed_) {
        StopBreadNoThrow();
        return;
      }
      bread_started_ = true;
      try {
        bread_timer_.expires_after(1s);
        auto self = shared_from_this();
        bread_timer_.async_wait(
          net::bind_executor(
            strand_,
            [self](const boost::system::error_code& ec) {
              self->OnBreadTimer(ec);
            }));
      } catch (...) {
        CompleteWithError(std::current_exception());
      }
    }

    void OnSausageStarted() {
      if (completed_) {
        StopSausageNoThrow();
        return;
      }
      sausage_started_ = true;
      try {
        sausage_timer_.expires_after(1500ms);
        auto self = shared_from_this();
        sausage_timer_.async_wait(
          net::bind_executor(
            strand_,
            [self](const boost::system::error_code& ec) {
              self->OnSausageTimer(ec);
            }));
      } catch (...) {
        CompleteWithError(std::current_exception());
      }
    }

    void OnBreadTimer(const boost::system::error_code& ec) {
      if (completed_) {
        return;
      }
      if (ec) {
        CompleteWithError(
          std::make_exception_ptr(
            std::runtime_error{"Bread timer error: " + ec.message()}));
        return;
      }
      try {
        bread_->StopBaking();
        bread_ready_ = true;
        TryMakeHotDog();
      } catch (...) {
        CompleteWithError(std::current_exception());
      }
    }

    void OnSausageTimer(const boost::system::error_code& ec) {
      if (completed_) {
        return;
      }
      if (ec) {
        CompleteWithError(
          std::make_exception_ptr(
            std::runtime_error{"Sausage timer error: " + ec.message()}));
        return;
      }
      try {
        sausage_->StopFry();
        sausage_ready_ = true;
        TryMakeHotDog();
      } catch (...) {
        CompleteWithError(std::current_exception());
      }
    }

    void TryMakeHotDog() {
      if (completed_ || !bread_ready_ || !sausage_ready_) {
        return;
      }
      completed_ = true;
      try {
        CallHandler(Result<HotDog>{
          HotDog{
            hotdog_id_,
            std::move(sausage_),
            std::move(bread_)
          }
        });
      } catch (...) {
        CallHandler(Result<HotDog>::FromCurrentException());
      }
    }

    void CompleteWithError(std::exception_ptr error) {
      if (completed_) {
        return;
      }
      completed_ = true;
      try {
        bread_timer_.cancel();
      } catch (...) {
      }
      try {
        sausage_timer_.cancel();
      } catch (...) {
      }
      if (bread_started_ && !bread_ready_) {
        StopBreadNoThrow();
      }
      if (sausage_started_ && !sausage_ready_) {
        StopSausageNoThrow();
      }
      CallHandler(Result<HotDog>{std::move(error)});
    }

    void StopBreadNoThrow() noexcept {
      try {
        if (bread_ && !bread_->IsCooked()) {
          bread_->StopBaking();
        }
      } catch (...) {
      }
    }

    void StopSausageNoThrow() noexcept {
      try {
        if (sausage_ && !sausage_->IsCooked()) {
          sausage_->StopFry();
        }
      } catch (...) {
      }
    }

    void CallHandler(Result<HotDog> result) noexcept {
      try {
        handler_(std::move(result));
      } catch (...) {
      }
    }
    net::strand<net::io_context::executor_type> strand_;
    int hotdog_id_{ 0};
    std::shared_ptr<Bread> bread_;
    std::shared_ptr<Sausage> sausage_;
    net::steady_timer bread_timer_;
    net::steady_timer sausage_timer_;
    HotDogHandler handler_;
    bool bread_started_{ false};
    bool sausage_started_{ false};
    bool bread_ready_{ false};
    bool sausage_ready_{ false};
    bool completed_{ false};  
    
  public:
    Order(net::io_context& io,
        int hotdog_id,
        std::shared_ptr<Bread> bread,
        std::shared_ptr<Sausage> sausage,
        HotDogHandler handler)
      : strand_{net::make_strand(io)}
      , hotdog_id_{hotdog_id}
      , bread_{std::move(bread)}
      , sausage_{std::move(sausage)}
      , bread_timer_{io}
      , sausage_timer_{io}
      , handler_{std::move(handler)} {
    }

    void Start(GasCooker& cooker) {
      auto self = shared_from_this();
      try {
        bread_->StartBake(
          cooker,
          net::bind_executor(
            strand_,
            [self] {
              self->OnBreadStarted();
            }));
        sausage_->StartFry(
          cooker,
          net::bind_executor(
            strand_,
            [self] {
              self->OnSausageStarted();
            }));
      } catch (...) {
        CompleteWithError(std::current_exception());
      }
    }
  };

  net::io_context& io_;
  std::mutex store_mutex_;
  Store store_;
  int next_hotdog_id_{ 0};
  std::shared_ptr<GasCooker> gas_cooker_{ std::make_shared<GasCooker>(io_)};

public:
  explicit Cafeteria(net::io_context& io)
    : io_{io} {
  }

  void OrderHotDog(HotDogHandler handler) {
    try {
      std::shared_ptr<Bread> bread;
      std::shared_ptr<Sausage> sausage;
      int hotdog_id = 0;
      {
        std::lock_guard lock{store_mutex_};
        bread = store_.GetBread();
        sausage = store_.GetSausage();
        hotdog_id = ++next_hotdog_id_;
      }
      auto order = std::make_shared<Order>(
        io_,
        hotdog_id,
        std::move(bread),
        std::move(sausage),
        std::move(handler));
      order->Start(*gas_cooker_);
    } catch (...) {
      try {
        handler(Result<HotDog>::FromCurrentException());
      } catch (...) {
      }
    }
  }
};
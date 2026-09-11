#ifdef WIN32
#include <sdkddkver.h>
#endif

#include <chrono>
#include <memory>
#include <cassert>
#include <sstream>
#include <iostream>
#include <syncstream>
#include <unordered_map>
#include <boost/asio.hpp>
namespace net = boost::asio;
namespace sys = boost::system;

using namespace std::chrono;
using namespace std::literals;
using Timer = net::steady_timer;
namespace ph = std::placeholders;

class Hamburger {
  void AssureCutletRoasted() const {
    if (!cutlet_roasted_) {
      throw std::logic_error("Cutlet has not been roasted yet"s);
    }
  }

  bool cutlet_roasted_{ false};  
  bool has_onion_{ false};     
  bool is_packed_{ false};     
  
public:
  [[nodiscard]] bool IsCutletRoasted() const {
    return cutlet_roasted_;
  }
  
  void SetCutletRoasted() {
    if (IsCutletRoasted()) {
      throw std::logic_error("Cutlet has been roasted already"s);
    }
    cutlet_roasted_ = true;
  }

  [[nodiscard]] bool HasOnion() const {
    return has_onion_;
  }

  void AddOnion() {
    if (IsPacked()) {
      throw std::logic_error("Hamburger has been packed already"s);
    }
    AssureCutletRoasted();
    has_onion_ = true;
  }

  [[nodiscard]] bool IsPacked() const {
    return is_packed_;
  }
  
  void Pack() {
    AssureCutletRoasted();
    is_packed_ = true;
  }
};

std::ostream& operator<<(std::ostream& os, const Hamburger& h) {
  return os << "Hamburger: "sv << (h.IsCutletRoasted() ? "roasted cutlet"sv : " raw cutlet"sv)
        << (h.HasOnion() ? ", onion"sv : ""sv)
        << (h.IsPacked() ? ", packed"sv : ", not packed"sv);
} 

class Logger {
  std::string id_;
  steady_clock::time_point start_time_{steady_clock::now()};
  
public:
  explicit Logger(std::string id)
    : id_(std::move(id)) {
  }

  void LogMessage(std::string_view message) const {
    std::osyncstream os{std::cout};
    os << id_ << "> ["sv << duration<double>(steady_clock::now() - start_time_).count()
       << "s] "sv << message << std::endl;
  }
};

using OrderHandler = std::function<void(sys::error_code ec, int id, Hamburger* hamburger)>;

class Order : public std::enable_shared_from_this<Order> {
  void RoastCutlet() {
    logger_.LogMessage("Start roasting cutlet"sv);
    roast_timer_.async_wait(
      [self = shared_from_this()](sys::error_code ec) {
        self->OnRoasted(ec);
      });
  }

  void OnRoasted(sys::error_code ec) {
    if (ec) {
      logger_.LogMessage("Roast error: "s + ec.message());
      return CheckReadiness(ec);
    }
    hamburger_.SetCutletRoasted();
    logger_.LogMessage("Cutlet has been roasted"sv);
    CheckReadiness({});
  }

  void MarinadeOnion() {
    logger_.LogMessage("Start marinading onion"sv);
    marinade_timer_.async_wait(
      [self = shared_from_this()](sys::error_code ec) {
        self->OnOnionMarinaded(ec);
      });
  }

  void OnOnionMarinaded(sys::error_code ec) {
    if (ec) {
      logger_.LogMessage("Marinade onion error: "s + ec.message());
      return CheckReadiness(ec);
    }
    onion_marinaded_ = true;
    logger_.LogMessage("Onion has been marinaded"sv);
    CheckReadiness({});
  }

  void CheckReadiness(sys::error_code ec) {
    if (delivered_) {
      return;
    }
    if (ec) {
      Deliver(ec);
      return;
    }
    if (CanAddOnion()) {
      hamburger_.AddOnion();
      logger_.LogMessage("Onion has been added"sv);
    }
    if (IsReadyToPack()) {
      Pack();
    }
  }

  void Pack() {
    logger_.LogMessage("Packing"sv);
    auto start = steady_clock::now();
    while (steady_clock::now() - start < 500ms) {
    }
    hamburger_.Pack();
    logger_.LogMessage("Packed"sv);
    Deliver({});
  }

  void Deliver(sys::error_code ec) {
    if (delivered_) {
      return;
    }
    delivered_ = true;
    handler_(ec, id_, ec ? nullptr : &hamburger_);
  }

  [[nodiscard]] bool CanAddOnion() const {
    return hamburger_.IsCutletRoasted()
      && onion_marinaded_
      && !hamburger_.HasOnion();
  }

  [[nodiscard]] bool IsReadyToPack() const {
    return hamburger_.IsCutletRoasted()
      && (!with_onion_ || hamburger_.HasOnion());
  }

  int id_;
  bool with_onion_;
  OrderHandler handler_;
  Logger logger_;
  Timer roast_timer_;
  Timer marinade_timer_;
  Hamburger hamburger_;
  bool onion_marinaded_{ false};
  bool delivered_{ false};
  
public:
  Order(net::io_context& io, int id, bool with_onion, OrderHandler handler)
    : id_(id)
    , with_onion_(with_onion)
    , handler_(std::move(handler))
    , logger_(std::to_string(id))
    , roast_timer_(io, 1s)
    , marinade_timer_(io, 2s) {
  }

  void Execute() {
    logger_.LogMessage("Order has been started"sv);
    RoastCutlet();
    if (with_onion_) {
      MarinadeOnion();
    }
  }
};

class Restaurant {
  net::io_context& io_;
  int next_order_id_{ 0};
  
public:
  explicit Restaurant(net::io_context& io)
    : io_(io) {
  }

  int MakeHamburger(bool with_onion, OrderHandler handler) {
    const int order_id = ++next_order_id_;
    std::make_shared<Order>(
      io_,
      order_id,
      with_onion,
      std::move(handler)
    )->Execute();
    return order_id;
  }
};

int main() {
  net::io_context io;
  Restaurant restaurant{io};
  Logger logger{"main"s};
  struct OrderResult {
    sys::error_code ec;
    Hamburger hamburger;
  };
  std::unordered_map<int, OrderResult> orders;
  auto handle_result = [&orders](sys::error_code ec, int id, Hamburger* h) {
    orders.emplace(id, OrderResult{ec, ec ? Hamburger{} : *h});
  };
  const int id1 = restaurant.MakeHamburger(false, handle_result);
  const int id2 = restaurant.MakeHamburger(true, handle_result);
  assert(orders.empty());
  io.run();
  assert(orders.size() == 2u);
  {
    const auto& o = orders.at(id1);
    assert(!o.ec);
    assert(o.hamburger.IsCutletRoasted());
    assert(o.hamburger.IsPacked());
    assert(!o.hamburger.HasOnion());
  }
  {
    const auto& o = orders.at(id2);
    assert(!o.ec);
    assert(o.hamburger.IsCutletRoasted());
    assert(o.hamburger.IsPacked());
    assert(o.hamburger.HasOnion());
  }
}
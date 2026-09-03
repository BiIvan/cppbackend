#include <string>
#include <thread>
#include <vector>
#include <csignal>
#include <utility>
#include <iostream>
#include <algorithm>
#include <string_view>
#include <boost/asio/signal_set.hpp>

#include "sdk.h"
#include "http_server.h"

namespace {

  namespace net = boost::asio;
  namespace sys = boost::system;
  namespace http = boost::beast::http;

  using namespace std::literals;

  using StringRequest = http::request<http::string_body>;
  using StringResponse = http::response<http::string_body>;

  struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view TEXT_HTML = "text/html"sv;
  };

  StringResponse MakeStringResponse(
    http::status status,
    std::string_view body,
    unsigned http_version,
    bool keep_alive,
    std::string_view content_type = ContentType::TEXT_HTML) {
    StringResponse response(status, http_version);
    response.set(http::field::content_type, content_type);
    response.body() = body;
    response.content_length(body.size());
    response.keep_alive(keep_alive);
    return response;
  }

  StringResponse HandleRequest(StringRequest&& req) {
    const auto text_response{ 
      [&req](http::status status, std::string_view text) {
        return MakeStringResponse(status,text,req.version(),req.keep_alive());
      }
    };
    if (req.method() == http::verb::get ||
      req.method() == http::verb::head) {
      std::string_view target = req.target();
      if (!target.empty() && target.front() == '/') {
        target.remove_prefix(1);
      }
      const std::string body = "Hello, "s + std::string(target);
      if (req.method() == http::verb::head) {
        StringResponse response(http::status::ok, req.version());
        response.set(http::field::content_type, ContentType::TEXT_HTML);
        response.content_length(body.size());
        response.keep_alive(req.keep_alive());
        return response;
      }
      return text_response(http::status::ok, body);
    }
    StringResponse response = text_response(
      http::status::method_not_allowed,
      "Invalid method"sv);
    response.set(http::field::allow, "GET, HEAD");
    return response;
  }

  template <typename Fn>
  void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    while (--n) {
      workers.emplace_back(fn);
    }
    fn();
  }

}  // namespace

int main() {
  try {
    const unsigned num_threads =
      std::max(1u, std::thread::hardware_concurrency());
    net::io_context ioc(num_threads);
    net::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait(
      [&ioc](const sys::error_code& ec,
           [[maybe_unused]] int signal_number) {
        if (!ec) {
          ioc.stop();
        }
      });
    const auto address = net::ip::make_address("0.0.0.0");
    constexpr net::ip::port_type port = 8080;
    http_server::ServeHttp(
      ioc,
      {address, port},
      [](auto&& req, auto&& sender) {
        sender(HandleRequest(
          std::forward<decltype(req)>(req)));
      });
    std::cout << "Server has started..."sv << std::endl;
    RunWorkers(num_threads, [&ioc] {
      ioc.run();
    });
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}
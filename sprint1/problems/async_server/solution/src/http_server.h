#pragma once

// Beast будет использовать std::string_view.
#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/dispatch.hpp>

#include <chrono>
#include <memory>
#include <utility>
#include <iostream>
#include <string_view>
#include <type_traits>

#include "sdk.h"

namespace http_server {

  namespace net = boost::asio;
  namespace beast = boost::beast;
  namespace http = beast::http;
  using tcp = net::ip::tcp;
  using namespace std::literals;
  using HttpRequest = http::request<http::string_body>;

  inline void ReportError(beast::error_code ec, std::string_view where) {
      if (ec == net::error::operation_aborted) {
          return;
      }
    std::cerr << where << ": " << ec.message() << std::endl;
  }

  class SessionBase {
    virtual std::shared_ptr<SessionBase> GetSharedThis() = 0;

    void Read() {
      request_ = {};
      stream_.expires_after(30s);

      http::async_read(
        stream_,
        buffer_,
        request_,
        beast::bind_front_handler(
          &SessionBase::OnRead,
          GetSharedThis()));
    }

    void OnRead(beast::error_code ec,
          [[maybe_unused]] std::size_t bytes_read) {
      if (ec == http::error::end_of_stream) {
        Close();
        return;
      }

      if (ec) {
        ReportError(ec, "read"sv);
        return;
      }

      HandleRequest(std::move(request_));
    }

    void Close() {
      beast::error_code ec;
      stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    }

    virtual void HandleRequest(HttpRequest&& request) = 0;

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    HttpRequest request_;
    
  public:
    SessionBase(const SessionBase&) = delete;
    SessionBase& operator=(const SessionBase&) = delete;

    void Run() {
      net::dispatch(
        stream_.get_executor(),
        beast::bind_front_handler(
          &SessionBase::Read,
          GetSharedThis()));
    }

  protected:
    explicit SessionBase(tcp::socket&& socket)
      : stream_(std::move(socket)) {
    }

    virtual ~SessionBase() = default;


    template <typename Body, typename Fields>
    void Write(http::response<Body, Fields>&& response) {
      auto safe_response{ std::make_shared<http::response<Body, Fields>>(std::move(response))};
      const bool close{ safe_response->need_eof()};
      auto self{ GetSharedThis()};
      http::async_write(
        stream_,
        *safe_response,
        [self, safe_response, close](
          beast::error_code ec,
          std::size_t bytes_written) {
          self->OnWrite(close, ec, bytes_written);
        });
    }

    void OnWrite(bool close, beast::error_code ec,[[maybe_unused]] std::size_t bytes_written) {
      if (ec) { ReportError(ec, "write"sv); return; }
      if (close) { Close(); return; }
      Read();
    }
  };

  template <typename RequestHandler>
  class Session
    : public SessionBase
    , public std::enable_shared_from_this<Session<RequestHandler>> {
      
    void HandleRequest(HttpRequest&& request) override {
      request_handler_(
        std::move(request),
        [self = this->shared_from_this()](auto&& response) {
          self->Write(std::move(response));
        });
    }

    std::shared_ptr<SessionBase> GetSharedThis() override {
      return this->shared_from_this();
    }

    RequestHandler request_handler_;
    
  public:
    template <typename Handler>
    Session(tcp::socket&& socket, Handler&& request_handler)
      : SessionBase(std::move(socket))
      , request_handler_(std::forward<Handler>(request_handler)) {
    }
  };

  template <typename RequestHandler>
  class Listener
    : public std::enable_shared_from_this<Listener<RequestHandler>> {
      
    void DoAccept() {
      acceptor_.async_accept(
        net::make_strand(ioc_),
        beast::bind_front_handler(&Listener::OnAccept,this->shared_from_this())
      );
    }

    void OnAccept(beast::error_code ec, tcp::socket socket) {
      if (ec) {
        if (ec != net::error::operation_aborted) {
          ReportError(ec, "accept");
        }
        return;
      }
      AsyncRunSession(std::move(socket));
      DoAccept();
    }

    void AsyncRunSession(tcp::socket&& socket) {
      std::make_shared<Session<RequestHandler>>(std::move(socket),request_handler_)->Run();
    }

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    RequestHandler request_handler_;
    
  public:
    template <typename Handler>
    Listener(net::io_context& ioc,
         const tcp::endpoint& endpoint,
         Handler&& request_handler)
      : ioc_(ioc)
      , acceptor_(net::make_strand(ioc))
      , request_handler_(std::forward<Handler>(request_handler)) {
      beast::error_code ec;
      acceptor_.open(endpoint.protocol(), ec); 
      if (ec) { ReportError(ec, "open"); return; }
      acceptor_.set_option(net::socket_base::reuse_address(true), ec);
      if (ec) { ReportError(ec, "set_option"); return; }
      acceptor_.bind(endpoint, ec);
      if (ec) { ReportError(ec, "bind"); return; }
      acceptor_.listen(net::socket_base::max_listen_connections, ec);
      if (ec) { ReportError(ec, "listen"); }
    }

    void Run() {
      DoAccept();
    }
  };

  template <typename RequestHandler>
  void ServeHttp(net::io_context& ioc,
           const tcp::endpoint& endpoint,
           RequestHandler&& handler) {
    using MyListener = Listener<std::decay_t<RequestHandler>>;
    std::make_shared<MyListener>(ioc,endpoint,std::forward<RequestHandler>(handler))->Run();
  }

}  // namespace http_server
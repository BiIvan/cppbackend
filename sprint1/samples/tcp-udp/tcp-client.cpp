#include <string>
#include <iostream>
#include <string_view>
#include <boost/asio.hpp>

namespace net = boost::asio;
using net::ip::tcp;

using namespace std::literals;

int main(int argc, const char** argv) {
  static const int port = 3333;
  static const size_t max_buffer_size = 1024;

  if (argc != 2) {
    std::cout << "Usage: "sv << argv[0] << " <server IP>"sv << std::endl;
    return 1;
  }

  try {
    net::io_context io_context;

    tcp::socket socket(io_context);
    socket.connect(tcp::endpoint(net::ip::make_address(argv[1]), port));

    boost::system::error_code ec;
    net::write(socket, net::buffer("Hello from TCP-client\n"sv), ec);

    if (ec) {
      std::cerr << "Data transmission error: "sv << ec.message() << std::endl;
      return 1;
    }

    net::streambuf stream_buf;
    net::read_until(socket, stream_buf, '\n', ec);

    if (ec) {
      std::cerr << "Error reading response: "sv << ec.message() << std::endl;
      return 1;
    }

    std::string response;
    std::istream is(&stream_buf);
    std::getline(is, response);
    
    std::cout << "Server responded: "sv << response << std::endl;
  } catch (std::exception& e) {
    std::cerr << "Error: "sv << e.what() << std::endl;
  }

  return 0;
}

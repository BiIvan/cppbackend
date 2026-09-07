#ifdef WIN32
#include <sdkddkver.h>
#endif

#include <array>
#include <limits>
#include <string>
#include <cstdint>
#include <iostream>
#include <optional>
#include <algorithm>
#include <string_view>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include "seabattle.h"

namespace net = boost::asio;
using net::ip::tcp;
using namespace std::literals;

constexpr size_t move_packet_size = 8;
constexpr std::array<char, 4> handshake_magic = { 'S', 'B', 'T', 'L'};
constexpr std::uint8_t protocol_version = 1;

struct GameConfig {
  std::uint16_t width;
  std::uint16_t height;
  std::uint16_t ship_cells;
};

void PrintFieldPair(const SeabattleField& left,const SeabattleField& right) {
  constexpr std::string_view delimiter = "     ";
  left.PrintDigitLine(std::cout);
  std::cout << delimiter << "   ";
  right.PrintDigitLine(std::cout);
  std::cout << '\n';
  const size_t max_height = std::max(left.GetHeight(),right.GetHeight());
  for (size_t y = 0; y < max_height; ++y) {
    if (y < left.GetHeight()) {
      left.PrintLine(std::cout, y);
    } else {
      std::cout << std::string(left.GetWidth() * 3 + 5, ' ');
    }
    std::cout << delimiter;
    if (y < right.GetHeight()) {
      right.PrintLine(std::cout, y);
    }
    std::cout << '\n';
  }
  left.PrintDigitLine(std::cout);
  std::cout << delimiter << "   ";
  right.PrintDigitLine(std::cout);
  std::cout << '\n';
}

template <size_t sz>
static std::optional<std::string> ReadExact(tcp::socket& socket) {
  boost::array<char, sz> buffer;
  boost::system::error_code ec;
  net::read(socket,net::buffer(buffer),net::transfer_exactly(sz),ec);
  if (ec) {
    return std::nullopt;
  }
  return std::string(buffer.data(), sz);
}

static bool WriteExact(tcp::socket& socket,std::string_view data) {
  boost::system::error_code ec;
  net::write(socket,net::buffer(data.data(), data.size()),net::transfer_exactly(data.size()),ec);
  return !ec;
}

static std::array<char, 11> SerializeGameConfig(const GameConfig& config) {
  std::array<char, 11> packet{};
  packet[0] = handshake_magic[0];
  packet[1] = handshake_magic[1];
  packet[2] = handshake_magic[2];
  packet[3] = handshake_magic[3];
  packet[4] = static_cast<char>(protocol_version);
  packet[5] = static_cast<char>((config.width >> 8) & 0xFF);
  packet[6] = static_cast<char>(config.width & 0xFF);
  packet[7] = static_cast<char>((config.height >> 8) & 0xFF);
  packet[8] = static_cast<char>(config.height & 0xFF);
  packet[9] = static_cast<char>((config.ship_cells >> 8) & 0xFF);
  packet[10] = static_cast<char>(config.ship_cells & 0xFF);
  return packet;
}

static bool SendGameConfig(tcp::socket& socket,const GameConfig& config) {
  const auto packet{ SerializeGameConfig(config)};
  return WriteExact(socket,std::string_view(packet.data(), packet.size()));
}

static std::optional<GameConfig> ReceiveGameConfig(tcp::socket& socket) {
  const auto packet{ ReadExact<11>(socket)};
  if (!packet) {
    return std::nullopt;
  }
  if ((*packet)[0] != handshake_magic[0]
    || (*packet)[1] != handshake_magic[1]
    || (*packet)[2] != handshake_magic[2]
    || (*packet)[3] != handshake_magic[3]) {
    return std::nullopt;
  }
  const auto received_version{ static_cast<std::uint8_t>((*packet)[4])};
  if (received_version != protocol_version) {
    return std::nullopt;
  }
  const auto width = static_cast<std::uint16_t>(
    (static_cast<std::uint8_t>((*packet)[5]) << 8)
    | static_cast<std::uint8_t>((*packet)[6]));
  const auto height = static_cast<std::uint16_t>(
    (static_cast<std::uint8_t>((*packet)[7]) << 8)
    | static_cast<std::uint8_t>((*packet)[8]));
  const auto ship_cells = static_cast<std::uint16_t>(
    (static_cast<std::uint8_t>((*packet)[9]) << 8)
    | static_cast<std::uint8_t>((*packet)[10]));
  if (width == 0 || height == 0 || ship_cells == 0) {
    return std::nullopt;
  }
  return GameConfig{width,height,ship_cells};
}

class SeabattleAgent {
  SeabattleField my_field_;
  SeabattleField other_field_;
  int other_field_weight_;
  
public:
  SeabattleAgent(const SeabattleField& field,int other_field_weight)
    : my_field_(field)
    , other_field_(
      field.GetWidth(),
      field.GetHeight(),
      SeabattleField::State::UNKNOWN)
    , other_field_weight_(other_field_weight) {
  }

  void StartGame(tcp::socket& socket, bool my_initiative) {
    bool my_turn{ my_initiative};
    std::cout << (my_turn ? "You start the game." : "Opponent goes first.") << std::endl;
    while (!IsGameEnded()) {
      if (my_turn) {
        PrintFields();
        const auto move{ ReadPlayerMove()};
        if (!move) {
          std::cout << "Entry complete. Game stopped." << std::endl;
          return;
        }
        const std::string move_text{ MoveToString(*move)};
        if (move_text.size() >= move_packet_size) {
          std::cerr << "Coordinate too long for current protocol." << std::endl;
          return;
        }
        std::array<char, move_packet_size> packet{};
        std::copy(move_text.begin(),move_text.end(),packet.begin());
        if (!WriteExact(socket,std::string_view(packet.data(),packet.size()))) {
          std::cerr << "Failed to send move: connection closed." << std::endl;
          return;
        }
        const auto response = ReadExact<1>(socket);
        if (!response) {
          std::cerr << "Failed to receive shot result." << std::endl;
          return;
        }
        const auto result = 
          static_cast<SeabattleField::ShotResult>(static_cast<unsigned char>((*response)[0]));
        if (!IsValidShotResult(result)) {
          std::cerr << "Opponent sent an unknown shot result." << std::endl;
          return;
        }
        ApplyShotResult(other_field_,move->first,move->second,result);
        if (result == SeabattleField::ShotResult::HIT || result == SeabattleField::ShotResult::KILL) {
          --other_field_weight_;
        }
        std::cout << "Your shot " << move_text << ": " << ShotResultToString(result) << std::endl;
        if (other_field_weight_ == 0) {
          PrintFields();
          std::cout << "You win!" << std::endl;
          return;
        }
        if (result == SeabattleField::ShotResult::MISS) {
          my_turn = false;
        }
        continue;
      }
      std::cout << "Waiting for opponent's move..." << std::endl;
      const auto request = ReadExact<move_packet_size>(socket);
      if (!request) {
        std::cerr << "Opponent disconnected." << std::endl;
        return;
      }
      const size_t zero_pos{ request->find('\0')};
      const std::string_view raw_move(
        request->data(), zero_pos == std::string::npos ? request->size() : zero_pos);
      const auto move = ParseMove(raw_move,my_field_.GetWidth(),my_field_.GetHeight());
      if (!move) {
        std::cerr << "Opponent sent an invalid coordinate." << std::endl;
        return;
      }
      const auto result{ my_field_.Shoot(move->first,move->second)};
      const char response{ static_cast<char>(result)};
      if (!WriteExact(socket, std::string_view(&response, 1))) {
        std::cerr << "Failed to send shot result." << std::endl;
        return;
      }
      std::cout << "Opponent shoots at " << MoveToString(*move)
        << ": " << ShotResultToString(result) << std::endl;
      if (my_field_.IsLoser()) {
        PrintFields();
        std::cout << "You lose." << std::endl;
        return;
      }
      if (result == SeabattleField::ShotResult::MISS) {
        my_turn = true;
      }
    }
  }

private:
  static std::optional<std::pair<size_t, size_t>> ParseMove(std::string_view move,size_t width,size_t height) {
    if (move.size() < 2) {
      return std::nullopt;
    }
    const char row_char{ move[0]};
    if (row_char < 'A') {
      return std::nullopt;
    }
    const size_t y{ static_cast<size_t>(row_char - 'A')};
    if (y >= height) {
      return std::nullopt;
    }
    size_t column_number{ 0};
    for (size_t i = 1; i < move.size(); ++i) {
      const char ch = move[i];
      if (ch < '0' || ch > '9') {
        return std::nullopt;
      }
      column_number = column_number * 10 + static_cast<size_t>(ch - '0');
    }
    if (column_number == 0 || column_number > width) {
      return std::nullopt;
    }
    const size_t x{ column_number - 1};
    return std::pair<size_t, size_t>{x, y};
  }

  static std::string MoveToString(std::pair<size_t, size_t> move) {
    const auto [x, y]{ move};
    return std::string(1,static_cast<char>('A' + y))+ std::to_string(x + 1);
  }

  static bool IsValidShotResult(SeabattleField::ShotResult result) {
    return result == SeabattleField::ShotResult::MISS
      || result == SeabattleField::ShotResult::HIT
      || result == SeabattleField::ShotResult::KILL;
  }

  static const char* ShotResultToString(SeabattleField::ShotResult result) {
    switch (result) {
      case SeabattleField::ShotResult::MISS:
        return "Way off";
      case SeabattleField::ShotResult::HIT:
        return "Hit";
      case SeabattleField::ShotResult::KILL:
        return "Killed";
    }
    return "Unknown result";
  }

  static void ApplyShotResult( SeabattleField& field,size_t x,size_t y,SeabattleField::ShotResult result) {
    switch (result) {
      case SeabattleField::ShotResult::MISS:
        field.MarkMiss(x, y);
        return;
      case SeabattleField::ShotResult::HIT:
        field.MarkHit(x, y);
        return;
      case SeabattleField::ShotResult::KILL:
        field.MarkKill(x, y);
        return;
    }
  }

  std::optional<std::pair<size_t, size_t>> ReadPlayerMove() const {
    while (true) {
      std::cout << "Your move (e.g., A1, B10, P16): ";
      std::string input;
      if (!(std::cin >> input)) {
        return std::nullopt;
      }
      const auto move{ ParseMove(input,other_field_.GetWidth(),other_field_.GetHeight())};
      if (!move) {
        std::cout << "Invalid coordinate. " << "Row must be from A to " 
                  << static_cast<char>(
            'A' + other_field_.GetHeight() - 1) << ", Column — 1 to "
                  << other_field_.GetWidth() << "."
                  << std::endl;
        continue;
      }
      const auto [x, y] = *move;
      if (other_field_(x, y) != SeabattleField::State::UNKNOWN) {
        std::cout << "A shot has already been fired at this cell." << std::endl;
        continue;
      }
      return move;
    }
  }

  void PrintFields() const {
    std::cout << "\nYour field" 
              << std::string( my_field_.GetWidth() * 3 > 10 ? my_field_.GetWidth() * 3 - 10 : 2, ' ') 
              << "Opponent's field\n";
    PrintFieldPair(my_field_, other_field_);
    std::cout << std::endl;
  }

  bool IsGameEnded() const {
    return my_field_.IsLoser() || other_field_weight_ == 0;
  }
};

void StartServer(const SeabattleField& field,unsigned short port) {
  try {
    net::io_context io_context;
    tcp::acceptor acceptor(io_context,tcp::endpoint(tcp::v4(), port));
    std::cout << "Server started.\n"
      << "Field size: " << field.GetWidth() << "x" << field.GetHeight()
      << "\nPort: " << port << "\nWaiting for client..." << std::endl;
    tcp::socket socket(io_context);
    acceptor.accept(socket);
    std::cout << "Client connected: " << socket.remote_endpoint() << std::endl;
    const GameConfig config{
      static_cast<std::uint16_t>(field.GetWidth()),
      static_cast<std::uint16_t>(field.GetHeight()),
      static_cast<std::uint16_t>(field.GetWeight())
    };
    if (!SendGameConfig(socket, config)) {
      std::cerr << "Failed to send game parameters to the client." << std::endl;
      return;
    }
    std::cout << "Handshake sent: " << config.width << "x" << config.height
      << ", ship cells: " << config.ship_cells << std::endl;
    SeabattleAgent agent(field, config.ship_cells);
    agent.StartGame(socket, false);
  } catch (const boost::system::system_error& e) {
    std::cerr << "Server error: " << e.what() << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Unexpected server error: " << e.what() << std::endl;
  }
}

void StartClient(int seed,const std::string& ip_str,unsigned short port) {
  try {
    net::io_context io_context;
    tcp::socket socket(io_context);
    const auto address{ net::ip::make_address(ip_str)};
    const tcp::endpoint endpoint(address, port);
    std::cout << "Connecting to " << endpoint << "..." << std::endl;
    socket.connect(endpoint);
    std::cout << "Connection established. " << "Waiting for handshake..." << std::endl;
    const auto config = ReceiveGameConfig(socket);
    if (!config) {
      std::cerr << "Handshake not received or has an invalid format." << std::endl;
      return;
    }
    constexpr size_t max_supported_width{ 999999};
    constexpr size_t max_supported_height{ 26};
    if (config->width > max_supported_width || config->height > max_supported_height) {
      std::cerr << "The server sent an unsupported field size: " 
                << config->width << "x" << config->height << std::endl;
      return;
    }
    std::cout << "Game parameters received: " 
              << config->width << "x" << config->height << std::endl;
    std::mt19937 engine(seed);
    SeabattleField field =
      SeabattleField::GetRandomField(config->width,config->height,engine);
    SeabattleAgent agent(field, config->ship_cells);
    agent.StartGame(socket, true);
  } catch (const boost::system::system_error& e) {
    std::cerr << "Client error: " << e.what() << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Unexpected client error: " << e.what() << std::endl;
  }
}

int main(int argc, const char** argv) {
  try {
    if (argc != 4 && argc != 5) {
      std::cout << "Usage:\n" << "  server: program <seed> <width> <height> <port>\n"
                << "  client: program <seed> <server-ip> <port>\n";
      return 1;
    }
    const int seed{ std::stoi(argv[1])};
    if (argc == 5) {
      const size_t width{ std::stoul(argv[2])};
      const size_t height{ std::stoul(argv[3])};
      const unsigned short port{ static_cast<unsigned short>(std::stoul(argv[4]))};
      if (width == 0 || height == 0) {
        std::cerr << "The field width and height must be greater than zero." << std::endl;
        return 1;
      }
      if (width > std::numeric_limits<std::uint16_t>::max() 
        || height > std::numeric_limits<std::uint16_t>::max()) {
        std::cerr << "The width and height must not exceed 65535." << std::endl;
        return 1;
      }
      if (height > 26) {
        std::cerr << "The field height must not exceed 26: " 
                  << "rows are designated by the letters A-Z." << std::endl;
        return 1;
      }
      std::mt19937 engine(seed);
      SeabattleField field{ SeabattleField::GetRandomField(width,height,engine)};
      StartServer(field, port);
      return 0;
    }
    const std::string server_ip{ argv[2]};
    const unsigned short port{ static_cast<unsigned short>(std::stoul(argv[3]))};
    StartClient(seed, server_ip, port);
    return 0;
  } catch (const std::invalid_argument& e) {
    std::cerr << "Invalid command line argument: " << e.what() << std::endl;
    return 1;
  } catch (const std::out_of_range& e) {
    std::cerr << "Command line argument out of range: " << e.what() << std::endl;
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "Unexpected error: " << e.what() << std::endl;
    return 1;
  }
}
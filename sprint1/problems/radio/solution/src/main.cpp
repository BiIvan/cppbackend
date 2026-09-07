#include "audio.h"

#include <array>
#include <chrono>
#include <string>
#include <thread>
#include <cstddef>
#include <iostream>
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <string_view>
#include <boost/asio.hpp>

namespace net = boost::asio;

using net::ip::udp;
using namespace std::literals;

constexpr ma_format audio_format = ma_format_s16;
constexpr ma_uint32 audio_channels = 1;
constexpr std::size_t max_frame_size = 4;
constexpr std::size_t sample_rate = 44100;
constexpr std::size_t packet_duration_ms = 40;
constexpr auto sample_duration = std::chrono::milliseconds(1500);
constexpr std::size_t frames_per_packet = sample_rate * packet_duration_ms / 1000;
constexpr auto packet_duration = std::chrono::milliseconds(packet_duration_ms);
constexpr std::size_t max_packet_bytes = frames_per_packet * max_frame_size;

static_assert(frames_per_packet > 0);
static_assert(max_packet_bytes > 0);

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cout << "Usage: "sv << argv[0] << " <server|client> <port>\n"sv;
    return 1;
  }
  const std::string_view role = argv[1];
  if (role != "server" && role != "client") {
    std::cerr << "Role must be \"server\" or \"client\"\n"sv;
    return 1;
  }
  int port{ 0};
  try {
    std::size_t parsed{ 0};
    port = std::stoi(argv[2], &parsed);
    if (argv[2][parsed] != '\0') {
      throw std::invalid_argument( "port contains non-numeric characters");
    }
  } catch (const std::exception& e) {
    std::cerr << "Invalid port: " << e.what() << "\nPort must be in range [1, 65535]\n";
    return 1;
  }
  if (port < 1 || port > 65535) {
    std::cerr << "Port must be in range [1, 65535]\n";
    return 1;
  }
  const auto udp_port{ static_cast<unsigned short>(port)};
  if (role == "client") {
    try {
      Recorder recorder( audio_format,audio_channels,static_cast<ma_uint32>(sample_rate));
      net::io_context io_context;
      std::string ip_address;
      std::cout << "Enter server IPv4 address: "sv;
      std::getline(std::cin, ip_address);
      boost::system::error_code ec;
      const auto address{ net::ip::make_address_v4(ip_address, ec)};
      if (ec) {
        std::cerr << "Invalid IPv4 address: " << ec.message() << '\n';
        return 1;
      }
      const udp::endpoint endpoint(address,udp_port);
      std::cout << "Press Enter to record message..."sv << std::endl;
      std::string line;
      std::getline(std::cin, line);
      auto recording{ recorder.Record(sample_rate * 2,sample_duration)};
      const std::size_t frame_size{ recorder.GetFrameSize()};
      if (frame_size == 0) {
        std::cerr << "Invalid recorder frame size\n";
        return 1;
      }
      const std::size_t total_bytes{ recording.frames * frame_size};
      const std::size_t packet_bytes{ frames_per_packet * frame_size};
      if (recording.frames == 0 || total_bytes == 0) {
        std::cerr << "No audio was recorded\n";
        return 1;
      }
      if (packet_bytes == 0) {
        std::cerr << "Invalid packet size\n";
        return 1;
      }
      std::cout << "Recorded " << recording.frames 
                << " frames / " << total_bytes << " bytes\n";
      std::cout << "Sending UDP packets of up to " << packet_bytes 
                << " bytes every " << packet_duration_ms << " ms\n";
      udp::socket socket(io_context, udp::v4());
      for (std::size_t offset = 0; offset < total_bytes; offset += packet_bytes) {
        const std::size_t bytes_to_send{ std::min(packet_bytes,total_bytes - offset)};
        const std::size_t sent = 
          socket.send_to( net::buffer( recording.data.data() + offset, bytes_to_send), endpoint);
        if (sent != bytes_to_send) {
          std::cerr << "UDP datagram was sent partially: " << sent 
                    << " of " << bytes_to_send << " bytes\n";
          return 1;
        }
        std::this_thread::sleep_for(packet_duration);
      }
      std::cout << "Sending done\n";
    } catch (const std::exception& e) {
      std::cerr << "Client error: " << e.what() << '\n';
      return 1;
    }
  } else {
    try {
      constexpr std::size_t prebuffer_ms{ 100};
      constexpr std::size_t prebuffer_frames{ sample_rate * prebuffer_ms / 1000};
      Player player(audio_format,audio_channels,static_cast<ma_uint32>(sample_rate),500);
      net::io_context io_context;
      udp::socket socket(io_context,udp::endpoint(udp::v4(), udp_port));
      std::array<char, max_packet_bytes> receive_buffer{};
      bool playback_started{ false};
      std::cout << "Listening on UDP port " << port << '\n';
      std::cout << "Expected PCM format: s16, mono, " << sample_rate 
                << " Hz; packet size up to " << max_packet_bytes << " bytes\n";
      for (;;) {
        udp::endpoint remote_endpoint;
        const std::size_t bytes_received{ socket.receive_from(net::buffer(receive_buffer),remote_endpoint)};
        const std::size_t frame_size{ player.GetFrameSize()};
        if (frame_size == 0) {
          std::cerr << "Invalid player frame size\n";
          return 1;
        }
        const std::size_t expected_packet_bytes{ frames_per_packet * frame_size};
        if (bytes_received == 0) {
          continue;
        }
        if (bytes_received > expected_packet_bytes) {
          std::cerr << "Ignoring oversized PCM packet: " << bytes_received 
                    << " bytes; expected at most " << expected_packet_bytes << " bytes\n";
          continue;
        }
        if (bytes_received % frame_size != 0) {
          std::cerr << "Ignoring invalid PCM packet: " << bytes_received
                << " bytes is not divisible by frame size " << frame_size << '\n';
          continue;
        }
        const std::size_t frames{ bytes_received / frame_size};
        const std::size_t pushed{ player.PushBuffer( receive_buffer.data(),frames)};
        if (pushed != frames) {
          std::cerr << "Playback ring buffer overflow: " << "accepted " 
                    << pushed << " of " << frames << " frames\n";
        }
        std::cout << "Received " << bytes_received 
                  << " bytes (" << frames 
                  << " frames); buffered " << player.BufferedFrames()
                  << " frames; from " << remote_endpoint.address().to_string()
                  << ':' << remote_endpoint.port() << '\n';
        if (!playback_started && player.BufferedFrames() >= prebuffer_frames) {
          player.Start();
          playback_started = true;
          std::cout << "Playback started after buffering " << player.BufferedFrames() 
                    << " frames\n";
        }
      }
    } catch (const std::exception& e) {
      std::cerr << "Server error: " << e.what() << '\n';
      return 1;
    }
  }
  return 0;
}
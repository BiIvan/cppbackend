#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>

#include "logger.h"
#include "ticker.h"
#include "literals.h"
#include "http_server.h"
#include "json_loader.h"
#include "request_handler.h"
#include "model_serialization.h"

namespace net = boost::asio;
namespace json = boost::json;
namespace fs = std::filesystem;
namespace po = boost::program_options;

using tcp = net::ip::tcp;
using namespace std::literals;
using mSeconds = std::chrono::milliseconds;

namespace {

  struct Args {
    fs::path config_file;
    fs::path www_root;
    std::optional<mSeconds> tick_period;
    bool randomize_spawn_points = false;
    std::optional<fs::path> state_file;
    std::optional<mSeconds> save_state_period;
  };

  template <typename Fn>
  void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::thread> workers;
    workers.reserve(n - 1);
    while (--n) {
      workers.emplace_back(fn);
    }
    fn();
    for (std::thread& worker : workers) {
      worker.join();
    }
  }

  std::optional<Args> ParseArgs(int argc, const char* const argv[]) {
    po::options_description desc{"Allowed options"};
    desc.add_options()
      ("help,h", "produce help message")
      ("tick-period,t",
        po::value<std::int64_t>()->value_name("milliseconds"),
        "set tick period")
      ("config-file,c",
        po::value<std::string>()->value_name("file"),
        "set config file path")
      ("www-root,w",
        po::value<std::string>()->value_name("dir"),
        "set static files root")
      ("state-file", po::value<std::string>()->value_name("file"),
        "set state file path")
      ("save-state-period", po::value<std::int64_t>()->value_name("milliseconds"),
        "set state save period in game time")
      ("randomize-spawn-points",
        "spawn dogs at random positions");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help")) {
      std::cout << desc << '\n';
      return std::nullopt;
    }
    po::notify(vm);
    if (!vm.count("config-file") || !vm.count("www-root")) {
      throw std::invalid_argument(
        "--config-file and --www-root are required");
    }
    Args args{
      .config_file = vm["config-file"].as<std::string>(),
      .www_root = vm["www-root"].as<std::string>(),
      .randomize_spawn_points =
        vm.count("randomize-spawn-points") != 0
    };
    if (vm.count("tick-period")) {
      const auto ms = vm["tick-period"].as<std::int64_t>();
      if (ms <= 0) {
        throw std::invalid_argument(
          "--tick-period must be greater than zero");
      }
      args.tick_period = mSeconds{ms};
    }
    if (vm.count("state-file")) {
      const auto path = vm["state-file"].as<std::string>();
      if (path.empty()) { throw std::invalid_argument("Empty --state-file"); }
      args.state_file = fs::path{path};
      if (vm.count("save-state-period")) {
        const auto ms = vm["save-state-period"].as<std::int64_t>();
        if (ms < 0) {
          throw std::invalid_argument("--save-state-period must be nonnegative");
        }
        args.save_state_period = mSeconds{ms};
      }
    }
    return args;
  }

}  // namespace

int main(int argc, const char* argv[]) {
  logger::InitLogger();
  try {
    const auto args = ParseArgs(argc, argv);
    if (!args) {
      return EXIT_SUCCESS;
    }
    const char* db_url = std::getenv("GAME_DB_URL");
    if (!db_url || !*db_url) { throw std::runtime_error("GAME_DB_URL is required"); }
    database::Records records{db_url};
    extra_data::ExtraData extra;
    model::Game game = json_loader::LoadGame(args->config_file, extra);
    const unsigned num_threads = std::max(1u, std::thread::hardware_concurrency());
    net::io_context ioc{static_cast<int>(num_threads)};
    net::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&ioc](const boost::system::error_code& ec, int) {
      if (!ec) { ioc.stop(); }
    });
    auto api_strand = net::make_strand(ioc);
    http_handler::RequestHandler handler{
      game,
      extra,
      args->www_root,
      api_strand,
      args->randomize_spawn_points,
      args->tick_period.has_value()
    };
    handler.SetRecords(records);
    if (args->state_file) {
      serialization::StateSerializer::Load(handler.GetApplication(), *args->state_file);
      handler.GetApplication().ConfigureSaving(
        [&handler, path = *args->state_file] {
          serialization::StateSerializer::Save(handler.GetApplication(), path);
        }, args->save_state_period);
    }
    std::shared_ptr<Ticker> ticker;
    if (args->tick_period) {
      ticker = std::make_shared<Ticker>(
        api_strand,
        *args->tick_period,
        [&handler](mSeconds delta) {
          handler.GetApplication().Tick(delta.count());
        });
      ticker->Start();
    }
    const auto address = net::ip::make_address("0.0.0.0");
    constexpr unsigned short port = 8080;
    http_server::ServeHttp( ioc, tcp::endpoint{address, port}, handler);
    BOOST_LOG_TRIVIAL(info)
      << logging::add_value( additional_data, json::object{ {"port", port}, {"address", address.to_string()}, })
      << SRVSTR;
    logging::core::get()->flush();
    std::cout.flush();
    RunWorkers(num_threads, [&ioc] { ioc.run(); });
    if (args->state_file) {
      serialization::StateSerializer::Save(handler.GetApplication(), *args->state_file);
    }
    BOOST_LOG_TRIVIAL(info)
      << logging::add_value( additional_data, json::object{ {COD, EXIT_SUCCESS}, })
      << SRVEXT;
    return EXIT_SUCCESS;
  } catch (const std::exception& ex) {
    BOOST_LOG_TRIVIAL(error)
      << logging::add_value( additional_data, json::object{ {COD, EXIT_FAILURE}, {EXPT, ex.what()}, })
      << SRVEXT;
    return EXIT_FAILURE;
  }
}

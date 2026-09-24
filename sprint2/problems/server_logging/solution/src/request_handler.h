#pragma once

#include <string>
#include <utility>
#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <boost/json.hpp>
#include <boost/beast/core/string.hpp>

#include "model.h"
#include "http_server.h"

namespace http_handler {
  
  namespace beast = boost::beast;
  namespace http = beast::http;
  namespace json = boost::json;
  
  class RequestHandler {
    using StringResponse = http::response<http::string_body>;
    using FileResponse = http::response<http::file_body>;
    
    static StringResponse MakeStringResponse(
      http::status status,
      unsigned version,
      bool keep_alive,
      std::string body,
      beast::string_view content_type = "text/plain") {
      StringResponse response{status, version};
      response.set(http::field::content_type, content_type);
      response.keep_alive(keep_alive);
      response.body() = std::move(body);
      response.prepare_payload();
      return response;
    }
    
    static StringResponse MakeJsonResponse(
      http::status status,
      unsigned version,
      bool keep_alive,
      json::value body) {
      return MakeStringResponse(
        status,
        version,
        keep_alive,
        json::serialize(body),
        "application/json");
    }
    
    static StringResponse MakeErrorResponse(
      http::status status,
      unsigned version,
      bool keep_alive,
      std::string_view code,
      std::string_view message) {
      return MakeJsonResponse(
        status,
        version,
        keep_alive,
        json::object{
          {"code", code},
          {"message", message},
        });
    }
    
    static beast::string_view GetMimeType(
      const std::filesystem::path& path) {
      static const std::unordered_map< std::string, beast::string_view> types{
        {".htm", "text/html"},
        {".html", "text/html"},
        {".css", "text/css"},
        {".txt", "text/plain"},
        {".js", "application/javascript"},
        {".json", "application/json"},
        {".xml", "application/xml"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".bmp", "image/bmp"},
        {".ico", "image/vnd.microsoft.icon"},
        {".tiff", "image/tiff"},
        {".tif", "image/tiff"},
        {".svg", "image/svg+xml"},
        {".svgz", "image/svg+xml"},
        {".mp3", "audio/mpeg"},
      };
      const std::string extension = path.extension().string();
      if (const auto it = types.find(extension); it != types.end()) {
        return it->second;
      }
      return "application/octet-stream";
    }
    
    static bool IsPathInsideRoot(
      const std::filesystem::path& static_root,
      const std::filesystem::path& path) {
      std::error_code ec;
      const auto root = std::filesystem::weakly_canonical(
        static_root,
        ec);
      if (ec) {
        return false;
      }
      const auto candidate = std::filesystem::weakly_canonical(
        path,
        ec);
      if (ec) {
        return false;
      }
      auto root_it = root.begin();
      auto path_it = candidate.begin();
      for (; root_it != root.end() && path_it != candidate.end();
         ++root_it, ++path_it) {
        if (*root_it != *path_it) {
          return false;
        }
      }
      return root_it == root.end();
    }
    
    FileResponse MakeFileResponse( const std::filesystem::path& path, unsigned version, bool keep_alive) const {
      beast::error_code ec;
      http::file_body::value_type body;
      body.open(path.c_str(), beast::file_mode::scan, ec);
      if (ec) {
        return {};
      }
      FileResponse response{http::status::ok, version};
      response.set(http::field::content_type, GetMimeType(path));
      response.content_length(body.size());
      response.keep_alive(keep_alive);
      response.body() = std::move(body);
      return response;
    }
    
    template <typename Send>
    void HandleStaticRequest( const http::request<http::string_body>& req, Send&& send) const {
      beast::string_view target = req.target();
      if (target.empty() || target.front() != '/') {
        return send(MakeErrorResponse(
          http::status::bad_request,
          req.version(),
          req.keep_alive(),
          "badRequest",
          "Bad request"));
      }
      target.remove_prefix(1);
      if (target.empty()) {
        target = "index.html";
      }
      const std::filesystem::path file_path =
        static_root_ / std::filesystem::path{
          std::string{target.data(), target.size()}
        };
      std::error_code fs_ec;
      if (!IsPathInsideRoot(static_root_, file_path)
        || !std::filesystem::is_regular_file(file_path, fs_ec)) {
        return send(MakeErrorResponse(
          http::status::not_found,
          req.version(),
          req.keep_alive(),
          "notFound",
          "File not found"));
      }
      return send(MakeFileResponse(
        file_path,
        req.version(),
        req.keep_alive()));
    }
    
    static json::object SerializeRoad(const model::Road& road) {
      const model::Point start = road.GetStart();
      const model::Point end = road.GetEnd();
      if (road.IsHorizontal()) {
        return {
          {"x0", start.x},
          {"y0", start.y},
          {"x1", end.x},
        };
      }
      return {
        {"x0", start.x},
        {"y0", start.y},
        {"y1", end.y},
      };
    }
    
    static json::object SerializeBuilding(const model::Building& building) {
      const model::Rectangle& bounds = building.GetBounds();
      return {
        {"x", bounds.position.x},
        {"y", bounds.position.y},
        {"w", bounds.size.width},
        {"h", bounds.size.height},
      };
    }
    
    static json::object SerializeOffice(const model::Office& office) {
      const model::Point position = office.GetPosition();
      const model::Offset offset = office.GetOffset();
      return {
        {"id", *office.GetId()},
        {"x", position.x},
        {"y", position.y},
        {"offsetX", offset.dx},
        {"offsetY", offset.dy},
      };
    }
    
    static json::object SerializeMap(const model::Map& map) {
      json::array roads;
      roads.reserve(map.GetRoads().size());
      for (const model::Road& road : map.GetRoads()) {
        roads.emplace_back(SerializeRoad(road));
      }
      json::array buildings;
      buildings.reserve(map.GetBuildings().size());
      for (const model::Building& building : map.GetBuildings()) {
        buildings.emplace_back(SerializeBuilding(building));
      }
      json::array offices;
      offices.reserve(map.GetOffices().size());
      for (const model::Office& office : map.GetOffices()) {
        offices.emplace_back(SerializeOffice(office));
      }
      return {
        {"id", *map.GetId()},
        {"name", map.GetName()},
        {"roads", std::move(roads)},
        {"buildings", std::move(buildings)},
        {"offices", std::move(offices)},
      };
    }
    
    StringResponse MakeMapsResponse(
      unsigned version,
      bool keep_alive) const {
      json::array maps;
      maps.reserve(game_.GetMaps().size());
      for (const model::Map& map : game_.GetMaps()) {
        maps.emplace_back(json::object{
          {"id", *map.GetId()},
          {"name", map.GetName()},
        });
      }
      return MakeJsonResponse(
        http::status::ok,
        version,
        keep_alive,
        std::move(maps));
    }
    
    static StringResponse MakeMapResponse(
      const model::Map& map,
      unsigned version,
      bool keep_alive) {
      return MakeJsonResponse(
        http::status::ok,
        version,
        keep_alive,
        SerializeMap(map));
    }
    
    model::Game& game_;
    std::filesystem::path static_root_;
    
  public:
    RequestHandler(
      model::Game& game,
      std::filesystem::path static_root)
      : game_{game}
      , static_root_{std::move(static_root)} {
    }
    
    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;
    
    template <typename Body, typename Allocator, typename Send>
    void operator()(
      http::request<Body, http::basic_fields<Allocator>>&& req,
      Send&& send) {
      if (req.method() != http::verb::get) {
        return send(MakeErrorResponse(
          http::status::method_not_allowed,
          req.version(),
          req.keep_alive(),
          "invalidMethod",
          "Only GET method is expected"));
      }
      const beast::string_view target = req.target();
      if (target == "/api/v1/maps") {
        return send(MakeMapsResponse(
          req.version(),
          req.keep_alive()));
      }
      constexpr beast::string_view maps_prefix = "/api/v1/maps/";
      if (target.starts_with(maps_prefix)) {
        const beast::string_view map_id =
          target.substr(maps_prefix.size());
        if (map_id.empty()
          || map_id.find('/') != beast::string_view::npos) {
          return send(MakeErrorResponse(
            http::status::bad_request,
            req.version(),
            req.keep_alive(),
            "badRequest",
            "Bad request"));
        }
        const model::Map* map = game_.FindMap(
          model::Map::Id{
            std::string{map_id.data(), map_id.size()}
          });
        if (map == nullptr) {
          return send(MakeErrorResponse(
            http::status::not_found,
            req.version(),
            req.keep_alive(),
            "mapNotFound",
            "Map not found"));
        }
        return send(MakeMapResponse(
          *map,
          req.version(),
          req.keep_alive()));
      }
      return HandleStaticRequest(req, std::forward<Send>(send));
    }
  };
} // namespace http_handler
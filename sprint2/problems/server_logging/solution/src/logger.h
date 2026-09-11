#pragma once

#include <memory>
#include <string>
#include <iostream>
#include <boost/json.hpp>
#include <boost/optional.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/expressions.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
namespace json = boost::json;

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", boost::json::value)

namespace logger {

  inline void InitLogger() {
    logging::add_common_attributes();
    auto sink = boost::make_shared<sinks::synchronous_sink<
      sinks::text_ostream_backend>>();
    sink->locked_backend()->add_stream(
      boost::shared_ptr<std::ostream>(&std::cout, boost::null_deleter{}));
    sink->set_formatter([](
      const logging::record_view& record,
      logging::formatting_ostream& stream) {
      json::object result;
      const auto timestamp_value = record[timestamp];
      if (timestamp_value) {
          result["timestamp"] =
              boost::posix_time::to_iso_extended_string(timestamp_value.get());
      }
      const auto message = record[expr::smessage];
      result["message"] = message ? message.get() : "";
      const auto data = record[additional_data];
      if (data && data.get().is_object()) {
          result["data"] = data.get();
      } else {
          result["data"] = json::object{};
      }
      stream << json::serialize(result);
    });
    logging::core::get()->add_sink(sink);
  }

  inline void LogError(
    const boost::system::error_code& ec,
    std::string_view where) {
    BOOST_LOG_TRIVIAL(error)
      << logging::add_value(
           additional_data,
           json::object{
             {"code", ec.value()},
             {"text", ec.message()},
             {"where", where},
           })
      << "error";
  }

}  // namespace logger
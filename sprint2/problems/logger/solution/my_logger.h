#pragma once

#include <mutex>
#include <chrono>
#include <string>
#include <thread>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <optional>
#include <string_view>

using namespace std::literals;
#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
  mutable std::mutex mutex_;
  std::optional<std::chrono::system_clock::time_point> manual_ts_;
  std::ofstream log_file_;
  std::string opened_file_date_;

  Logger() = default;

  std::chrono::system_clock::time_point GetTimeUnlocked() const {
    if (manual_ts_) {
      return *manual_ts_;
    }
    return std::chrono::system_clock::now();
  }

  static std::tm ToLocalTime(std::time_t time) {
    std::tm result{};
#ifdef _WIN32
    // MSVC / Windows
    if (localtime_s(&result, &time) != 0) {
      throw std::runtime_error("localtime_s failed");
    }
#else
    // Linux / POSIX, включая путь /var/log
    if (localtime_r(&time, &result) == nullptr) {
      throw std::runtime_error("localtime_r failed");
    }
#endif
    return result;
  }

  std::string GetTimeStampUnlocked(
    std::chrono::system_clock::time_point now) const {
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    const std::tm local_time = ToLocalTime(time);
    std::ostringstream out;
    out << std::put_time(&local_time, "%F %T");
    return out.str();
  }

  std::string GetFileTimeStampUnlocked(
    std::chrono::system_clock::time_point now) const {
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    const std::tm local_time = ToLocalTime(time);
    std::ostringstream out;
    out << std::put_time(&local_time, "%Y_%m_%d");
    return out.str();
  }

  void OpenFileForDateUnlocked(const std::string& date) {
    if (date == opened_file_date_ && log_file_.is_open()) {
      return;
    }
    if (log_file_.is_open()) {
      log_file_.close();
    }
    const std::string filename =
      "/var/log/sample_log_" + date + ".log";
    log_file_.open(filename, std::ios::out | std::ios::app);
    if (!log_file_.is_open()) {
      throw std::runtime_error(
        "Cannot open log file: " + filename
      );
    }
    opened_file_date_ = date;
  }
  
public:
  static Logger& GetInstance() {
    static Logger obj;
    return obj;
  }

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  template <class... Ts>
  void Log(const Ts&... args) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = GetTimeUnlocked();
    const std::string file_date = GetFileTimeStampUnlocked(now);
    OpenFileForDateUnlocked(file_date);
    std::ostringstream line;
    line << GetTimeStampUnlocked(now) << ": ";
    (line << ... << args);
    line << '\n';
    log_file_ << line.str();
    log_file_.flush();
    if (!log_file_) {
      throw std::runtime_error("Failed to write to log file");
    }
  }

  void SetTimestamp(std::chrono::system_clock::time_point ts) {
    std::lock_guard<std::mutex> lock(mutex_);
    manual_ts_ = ts;
  }
};
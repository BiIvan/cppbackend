#pragma once

#include <miniaudio.h>

#include <chrono>
#include <limits>
#include <string>
#include <thread>
#include <vector>
#include <cstddef>
#include <cstring>
#include <utility>
#include <algorithm>
#include <stdexcept>

class Recorder {
  static void Callback(ma_device* device,void* /* output */,const void* input,ma_uint32 frame_count) {
    auto* self = static_cast<Recorder*>(device->pUserData);
    if (self != nullptr && input != nullptr) {
      self->SaveBuffer(input, frame_count);
    }
  }

  void SaveBuffer(const void* input, ma_uint32 frame_count) noexcept {
    if (!is_recording_ || current_offset_bytes_ >= buffer_.size()) {
      return;
    }
    const std::size_t requested_bytes{ static_cast<std::size_t>(frame_count) * frame_size_};
    const std::size_t available_bytes{ buffer_.size() - current_offset_bytes_};
    const std::size_t bytes_to_copy{ std::min(requested_bytes, available_bytes)};
    std::memcpy(buffer_.data() + current_offset_bytes_,input,bytes_to_copy);
    current_offset_bytes_ += bytes_to_copy;
  }

  static void Check(ma_result result, const char* operation) {
    if (result != MA_SUCCESS) {
      throw std::runtime_error(std::string(operation) + " failed: " + ma_result_description(result));
    }
  }

  ma_device device_{};
  ma_format format_{};
  ma_uint32 channels_{};
  ma_uint32 sample_rate_{};
  std::size_t frame_size_{};
  std::vector<char> buffer_;
  std::size_t current_offset_bytes_{ 0};
  bool is_recording_{ false};
  bool device_initialized_{ false};
  
public:
  struct RecordingResult {
    std::vector<char> data;
    std::size_t frames{ 0};
  };

  Recorder(ma_format format,ma_uint32 channels,ma_uint32 sample_rate = 44100)
    : format_(format),
      channels_(channels),
      sample_rate_(sample_rate),
      frame_size_(ma_get_bytes_per_frame(format, channels)) {
    if (frame_size_ == 0) {
      throw std::invalid_argument("Unsupported recording format or channel count");
    }
    ma_device_config config =
      ma_device_config_init(ma_device_type_capture);
    config.capture.format = format_;
    config.capture.channels = channels_;
    config.sampleRate = sample_rate_;
    config.dataCallback = &Recorder::Callback;
    config.pUserData = this;
    Check(ma_device_init(nullptr, &config, &device_),"ma_device_init(capture)");
    device_initialized_ = true;
  }

  Recorder(const Recorder&) = delete;
  Recorder& operator=(const Recorder&) = delete;

  Recorder(Recorder&&) = delete;
  Recorder& operator=(Recorder&&) = delete;

  ~Recorder() {
    if (device_initialized_) {
      ma_device_stop(&device_);
      ma_device_uninit(&device_);
    }
  }

  template <typename Rep, typename Period>
  RecordingResult Record(std::size_t max_frames,std::chrono::duration<Rep, Period> duration) {
    if (max_frames == 0) {
      return {};
    }
    buffer_.assign(max_frames * frame_size_, 0);
    current_offset_bytes_ = 0;
    is_recording_ = true;
    Check(ma_device_start(&device_),"ma_device_start(capture)");
    std::this_thread::sleep_for(duration);
    Check(ma_device_stop(&device_),"ma_device_stop(capture)");
    is_recording_ = false;
  const std::size_t bytes_recorded { current_offset_bytes_};
    buffer_.resize(bytes_recorded);
    RecordingResult result{std::move(buffer_),bytes_recorded / frame_size_};
    current_offset_bytes_ = 0;
    return result;
  }

  [[nodiscard]] std::size_t GetFrameSize() const noexcept {
    return frame_size_;
  }

  [[nodiscard]] ma_uint32 GetSampleRate() const noexcept {
    return sample_rate_;
  }
};

class Player {
  static void Callback(ma_device* device,void* output,const void* /* input */,ma_uint32 frame_count) {
    auto* self{ static_cast<Player*>(device->pUserData)};
    if (self == nullptr || output == nullptr) {
      return;
    }
    self->ReadToOutput(output, frame_count);
  }

  void ReadToOutput(void* output,ma_uint32 frame_count) noexcept {
    auto* destination{ static_cast<ma_uint8*>(output)};
    ma_uint32 total_read{ 0};
    while (total_read < frame_count) {
      ma_uint32 readable_frames{ frame_count - total_read};
      void* read_ptr{ nullptr};
      const ma_result acquire_result{ ma_pcm_rb_acquire_read(
                      &ring_buffer_,&readable_frames,&read_ptr)};
      if (acquire_result != MA_SUCCESS || 
          read_ptr == nullptr ||
          readable_frames == 0) {
        break;
      }
      std::memcpy(
        destination + static_cast<std::size_t>(total_read) * frame_size_,
        read_ptr, static_cast<std::size_t>(readable_frames) * frame_size_
      );
      const ma_result commit_result{ ma_pcm_rb_commit_read(
                      &ring_buffer_, readable_frames)};
      if (commit_result != MA_SUCCESS) {
        break;
      }
      total_read += readable_frames;
    }
    if (total_read < frame_count) {
      ma_uint8* silence_begin{ destination + static_cast<std::size_t>(total_read) * frame_size_};
      ma_silence_pcm_frames(silence_begin,frame_count - total_read,format_,channels_);
    }
  }

  static void Check(ma_result result, const char* operation) {
    if (result != MA_SUCCESS) {
      throw std::runtime_error(std::string(operation) + " failed: " + ma_result_description(result));
    }
  }

  ma_device device_{};
  ma_pcm_rb ring_buffer_{};
  ma_format format_{};
  ma_uint32 channels_{};
  ma_uint32 sample_rate_{};
  std::size_t frame_size_{};
  bool device_initialized_{ false};
  bool ring_buffer_initialized_{ false};
  
public:
  Player(ma_format format,ma_uint32 channels,ma_uint32 sample_rate = 44100,std::size_t buffer_duration_ms = 500)
    : format_(format),
      channels_(channels),
      sample_rate_(sample_rate),
      frame_size_(ma_get_bytes_per_frame(format, channels)) {
    if (frame_size_ == 0) {
      throw std::invalid_argument("Unsupported playback format or channel count");
    }
    const std::size_t capacity_frames =
      std::max<std::size_t>( 1, static_cast<std::size_t>(sample_rate_) * buffer_duration_ms / 1000 );
    if (capacity_frames > static_cast<std::size_t>(std::numeric_limits<ma_uint32>::max())) {
      throw std::invalid_argument("Playback ring-buffer capacity is too large");
    }
    Check(
      ma_pcm_rb_init(
        format_,
        channels_,
        static_cast<ma_uint32>(capacity_frames),
        nullptr,
        nullptr,
        &ring_buffer_
      ),
      "ma_pcm_rb_init"
    );
    ring_buffer_initialized_ = true;
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = format_;
    config.playback.channels = channels_;
    config.sampleRate = sample_rate_;
    config.dataCallback = &Player::Callback;
    config.pUserData = this;
    const ma_result device_result = ma_device_init(nullptr, &config, &device_);
    if (device_result != MA_SUCCESS) {
      ma_pcm_rb_uninit(&ring_buffer_);
      ring_buffer_initialized_ = false;
      throw std::runtime_error(
        std::string("ma_device_init(playback) failed: ") +
        ma_result_description(device_result)
      );
    }
    device_initialized_ = true;
  }

  Player(const Player&) = delete;
  Player& operator=(const Player&) = delete;

  Player(Player&&) = delete;
  Player& operator=(Player&&) = delete;

  ~Player() {
    if (device_initialized_) {
      ma_device_stop(&device_);
      ma_device_uninit(&device_);
    }
    if (ring_buffer_initialized_) {
      ma_pcm_rb_uninit(&ring_buffer_);
    }
  }

  void Start() {
    Check(ma_device_start(&device_),"ma_device_start(playback)");
  }

  void Stop() {
    Check(ma_device_stop(&device_),"ma_device_stop(playback)");
  }

  [[nodiscard]] std::size_t GetFrameSize() const noexcept {
    return frame_size_;
  }

  [[nodiscard]] ma_uint32 GetSampleRate() const noexcept {
    return sample_rate_;
  }

  [[nodiscard]] std::size_t BufferedFrames() noexcept {
    return ma_pcm_rb_available_read(&ring_buffer_);
  }

  std::size_t PushBuffer(const char* data, std::size_t frames) {
    if (data == nullptr || frames == 0) {
      return 0;
    }
    std::size_t total_written = 0;
    while (total_written < frames) {
      const std::size_t remaining_frames = frames - total_written;
      const ma_uint32 requested_frames =
        static_cast<ma_uint32>(
          std::min<std::size_t>(
            remaining_frames,
            static_cast<std::size_t>(
              std::numeric_limits<ma_uint32>::max()
            )
          )
        );
      ma_uint32 writable_frames = requested_frames;
      void* write_ptr = nullptr;
      const ma_result acquire_result =
        ma_pcm_rb_acquire_write(
          &ring_buffer_,
          &writable_frames,
          &write_ptr
        );
      Check(acquire_result, "ma_pcm_rb_acquire_write");
      if (write_ptr == nullptr || writable_frames == 0) {
        break;
      }
      const char* source = data + total_written * frame_size_;
      std::memcpy(
        write_ptr,
        source,
        static_cast<std::size_t>(writable_frames) *
          frame_size_
      );
    const ma_result commit_result{ ma_pcm_rb_commit_write( &ring_buffer_,writable_frames)};
      Check(commit_result, "ma_pcm_rb_commit_write");
      total_written += writable_frames;
    }
    return total_written;
  }
};
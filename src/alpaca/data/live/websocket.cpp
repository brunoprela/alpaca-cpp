#include "alpaca/data/live/websocket.hpp"

#include <chrono>
#include <stdexcept>
#include <thread>

namespace alpaca::data::live {

DataStream::DataStream(std::string endpoint, std::string api_key, std::string secret_key,
                       bool raw_data)
    : endpoint_(std::move(endpoint)), api_key_(std::move(api_key)),
      secret_key_(std::move(secret_key)), raw_data_(raw_data) {}

DataStream::~DataStream() {
    stop();
}

void DataStream::run() {
    if (worker_thread_ && worker_thread_->joinable()) {
        return; // Already running
    }
    should_run_ = true;
    worker_thread_ = std::make_unique<std::thread>(&DataStream::run_loop, this);
}

void DataStream::run_loop() {
    while (should_run_) {
        try {
            if (!running_) {
                connect_impl();
                authenticate_impl();
                send_subscribe_message_impl();
                running_ = true;
            }
            consume_messages_impl();
        } catch (const std::exception &e) {
            running_ = false;
            close_impl();
            if (should_run_) {
                // Wait a bit before reconnecting
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }
}

void DataStream::stop() {
    should_run_ = false;
    close();
    if (worker_thread_ && worker_thread_->joinable()) {
        worker_thread_->join();
    }
}

void DataStream::close() {
    running_ = false;
    close_impl();
}

} // namespace alpaca::data::live


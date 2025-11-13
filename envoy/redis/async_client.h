#pragma once

#include <chrono>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace Envoy {

namespace Event {

class Dispatcher;
}

namespace Redis {

struct AsyncClientConfig {
public:
  AsyncClientConfig(std::string&& username, std::string&& password, int op_timeout_milliseconds,
                    std::map<std::string, std::string>&& params)
      : auth_username_(std::move(username)), auth_password_(std::move(password)),
        op_timeout_(op_timeout_milliseconds),
        max_buffer_size_before_flush_(parseUint32FromParams(params, "max_buffer_size_before_flush", 1024)),
        buffer_flush_timeout_(parseUint32FromParams(params, "buffer_flush_timeout", 3)),
        params_(std::move(params)) {
  }

  const std::string auth_username_;
  const std::string auth_password_;

  const std::chrono::milliseconds op_timeout_;
  const uint32_t max_buffer_size_before_flush_;
  const std::chrono::milliseconds buffer_flush_timeout_;
  const uint32_t max_upstream_unknown_connections_{100};
  const bool enable_command_stats_{false};
  const std::map<std::string, std::string> params_;

private:
  // Helper function to parse uint32 from params map with default value
  static uint32_t parseUint32FromParams(const std::map<std::string, std::string>& params,
                                         const std::string& key, uint32_t default_value) {
    auto it = params.find(key);
    if (it != params.end()) {
      try {
        unsigned long value = std::stoul(it->second);
        if (value <= std::numeric_limits<uint32_t>::max()) {
          return static_cast<uint32_t>(value);
        }
      } catch (const std::exception&) {
        // If parsing fails, return default value
      }
    }
    return default_value;
  }
};

/**
 * A handle to an outbound request.
 */
class PoolRequest {
public:
  virtual ~PoolRequest() = default;

  /**
   * Cancel the request. No further request callbacks will be called.
   */
  virtual void cancel() PURE;
};

class AsyncClient {
public:
  class Callbacks {
  public:
    virtual ~Callbacks() = default;

    virtual void onSuccess(std::string_view query, std::string&& response) PURE;

    virtual void onFailure(std::string_view query) PURE;
  };

  virtual ~AsyncClient() = default;

  virtual void initialize(AsyncClientConfig config) PURE;

  virtual PoolRequest* send(std::string&& query, Callbacks& callbacks) PURE;

  virtual Event::Dispatcher& dispatcher() PURE;
};

using AsyncClientPtr = std::unique_ptr<AsyncClient>;

} // namespace Redis
} // namespace Envoy

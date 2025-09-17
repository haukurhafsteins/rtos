#pragma once
#include <string>
#include <string_view>
#include <memory>
#include "rtos/config/Result.hpp"

namespace rtos::config {
struct ConfigStore {
  virtual ~ConfigStore() = default;
  virtual Result<std::string> read(const char* key) = 0;
  virtual Result<void>        write_atomic(const char* key, std::string_view data) = 0;
};

// File-backed store (atomic writes via tmp + fsync + rename)
class FsStore : public ConfigStore {
public:
  explicit FsStore(const char* base_dir) : base_(base_dir?base_dir:"") {}

  Result<std::string> read(const char* key) override;
  Result<void> write_atomic(const char* key, std::string_view data) override;
private:
  std::string join(const char* key) const;
  std::string base_;
};
} // namespace rtos::config
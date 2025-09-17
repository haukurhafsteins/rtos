#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include "rtos/Mutex.hpp"
#include "config/ConfigStore.hpp"
template<class T>
class ConfigManager {
public:
  using ValidationFn = bool(*)(const T&, Error&); // additional app-level checks

  ConfigManager(const char* name,
                std::unique_ptr<ConfigStore> store,
                std::unique_ptr<JsonCodec<T>> codec,
                ValidationFn extra_validate = nullptr)
  : name_(name),
    store_(std::move(store)),
    codec_(std::move(codec)),
    validate_(extra_validate) {}

  // Load from store, migrate, validate, apply.
  Result<void> load() {
    auto r = store_->read(name_);
    if (!r) return Result<void>::fail(name_, "read failed");

    std::string json = std::move(r.value);
    int ver = 0;
    if (!codec_->migrate_inplace(json, ver))
      return Result<void>::fail("version", "migration failed");

    auto cfgR = codec_->decode(json);
    if (!cfgR) return Result<void>::fail(cfgR.error.where, cfgR.error.msg);

    Error e{};
    if (validate_ && !validate_(cfgR.value, e))
      return Result<void>::fail(e.where, e.msg);

    return apply_swap(std::move(cfgR.value));
  }

  // Apply a JSON string (e.g., from REST). Does not persist unless asked.
  Result<void> apply_json(std::string_view json, bool persist) {
    auto cfgR = codec_->decode(json);
    if (!cfgR) return Result<void>::fail(cfgR.error.where, cfgR.error.msg);

    Error e{};
    if (validate_ && !validate_(cfgR.value, e))
      return Result<void>::fail(e.where, e.msg);

    auto ar = apply_swap(cfgR.value);
    if (!ar) return ar;

    if (persist) {
      auto enc = codec_->encode(current());
      if (!enc) return Result<void>::fail(enc.error.where, enc.error.msg);
      auto wr = store_->write_atomic(name_, enc.value);
      if (!wr) return wr;
    }
    return Result<void>::ok({});
  }

  const T& current() const {
    std::lock_guard<Mutex> lk(m_);
    return cfg_; // return-by-reference; keep access short
  }

  void add_observer(ConfigObserver<T>* obs) {
    std::lock_guard<Mutex> lk(m_);
    observers_.push_back(obs);
  }

private:
  Result<void> apply_swap(T next) {
    // notify observers *without* holding the lock too long:
    // 1) copy list, 2) test-apply, 3) commit.
    std::vector<ConfigObserver<T>*> obsCopy;
    {
      std::lock_guard<Mutex> lk(m_);
      obsCopy = observers_;
    }
    for (auto* o : obsCopy) {
      if (o && !o->on_config_apply(next))
        return Result<void>::fail(name_, "observer veto");
    }
    {
      std::lock_guard<Mutex> lk(m_);
      cfg_ = std::move(next);
    }
    return Result<void>::ok({});
  }

  // wire to your RTOS mutex
  using Mutex = rtos::Mutex;

  const char* name_;
  std::unique_ptr<ConfigStore>   store_;
  std::unique_ptr<JsonCodec<T>>  codec_;
  ValidationFn                   validate_;
  mutable Mutex                  m_;
  T                              cfg_{};
  std::vector<ConfigObserver<T>*> observers_;
};

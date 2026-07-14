#pragma once

#include <cstdint>
#include <cstddef>

// ---------------------------------------------------------------------------
// Portable non-volatile key/value storage abstraction.
//
// Modelled as a string-keyed key/value store grouped into namespaces — the
// ESP-IDF NVS model. On Zephyr the same shape maps onto the settings
// subsystem ("namespace/key" paths backed by NVS/ZMS); Zephyr's raw fs/nvs
// API is numeric-ID based and is not used.
//
// This header declares the complete interface. Each backend implements the
// methods in backends/<backend>/rtos_nvs.cpp; the build system selects which
// backend is compiled.
//
// Backend support:
//   - espidf: full implementation (nvs_flash component)
//   - zephyr: compile-only stub, all operations return false
//   - linux:  compile-only stub, all operations return false
// ---------------------------------------------------------------------------

namespace rtos::nvs
{

/// Initialise the default NVS partition. If the partition is corrupt or has
/// an incompatible layout it is erased and initialisation retried.
/// Returns true when storage is usable.
bool init();

/// Tear down the default NVS partition. Safe to call when not initialised.
void deinit();

/// Erase the entire default NVS partition (all namespaces).
/// Call init() again afterwards before using storage.
bool erase();

} // namespace rtos::nvs

namespace rtos
{

// ---------------------------------------------------------------------------
// Nvs
// One open namespace. Non-copyable, movable. Closes on destruction.
// ---------------------------------------------------------------------------
class Nvs
{
public:
    enum class Mode
    {
        ReadOnly,
        ReadWrite,
    };

    Nvs() = default;

    Nvs(const Nvs&)            = delete;
    Nvs& operator=(const Nvs&) = delete;

    Nvs(Nvs&& other) noexcept
        : handle_(other.handle_), opened_(other.opened_)
    { other.handle_ = 0; other.opened_ = false; }

    Nvs& operator=(Nvs&& other) noexcept
    {
        if (this != &other)
        {
            close();
            handle_       = other.handle_;
            opened_       = other.opened_;
            other.handle_ = 0;
            other.opened_ = false;
        }
        return *this;
    }

    ~Nvs() { close(); }

    /// Open namespace `nspace` (e.g. "URU"). Returns true on success.
    bool open(const char* nspace, Mode mode = Mode::ReadWrite);

    /// Close the namespace. Pending writes that were not committed are lost.
    /// Safe to call when not opened.
    void close();

    bool opened() const { return opened_; }

    // -----------------------------------------------------------------------
    // Getters — return true when the key exists and the value fits.
    // -----------------------------------------------------------------------

    /// Read a NUL-terminated string. `inout_size` is the buffer size on entry
    /// and the stored length (including the NUL) on success.
    bool get_str(const char* key, char* out, size_t& inout_size);

    /// Read a binary blob. `inout_size` is the buffer size on entry and the
    /// stored length on success.
    bool get_blob(const char* key, void* out, size_t& inout_size);

    bool get_u32(const char* key, uint32_t& out);
    bool get_i32(const char* key, int32_t& out);

    // -----------------------------------------------------------------------
    // Setters — return true on success. Call commit() to flush to flash.
    // -----------------------------------------------------------------------

    bool set_str(const char* key, const char* value);
    bool set_blob(const char* key, const void* data, size_t size);
    bool set_u32(const char* key, uint32_t value);
    bool set_i32(const char* key, int32_t value);

    /// Remove a key from the namespace.
    bool erase_key(const char* key);

    /// Flush pending writes to flash.
    bool commit();

private:
    uintptr_t handle_ = 0;
    bool      opened_ = false;
};

} // namespace rtos

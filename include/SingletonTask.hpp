// SingletonTaskTemplate.hpp
#pragma once
#include "rtos_assert.hpp"

template <typename Derived>
class SingletonTask {
public:
    static Derived& get() {
        RTOS_ENSURE(_instance != nullptr);
        return *_instance;
    }

    static void bind(Derived& instance) {
        // prevent accidental rebinds
        RTOS_ENSURE(_instance == nullptr);
        _instance = &instance;
    }

    static bool is_bound() { return _instance != nullptr; }

protected:
    SingletonTask() = default;
    static inline Derived* _instance = nullptr; // C++17 inline var avoids separate definition
};

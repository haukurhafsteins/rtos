// SingletonTaskTemplate.hpp
#pragma once

// Use this template to create singleton-style tasks with configurable construction
// It supports manual lifetime, compile-time safety, and easy reuse across tasks.

template <typename Derived>
class SingletonTask {
public:
    static Derived& get() {
        configASSERT(_instance); // or custom assert/log
        return *_instance;
    }

    static void bind(Derived& instance) {
        _instance = &instance;
    }

protected:
    SingletonTask() = default;
    static Derived* _instance;
};

// Define the static pointer in your .cpp file like:
// template<> AhrsTask* SingletonTask<AhrsTask>::_instance = nullptr;

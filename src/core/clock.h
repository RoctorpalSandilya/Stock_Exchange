#pragma once
#include <chrono>
#if defined(_MSC_VER) || defined(__x86_64__) || defined(__i386__)
#include <intrin.h>
#endif
#include "core/types.h"

namespace exchange::core {

    class LogicalClock {
    public:
        [[nodiscard]] Timestamp next() noexcept { return ++current_; } // increment the current timestamp and return it
        [[nodiscard]] Timestamp current() const noexcept { return current_; } // return the current timestamp
        void reset() noexcept { current_ = 0; } // reset the current timestamp to 0

    private:
        Timestamp current_{0};
    };

    [[nodiscard]] inline Timestamp steady_timestamp_now() noexcept {
        const auto duration = std::chrono::steady_clock::now().time_since_epoch(); // get the current time since epoch
        return static_cast<Timestamp>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count()); // convert the duration to nanoseconds
    }

    [[nodiscard]] inline Timestamp rdtsc() noexcept {
#if defined(_MSC_VER) || defined(__x86_64__) || defined(__i386__)
        return __rdtsc();
#else
        return steady_timestamp_now();
#endif
    }

} // namespace exchange::core
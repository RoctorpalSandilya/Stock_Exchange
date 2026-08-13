#pragma once
#include <chrono>
#include "core/types.h"

namespace exchange::core{
    class LogicalClock{
        public:
            [[nodiscard]] Timestamp next() noexcept{ return ++current_; } // increment the current timestamp and return it
            [[nodiscard]] Timestamp current() const noexcept{ return current_; } // return the current timestamp
            void reset() noexcept{ current_= 0; } // reset the current timestamp to 0
        
        private:
            Timestamp current_{0};
    };

    [[nodiscard]] inline Timestamp steady_timestamp_now() noexcept{
        //check why steady clock is used instead of system clock, steady clock is monotonic and not affected by system clock changes, so it is more suitable for measuring time intervals
        const auto duration= std::chrono::steady_clock::now().time_since_epoch(); // get the current time since epoch
        return static_cast<Timestamp>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count()); // convert the duration to nanoseconds and return it as a Timestamp
    }

}
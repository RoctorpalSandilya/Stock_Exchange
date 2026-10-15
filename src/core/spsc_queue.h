/** 
//We want a queue where producer and consumer nvr uses lock while pushing or popping from queue
//They nvr block each other, nvr calls the kernel and nvr touches the heap
//The std::queue is not suitable for this purpose as it uses locks and heap memory allocation
//We will implement a single producer single consumer queue using a circular buffer
 **/

#pragma once

#include <cstdint>
#include <atomic>
#include <cstddef>
#include <array>
#include <type_traits>

namespace exchange::core {

    template<typename T, std::size_t N> class SPSCQueue {
        static_assert(N > 1, "Queue must be at least 2 slots");
        static_assert((N & (N - 1)) == 0, "Queue size must be a power of 2"); // % ops is expensive, N being a power of 2 allows us to replace that with & (N-1)
        static_assert(std::is_trivially_copyable_v<T>, "Queue can only hold trivially copyable types");

        struct alignas(128) Cursor {
            std::atomic<std::uint64_t> value{0}; // index of the head or tail of the circular buffer
        };

    public:
        bool push(const T& value) noexcept {
            const std::uint64_t tail = tail_.value.load(std::memory_order_relaxed);
            const std::uint64_t head = head_.value.load(std::memory_order_acquire);
            if ((tail - head) >= N) [[unlikely]] { return false; }
            buffer_[tail & MASK] = value;
            tail_.value.store(tail + 1U, std::memory_order_release);
            return true;
        }

        bool pop(T& value) noexcept {
            const std::uint64_t head = head_.value.load(std::memory_order_relaxed);
            const std::uint64_t tail = tail_.value.load(std::memory_order_acquire);
            if ((tail - head) == 0) [[unlikely]] { return false; }
            value = buffer_[head & MASK];
            head_.value.store(head + 1U, std::memory_order_release);
            return true;
        }

        [[nodiscard]] bool empty() const noexcept {
            return head_.value.load(std::memory_order_acquire) == tail_.value.load(std::memory_order_acquire);
        }

    private:
        static constexpr std::size_t MASK = N - 1U;
        std::array<T, N> buffer_{};
        Cursor head_{};
        Cursor tail_{};
    };

} // namespace exchange::core
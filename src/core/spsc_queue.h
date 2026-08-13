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

 namespace exchange::core{
    template<typename T, std::size_t N> class SPSCQueue{
        static_assert(N > 1, "Queue must be atleast 2 slots");
        static_assert(N&(N-1) == 0, "Queue size must be a power of 2"); // % ops is expensive, N being a power of 2 allows us to replace that with & (N-1)(compiler does that internally)
        static_assert(std::is_trivially_copyable_v<T>, "Queue can only hold trivially copyable types");// makes copying and moving the data faster as it can be done using memcpy instead of calling copy constructor or move constructor

        //There is a chance of head and tail of the circular buffer being on the same cache line,(learn how it causes degradation)
        //which can cause false sharing and performance degradation, so we will align the head and tail to cache line size(128 bytes)
        //so both head and tail will be on different cache lines and will not cause false sharing
        //Here we used 128 because some modern CPUs fetch adjacent cache lines together, we want them in different cache lines
        struct alignas(128) Cursor{
            std::atomic<std::usize64_t> value{d:0}; // index of the head or tail of the circular buffer
        };

        public:
            bool push(const T& value) noexcept{
                const std::uint64_t tail= tail_.value.load(std::memory_order_relaxed); // load the tail of the circular buffer
                const std::uint64_t head= head_.value.load(std::memory_order_acquire); // load the head of the circular buffer
                if((tail - head) >= N) [[unlikely]] {return false;} // if the circular buffer is full, return false
                buffer_[tail & MASK]= value; // store the value in the circular buffer at the tail index
                tail_.value.store(tail + 1U, std::memory_order_release); // increment the tail of the circular buffer
                return true; // return true to indicate that the value was pushed successfully
            }

            bool pop(T& value) noexcept{
                const std::uint64_t head= head_.value.load(std::memory_order_relaxed); // load the head of the circular buffer
                const std::uint64_t tail= tail_.value.load(std::memory_order_acquire); // load the tail of the circular buffer
                if((tail - head) == 0) [[unlikely]] {return false;} // if the circular buffer is empty, return false
                value= buffer_[head & MASK]; // load the value from the circular buffer at the head index
                head_.value.store(head + 1U, std::memory_order_release); // increment the head of the circular buffer
                return true; // return true to indicate that the value was popped successfully
            }

            [[nodiscard]] bool empty() const noexcept{
                return head_.value.load(std::memory_order_acquire) == tail_.value.load(std::memory_order_acquire); // if the head and tail of the circular buffer are equal, the circular buffer is empty
            }

        private:
            static constexpr std::size_t MASK= N - 1U; // mask to wrap around the circular buffer
            std::array<T, N> buffer_{}; // circular buffer to hold the data
            Cursor head_{}; // head of the circular buffer
            Cursor tail_{}; // tail of the circular buffer
    };
 }
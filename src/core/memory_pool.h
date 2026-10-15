/**
//we want to avoid using the keyword 'new' and 'delete' in our code, so we implement a memory pool
  to manage memory allocation and deallocation
//as allocation and deallocation in heap memory is slow, we implement a memory pool to manage memory
  allocation and deallocation in stack memory, which is faster than heap memory
//finding new space with some predefined size in heap is linear and expensive process
//we will keep a table to keep track of the free and used memory blocks in the memory pool,
  so that we can quickly find a free block of memory to allocate and deallocate memory blocks 
  in the memory pool
//now if we somehow run out of space in RAM(no more free blocks available) we need to make a 
  syscall to switch to kernel mode so that more memory can be allocated to our program in RAM
//We have virutal memory which is mapped to physical memory and the mapping is stored in page table,
  if page for that virtual memory is not present in physical memory, then a page fault occurs and 
  the kernel will allocate a new page in physical memory and update the page table to map the 
  virtual memory to the new physical memory page(it is idone when RAM is full)
//once memory block is allocated the OS keeps a loockup table(TLB) to store the mapping instead
  of calculating the mapping every time, so that the mapping can be found quickly
//there is a lot of locking and unlocking when accessing the memory as multiple threads
  compete for it, which is expensive and takes time(ALL THOSE PROCESS ABOVE TAKES MICROSECONDS TO COMPLETE)
//There will be a lot of fragmentation in memory when new keyword is called 1000x times, heap will become ugly
//Also there will be a lot of cache misses, so we need to store the data in array so hardware
  prefether can prefetch the data in cache and reduce the cache misses(Spatial locality(other one is temporal locality))
//Learn Intrusive and Non-intrusive memory pool, we will implement intrusive memory pool as it is faster than non-intrusive memory pool
//We will be using our custom allocator and deallocator using non-intrusive memory pool, 
  so that we can avoid using the keyword 'new' and 'delete'(expensive) in our code
//LEARN HOW MALLOC WORKS AND ALLOCATES MEMORY IN HEAP
 **/

 #pragma once
 #include <cstddef>
 #include <memory>
 #include <array>

 namespace exchange::core{
    template<typename T, std::size_t N>/**A way to tell compiler at compile time the size of the memory pool, 
    if N is a power of 2 compiler can run idx&(N-1)(faster) instead of idx%N(slower) also 
    if as much compiler knows at compile time, it can optimize the code better */
    class MemoryPool {
        static_assert(N > 0, "Memory pool size must be greater than 0");
        static_assert(sizeof(T) >= sizeof(void*), "Object storage must fit an intrusive free-list pointer");
        
        //to avoid cache splitting we will align the memory pool to cache line size(64 bytes) and also align the storage of each order to cache line size(64 bytes)
        struct alignas ((alignof(T)> 64U) ? alignof(T) : 64U) Slot{
            std::array<std::byte, sizeof(T)> storage{}; // store one order or store next pointer to next free slot, so we use std::array to store the storage of one order
        };

        struct FreeNode{
            FreeNode* next; // pointer to next free slot in the memory pool
        };

        private:
            std::unique_ptr<Slot[]> slots_; // unique pointer to the memory pool
            FreeNode* free_list_head{nullptr}; // pointer to the head of the free list
            void initialize_free_list() noexcept{
                free_list_head= nullptr;
                for(std::size_t index=0; index<N; ++index){
                    auto *node= reinterpret_cast<FreeNode*>(slots_[index].storage.data()); // cast the storage of the slot to a FreeNode pointer
                    node->next= free_list_head; // set the next pointer of the node to the head of the free list
                    free_list_head= node; // set the head of the free list to the node
                }
            }
            std::size_t allocated_count{0}; // count of allocated slots in the memory pool
        public:
            MemoryPool() : slots_(std::make_unique<Slot[]>(N)) {
                initialize_free_list();
            }
            MemoryPool(const MemoryPool&) = delete; // disable copy constructor
            MemoryPool& operator=(const MemoryPool&) = delete; // disable copy assignment operator
            MemoryPool(MemoryPool&&) = delete; // disable move constructor
            MemoryPool& operator=(MemoryPool&&) = delete; // disable move assignment operator

            T* allocate() noexcept{
                if(free_list_head== nullptr) [[unlikely]]{ // if there are no free slots available in the memory pool{
                    return nullptr; // no free slots available
                }
                FreeNode* node= free_list_head; // get the head of the free list
                free_list_head= node->next; // set the head of the free list to the next node
                ++allocated_count; // increment the count of allocated slots
                return reinterpret_cast<T*>(node); // return the pointer to the allocated slot
            }

            void deallocate(T* object) noexcept{
                if(object== nullptr) [[unlikely]]{ // if the object is null, do nothing
                    return;
                }
                std::destroy_at(object); // destroy the object at the given pointer
                FreeNode* node= reinterpret_cast<FreeNode*>(object); // cast the object pointer to a FreeNode pointer
                node->next= free_list_head; // set the next pointer of the node to the
                free_list_head= node; // set the head of the free list to the node
                --allocated_count; // decrement the count of allocated slots
            }
    };
 }
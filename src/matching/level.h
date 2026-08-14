#pragma once
#include "matching/Order.h"

namespace exchange::matching{
    struct alignas(64) Level{
        core::Price price{0}; // price of the level
        core::Quantity total_qty{0}; // total quantity of the level
        std::uint32_t order_count{0}; // number of orders in the level
        Order* head{nullptr}; // pointer to the head order in the level
        Order* tail{nullptr}; // pointer to the tail order in the level

        void add_order(Order* order) noexcept{
            order->prev = tail;
            order->next = nullptr;
            order->parent_level = this;
            if(tail!=nullptr){
                tail->next = order;
            }
            else{
                head = order;
            }
            tail = order;
            total_qty += order->display_qty;
            order_count++;
        }
        void remove_order(Order* order){
            if(order->prev!=nullptr){
                order->prev->next = order->next;
            }
            else{
                head = order->next;
            }
            if(order->next!=nullptr){
                order->next->prev = order->prev;
            }
            else{
                tail = order->prev;
            }
            total_qty -= order->display_qty;
            order_count--;
            order->prev = nullptr;
            order->next = nullptr;
            order->parent_level = nullptr;
        }
        [[nodiscard]] Order* front() const noexcept{
            return head;
        }
        [[nodiscard]] Order* back() const noexcept{
            return tail;
        }
        [[nodiscard]] bool is_empty() const noexcept{
            return head == nullptr;
        }
    };
}
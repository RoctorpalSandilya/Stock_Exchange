#pragma once

#include <cstdint>
#include <algorithm>
#include <array>
#include <string_view>
#include <type_traits>

// we want to keep the code small so that entire code can fit into L1 cache, so we use fixed size types instead of variable size types
// thats why when it comes to latency small size is preferred

namespace exchange::core{
    using Price = std::int64_t; // doubles aren't stored exactly in memory, they are just approximations, so we use int64_t to store prices in multiplication of 10k
    using Quantity = std::uint32_t; // quantity is always a positive number, so we use uint32_t to store it
    using OrderId = std::uint64_t; // order id is always a positive number, so we use uint64_t to store it
    using SequenceNumber = std::uint64_t; // sequence number is always a positive number, so we use uint64_t to store it
    using Timestamp = std::uint64_t; // timestamp is always a positive number, so we use uint64_t to store it
    using ParticipantId = std::uint32_t; // participant id is always a positive number, so we use uint32_t to store it
    using MatchId = std::uint64_t; // match id is always a positive number, so we use uint64_t to store it
    
    inline constexpr Price PRICE_SCALE= 10000; // price is stored in multiplication of 10k, so we define a constant to scale it back to the original value
    
    enum class Side : std::uint8_t { BUY, SELL }; // side is either buy or sell, so we use enum class to define it
    
    enum class OrderType : std::uint8_t { 
        LIMIT, // limit order is an order to buy or sell a stock at a specific price or better
        MARKET, // market order is an order to buy or sell a stock at the best available price
        IOC, // immediate or cancel order is an order that must be executed immediately or cancelled
        FOK,  // fill or kill order is an order that must be executed in its entirety immediately or cancelled
        GTC, // good till cancelled order is an order that remains active until it is executed or cancelled
        STOP, // stop order is an order to buy or sell a stock once the price reaches a specified level
        STOP_LIMIT, // stop limit order is a combination of a stop order and a limit order
        ICEBERG, // iceberg order is an order that is only partially visible in the order book
        POST_ONLY, // post only order is an order that is only posted to the order book and not executed immediately
    };

    enum class OrderStatus : std::uint8_t { 
        NEW, // new order is an order that has been accepted by the exchange but not yet executed
        PARTIALLY_FILLED, // partially filled order is an order that has been partially executed
        ACCEPTED, // accepted order is an order that has been accepted by the exchange
        FILLED, // filled order is an order that has been completely executed
        CANCELED, // cancelled order is an order that has been cancelled by the user or the exchange
        REJECTED, // rejected order is an order that has been rejected by the exchange
    };

    // string generally takes more space(like heap memory address, size, capacity etc.) than a fixed size array, so we use a fixed size array to store the symbol of a stock
    using Symbol = std::array<char, 8>; // symbol is a string that represents the identifier of a stock(NVDA, AAPL etc.)

    constexpr Symbol make_symbol(std::string_view str) noexcept {
        Symbol symbol{};
        const auto size = std::min(str.size(), symbol.size());
        for(std::size_t i = 0; i < size; ++i) {
            symbol[i] = str[i];
        }
        return symbol;
    }

    constexpr std::string_view symbol_to_string(const Symbol& symbol) noexcept {
        std::size_t size = 0;
        for(std::size_t i = 0; i < symbol.size(); ++i) {
            if(symbol[i] == '\0') {
                break;
            }
            ++size;
        }
        return std::string_view(symbol.data(), size);
    }

    struct SymbolLess {
        bool operator()(const Symbol& lhs, const Symbol& rhs) const noexcept {
            return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
        }
    };

    static_assert(std::is_trivially_copyable_v<Price>, "Price must be trivially copyable");
    static_assert(std::is_trivially_copyable_v<Symbol>, "Symbol must be trivially copyable");
    static_assert(std::is_trivially_copyable_v<Quantity>, "Quantity must be trivially copyable");
    static_assert(std::is_trivially_copyable_v<OrderId>, "OrderId must be trivially copyable");
    static_assert(std::is_trivially_copyable_v<SequenceNumber>, "SequenceNumber must be trivially copyable");
    static_assert(std::is_trivially_copyable_v<Timestamp>, "Timestamp must be trivially copyable");
    static_assert(std::is_trivially_copyable_v<ParticipantId>, "ParticipantId must be trivially copyable");
    static_assert(std::is_trivially_copyable_v<MatchId>, "MatchId must be trivially copyable");
    static_assert(std::is_trivially_copyable_v<Side>, "Side must be trivially copyable");
    static_assert(std::is_trivially_copyable_v<OrderType>, "OrderType must be trivially copyable");
    static_assert(std::is_trivially_copyable_v<OrderStatus>, "OrderStatus must be trivially copyable");
}
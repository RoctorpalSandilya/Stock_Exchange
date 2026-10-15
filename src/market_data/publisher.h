#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <type_traits>
#include "core/types.h"
#include "matching/events.h"
#include "matching/order_book.h"

namespace exchange::market_data {

/**
 * @brief Level 1 Market Data Packet (Best Bid and Offer + Last Trade).
 */
struct alignas(32) MarketDataL1 {
    core::SequenceNumber sequence_number{0};
    core::Timestamp timestamp{0};
    core::Symbol symbol{};
    core::Price bid_price{0};
    core::Quantity bid_qty{0};
    core::Price ask_price{0};
    core::Quantity ask_qty{0};
    core::Price last_trade_price{0};
    core::Quantity last_trade_qty{0};
};

static_assert(std::is_trivially_copyable_v<MarketDataL1>);

/**
 * @brief Price Level Entry for L2 Depth.
 */
struct DepthLevel {
    core::Price price{0};
    core::Quantity qty{0};
    std::uint32_t order_count{0};
};

static_assert(std::is_trivially_copyable_v<DepthLevel>);

/**
 * @brief Level 2 Market Data Packet (Top 5 Bids and Asks Depth Ladder).
 */
template <std::size_t MaxDepth = 5>
struct alignas(64) MarketDataL2 {
    core::SequenceNumber sequence_number{0};
    core::Timestamp timestamp{0};
    core::Symbol symbol{};
    std::uint8_t bid_levels_count{0};
    std::uint8_t ask_levels_count{0};
    std::array<DepthLevel, MaxDepth> bids{};
    std::array<DepthLevel, MaxDepth> asks{};
};

/**
 * @brief High-Performance Market Data Publisher.
 * 
 * Formats matching engine events and order book snapshots into compact,
 * cache-line aligned binary packets for streaming over UDP multicast or sockets.
 */
class MarketDataPublisher {
public:
    MarketDataPublisher() = default;

    /**
     * @brief Generates an L1 Best Bid / Offer snapshot packet from an order book.
     */
    template <std::size_t OrderCap, std::size_t LevelCap, std::size_t EventCap, std::size_t StopCap>
    [[nodiscard]] static MarketDataL1 build_l1_snapshot(
        const matching::OrderBook<OrderCap, LevelCap, EventCap, StopCap>& book,
        core::SequenceNumber sequence_number,
        core::Timestamp timestamp) noexcept 
    {
        MarketDataL1 l1{};
        l1.sequence_number = sequence_number;
        l1.timestamp = timestamp;
        l1.symbol = book.symbol();

        const auto bbo = book.get_bbo();
        l1.bid_price = bbo.bid_price;
        l1.bid_qty = bbo.bid_qty;
        l1.ask_price = bbo.ask_price;
        l1.ask_qty = bbo.ask_qty;
        l1.last_trade_price = book.last_trade_price();
        return l1;
    }

    /**
     * @brief Generates an L2 5-Level Depth of Market snapshot from an order book.
     */
    template <std::size_t OrderCap, std::size_t LevelCap, std::size_t EventCap, std::size_t StopCap>
    [[nodiscard]] static MarketDataL2<5> build_l2_snapshot(
        const matching::OrderBook<OrderCap, LevelCap, EventCap, StopCap>& book,
        core::SequenceNumber sequence_number,
        core::Timestamp timestamp) noexcept 
    {
        MarketDataL2<5> l2{};
        l2.sequence_number = sequence_number;
        l2.timestamp = timestamp;
        l2.symbol = book.symbol();

        std::array<matching::PriceLevelView, 5> bid_views{};
        std::array<matching::PriceLevelView, 5> ask_views{};

        const auto num_bids = book.get_bid_levels(bid_views);
        const auto num_asks = book.get_ask_levels(ask_views);

        l2.bid_levels_count = static_cast<std::uint8_t>(num_bids);
        l2.ask_levels_count = static_cast<std::uint8_t>(num_asks);

        for (std::size_t i = 0; i < num_bids; ++i) {
            l2.bids[i] = DepthLevel{.price = bid_views[i].price, .qty = bid_views[i].qty, .order_count = 1};
        }

        for (std::size_t i = 0; i < num_asks; ++i) {
            l2.asks[i] = DepthLevel{.price = ask_views[i].price, .qty = ask_views[i].qty, .order_count = 1};
        }

        return l2;
    }
};

} // namespace exchange::market_data

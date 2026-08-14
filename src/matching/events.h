#pragma once
#include <cstdint>
#include <type_traits>
#include "core/types.h"

//We want to avoid touching the heap in the hot path as much as possible

namespace exchange::matching {
    using core::MatchId;
    using core::OrderId;
    using core::OrderType;
    using core::Price;
    using core::Quantity;
    using core::SequenceNumber;
    using core::Side;
    using core::Symbol;
    using core::Timestamp;

    enum class ReasonCode : std::uint16_t{
        NONE = 0, // no reason code
        ZERO_QUANTITY, // order quantity is zero
        NEGATIVE_PRICE, // order price is negative
        BOOK_EMPTY, // order book is empty
        DUPLICATE_ORDER_ID, // order id is duplicate
        ORDER_NOT_FOUND, // order id is not found
        ORDER_NOT_ACTIVE, // order is not active
        FOK_INSUFFICIENT_LIQUIDITY, // fill or kill order cannot be filled due to insufficient liquidity
        POST_ONLY_WOULD_CROSS, // post only order would cross the book
        INVALID_MODIFICATION, // order modification is invalid
        INVALID_ICEBERG_DISPLAY, // iceberg order display quantity is invalid
        ORDER_POOL_EXHAUSTED, // order pool is exhausted
    };

    enum class EventType : std::uint8_t {
        ORDER_ACCEPTED, // order is accepted
        ORDER_RESTED, // order is resting in the book
        ORDER_REJECTED, // order is rejected
        ORDER_REDUCED, // order is reduced
        ORDER_PARTIALLY_FILLED, // order is partially filled
        ORDER_FILLED, // order is filled
        ORDER_CANCELLED, // order is cancelled
        TRADE,
    };
    struct OrderAccepted{
        SequenceNumber sequence_number;
        Timestamp timestamp;
        OrderId order_id;
        Symbol symbol;
        Side side;
        Price price;
        Quantity quantity;
        OrderType order_type;
    };
}
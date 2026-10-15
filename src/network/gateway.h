#pragma once

#include <cstdint>
#include <type_traits>
#include "core/types.h"
#include "matching/events.h"
#include "matching/matching_engine.h"

namespace exchange::network {

/**
 * @brief Wire protocol message types.
 */
enum class MsgType : std::uint8_t {
    NEW_ORDER_REQ = 1,
    CANCEL_ORDER_REQ = 2,
    MODIFY_ORDER_REQ = 3,
    EXEC_REPORT = 4,
    HEARTBEAT = 5
};

/**
 * @brief Fixed-size binary wire request frame sent over network sockets by traders.
 */
#pragma pack(push, 1)
struct ClientOrderRequest {
    MsgType msg_type{MsgType::NEW_ORDER_REQ};
    std::uint8_t action{0}; // 0 = ADD, 1 = CANCEL, 2 = MODIFY
    core::Symbol symbol{};
    core::OrderId order_id{0};
    core::Side side{core::Side::BUY};
    core::Price price{0};
    core::Quantity qty{0};
    core::OrderType order_type{core::OrderType::LIMIT};
    core::ParticipantId participant_id{0};
    core::Price trigger_price{0};
    core::Quantity display_qty{0};
    core::Timestamp client_timestamp{0};
};
#pragma pack(pop)

static_assert(sizeof(ClientOrderRequest) <= 64, "Wire protocol packet must fit into a single cache line");

/**
 * @brief Binary Execution Report payload sent back to clients.
 */
#pragma pack(push, 1)
struct ExecutionReport {
    MsgType msg_type{MsgType::EXEC_REPORT};
    matching::EventType event_type{matching::EventType::ORDER_ACCEPTED};
    core::SequenceNumber sequence_number{0};
    core::Timestamp exchange_timestamp{0};
    core::OrderId order_id{0};
    core::Symbol symbol{};
    core::Side side{core::Side::BUY};
    core::Price price{0};
    core::Quantity qty{0};
    core::Quantity remaining_qty{0};
    matching::ReasonCode reason_code{matching::ReasonCode::NONE};
};
#pragma pack(pop)

/**
 * @brief Network Order Entry Gateway Helper functions.
 * 
 * Provides zero-copy parsing between binary network frames and engine structs.
 */
class GatewayParser {
public:
    /**
     * @brief Translates binary wire client request packet into engine InboundOrder.
     */
    [[nodiscard]] static constexpr matching::InboundOrder parse_request(
        const ClientOrderRequest& req, core::Timestamp receive_timestamp) noexcept 
    {
        matching::InboundOrder order{};
        order.action = static_cast<matching::RequestAction>(req.action);
        order.symbol = req.symbol;
        order.order_id = req.order_id;
        order.side = req.side;
        order.price = req.price;
        order.qty = req.qty;
        order.order_type = req.order_type;
        order.timestamp = receive_timestamp;
        order.participant_id = req.participant_id;
        order.trigger_price = req.trigger_price;
        order.display_qty = req.display_qty;
        return order;
    }

    /**
     * @brief Translates matching engine Event into an outbound ExecutionReport binary frame.
     */
    [[nodiscard]] static constexpr ExecutionReport format_report(const matching::Event& event) noexcept {
        ExecutionReport report{};
        report.msg_type = MsgType::EXEC_REPORT;
        report.event_type = event.type;

        switch (event.type) {
        case matching::EventType::ORDER_ACCEPTED: {
            const auto& p = event.payload.order_accepted;
            report.sequence_number = p.sequence_number;
            report.exchange_timestamp = p.timestamp;
            report.order_id = p.order_id;
            report.symbol = p.symbol;
            report.side = p.side;
            report.price = p.price;
            report.qty = p.qty;
            break;
        }
        case matching::EventType::ORDER_REJECTED: {
            const auto& p = event.payload.order_rejected;
            report.sequence_number = p.sequence_number;
            report.exchange_timestamp = p.timestamp;
            report.order_id = p.order_id;
            report.reason_code = p.reason_code;
            break;
        }
        case matching::EventType::TRADE: {
            const auto& p = event.payload.trade;
            report.sequence_number = p.sequence_number;
            report.exchange_timestamp = p.timestamp;
            report.order_id = p.buy_order_id;
            report.symbol = p.symbol;
            report.price = p.price;
            report.qty = p.qty;
            break;
        }
        default:
            break;
        }
        return report;
    }
};

} // namespace exchange::network

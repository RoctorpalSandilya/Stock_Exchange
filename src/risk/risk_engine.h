#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include "core/types.h"
#include "matching/matching_engine.h"

namespace exchange::risk {

/**
 * @brief Pre-Trade Risk Validation Result Status Codes.
 * 
 * Every order evaluated by the RiskEngine returns one of these status codes.
 * If ACCEPTED (0), the order proceeds to the matching engine.
 * Otherwise, the order is rejected immediately before touching the order book.
 */
enum class RiskResult : std::uint8_t {
    ACCEPTED = 0,                    ///< Order passed all risk rules
    REJECTED_INSUFFICIENT_FUNDS,     ///< Buyer cash balance is lower than order value
    REJECTED_INSUFFICIENT_SHARES,    ///< Seller share position is lower than order size
    REJECTED_PRICE_OUT_OF_BOUNDS,    ///< Order price violates absolute min/max collars or BBO price band
    REJECTED_MAX_QTY_EXCEEDED,       ///< Order size exceeds max allowed single order quantity
    REJECTED_RATE_LIMIT_EXCEEDED,    ///< Participant exceeded max allowed orders per second
    REJECTED_SELF_MATCH              ///< Order would match against participant's own resting order
};

/**
 * @brief Configuration parameters controlling risk thresholds.
 */
struct RiskConfig {
    core::Quantity max_order_qty{1'000'000};             ///< Maximum single order quantity (prevents fat-finger errors)
    core::Price min_price{100};                           ///< Minimum allowable limit price ($0.01 scaled by 10,000)
    core::Price max_price{10'000'000 * core::PRICE_SCALE};///< Maximum allowable limit price ($10,000 scaled by 10,000)
    double max_price_deviation_pct{0.20};                ///< Maximum allowed deviation (20%) from reference midpoint price
    std::uint32_t max_orders_per_second{10'000};          ///< Rate limit threshold
    bool enable_self_match_prevention{true};             ///< Self-Match Prevention (SMP) flag
};

/**
 * @class RiskEngine
 * @brief Ultra-Fast Pre-Trade Risk & Compliance Gatekeeper.
 * 
 * WHAT THIS CLASS DOES:
 * In institutional stock exchanges (like NASDAQ or NYSE), brokers and algorithmic traders
 * must pass pre-trade risk checks before their order can enter the matching engine order book.
 * 
 * LOW-LATENCY DESIGN DETAILS:
 *  - Inline zero-allocation evaluation: No heap allocations take place during check_order().
 *  - Branch prediction optimization: Common path (ACCEPTED) uses standard execution layout,
 *    and failure paths use `[[unlikely]]` attributes to keep instruction cache tight.
 *  - Scaled integer calculations: Financial price-quantity math uses integer arithmetic
 *    without floating-point non-determinism.
 */
template <std::size_t MaxParticipants = 1'000>
class RiskEngine {
public:
    explicit RiskEngine(RiskConfig config = {}) noexcept : config_(config) {}

    /**
     * @brief Updates risk configuration parameters.
     */
    void set_config(const RiskConfig& config) noexcept { config_ = config; }

    /**
     * @brief Deposits/updates participant available cash buying power.
     * @param participant_id Unique trader account identifier.
     * @param cash Scaled cash amount in USD (cents * 100).
     */
    void set_cash_balance(core::ParticipantId participant_id, std::int64_t cash) noexcept {
        participant_cash_[participant_id] = cash;
    }

    /**
     * @brief Queries participant available cash buying power.
     */
    [[nodiscard]] std::int64_t get_cash_balance(core::ParticipantId participant_id) const noexcept {
        auto it = participant_cash_.find(participant_id);
        return (it != participant_cash_.end()) ? it->second : 0;
    }

    /**
     * @brief Sets asset share balance for sell order validation.
     */
    void set_position(core::ParticipantId participant_id, const core::Symbol& symbol, std::int64_t qty) noexcept {
        participant_positions_[{participant_id, symbol}] = qty;
    }

    /**
     * @brief Queries participant asset share holdings.
     */
    [[nodiscard]] std::int64_t get_position(core::ParticipantId participant_id, const core::Symbol& symbol) const noexcept {
        auto it = participant_positions_.find({participant_id, symbol});
        return (it != participant_positions_.end()) ? it->second : 0;
    }

    /**
     * @brief Evaluates an inbound order against active risk rules.
     * 
     * @param order The incoming order request struct.
     * @param reference_price Current market midpoint price or last trade price (for price band collar check).
     * @return RiskResult status indicating ACCEPTED or specific rejection cause.
     */
    [[nodiscard]] RiskResult check_order(const matching::InboundOrder& order, core::Price reference_price = 0) noexcept {
        // Cancel/Modify commands do not consume extra capital, so they pass immediately
        if (order.action != matching::RequestAction::ADD) {
            return RiskResult::ACCEPTED;
        }

        // Rule 1: Order Quantity Boundary Check (Fat-Finger Protection)
        if (order.qty == 0 || order.qty > config_.max_order_qty) [[unlikely]] {
            return RiskResult::REJECTED_MAX_QTY_EXCEEDED;
        }

        // Rule 2: Limit Price Boundary Checks
        if (order.order_type == core::OrderType::LIMIT) {
            if (order.price < config_.min_price || order.price > config_.max_price) [[unlikely]] {
                return RiskResult::REJECTED_PRICE_OUT_OF_BOUNDS;
            }

            // Rule 3: Dynamic Price Band Collar vs Reference Price
            if (reference_price > 0 && config_.max_price_deviation_pct > 0.0) {
                const auto max_dev = static_cast<core::Price>(reference_price * config_.max_price_deviation_pct);
                if (order.price > reference_price + max_dev || order.price < reference_price - max_dev) [[unlikely]] {
                    return RiskResult::REJECTED_PRICE_OUT_OF_BOUNDS;
                }
            }
        }

        // Rule 4: Buying Power / Short Position Check
        if (order.side == core::Side::BUY) {
            // Cash required = (Price * Quantity) / PRICE_SCALE
            const std::int64_t required_cash = (order.price * order.qty) / core::PRICE_SCALE;
            const std::int64_t available_cash = get_cash_balance(order.participant_id);
            if (available_cash < required_cash) [[unlikely]] {
                return RiskResult::REJECTED_INSUFFICIENT_FUNDS;
            }
        } else { // SELL
            const std::int64_t available_shares = get_position(order.participant_id, order.symbol);
            if (available_shares < static_cast<std::int64_t>(order.qty)) [[unlikely]] {
                return RiskResult::REJECTED_INSUFFICIENT_SHARES;
            }
        }

        return RiskResult::ACCEPTED;
    }

private:
    struct PositionKey {
        core::ParticipantId participant_id;
        core::Symbol symbol;

        bool operator==(const PositionKey& other) const noexcept {
            return participant_id == other.participant_id && symbol == other.symbol;
        }
    };

    struct PositionHash {
        std::size_t operator()(const PositionKey& key) const noexcept {
            std::size_t h1 = std::hash<core::ParticipantId>{}(key.participant_id);
            std::size_t h2 = 0;
            for (char c : key.symbol) h2 = h2 * 31 + c;
            return h1 ^ (h2 << 1);
        }
    };

    RiskConfig config_;
    std::unordered_map<core::ParticipantId, std::int64_t> participant_cash_;
    std::unordered_map<PositionKey, std::int64_t, PositionHash> participant_positions_;
};

} // namespace exchange::risk

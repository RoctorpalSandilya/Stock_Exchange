#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include "core/types.h"
#include "matching/events.h"

namespace exchange::clearing {

/**
 * @brief Account Position snapshot for a participant in a specific symbol.
 */
struct PositionRecord {
    core::Symbol symbol{};
    std::int64_t share_quantity{0};   // Positive for LONG, negative for SHORT
    std::int64_t total_cost_basis{0}; // Scaled total acquisition cost
    std::int64_t realized_pnl{0};     // Accumulated realized profit/loss
};

/**
 * @brief Complete Participant Portfolio Account record.
 */
struct AccountRecord {
    core::ParticipantId participant_id{0};
    std::int64_t cash_balance{0};     // Scaled cash balance in USD cents/multiples
    std::int64_t total_trades_settled{0};
};

/**
 * @brief High-Performance Clearing & Settlement Ledger.
 * 
 * Processes matching engine trade output events and updates participant balances,
 * position inventories, cost basis, and realized PnL.
 */
class ClearingLedger {
public:
    ClearingLedger() = default;

    /**
     * @brief Deposits or sets cash balance for a participant.
     */
    void deposit_cash(core::ParticipantId participant_id, std::int64_t cash_amount) noexcept {
        accounts_[participant_id].participant_id = participant_id;
        accounts_[participant_id].cash_balance += cash_amount;
    }

    /**
     * @brief Sets asset share balance for a participant.
     */
    void deposit_shares(core::ParticipantId participant_id, const core::Symbol& symbol, std::int64_t qty) noexcept {
        auto& pos = positions_[{participant_id, symbol}];
        pos.symbol = symbol;
        pos.share_quantity += qty;
    }

    /**
     * @brief Gets current cash balance for a participant.
     */
    [[nodiscard]] std::int64_t get_cash(core::ParticipantId participant_id) const noexcept {
        auto it = accounts_.find(participant_id);
        return (it != accounts_.end()) ? it->second.cash_balance : 0;
    }

    /**
     * @brief Gets current share position for a participant and symbol.
     */
    [[nodiscard]] std::int64_t get_position(core::ParticipantId participant_id, const core::Symbol& symbol) const noexcept {
        auto it = positions_.find({participant_id, symbol});
        return (it != positions_.end()) ? it->second.share_quantity : 0;
    }

    /**
     * @brief Processes a Trade event emitted by the matching engine to settle cash and securities.
     * @param trade Trade execution payload containing buy/sell order IDs, match price, and quantity.
     * @param buy_participant Participant ID of the buyer.
     * @param sell_participant Participant ID of the seller.
     */
    void settle_trade(const matching::Trade& trade, core::ParticipantId buy_participant, core::ParticipantId sell_participant) noexcept {
        const std::int64_t trade_value = (static_cast<std::int64_t>(trade.price) * trade.qty) / core::PRICE_SCALE;

        // 1. Settle Buyer Account: Cash decreases, Stock Shares increase
        accounts_[buy_participant].cash_balance -= trade_value;
        accounts_[buy_participant].total_trades_settled++;
        auto& buy_pos = positions_[{buy_participant, trade.symbol}];
        buy_pos.symbol = trade.symbol;
        buy_pos.share_quantity += trade.qty;
        buy_pos.total_cost_basis += trade_value;

        // 2. Settle Seller Account: Cash increases, Stock Shares decrease
        accounts_[sell_participant].cash_balance += trade_value;
        accounts_[sell_participant].total_trades_settled++;
        auto& sell_pos = positions_[{sell_participant, trade.symbol}];
        sell_pos.symbol = trade.symbol;
        sell_pos.share_quantity -= trade.qty;
        
        // Compute Realized PnL for seller if closing out a long position
        if (sell_pos.share_quantity >= 0 && sell_pos.total_cost_basis > 0) {
            const std::int64_t avg_cost = sell_pos.total_cost_basis / (sell_pos.share_quantity + trade.qty);
            const std::int64_t cost_of_sold_shares = avg_cost * trade.qty;
            sell_pos.realized_pnl += (trade_value - cost_of_sold_shares);
            sell_pos.total_cost_basis -= cost_of_sold_shares;
        }
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

    std::unordered_map<core::ParticipantId, AccountRecord> accounts_;
    std::unordered_map<PositionKey, PositionRecord, PositionHash> positions_;
};

} // namespace exchange::clearing

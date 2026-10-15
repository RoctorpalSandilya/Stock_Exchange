/**
 * @file trader_terminal.cpp
 * @brief Interactive Command-Line Trader Terminal Application.
 * 
 * WHAT THIS FILE DOES:
 * Provides an interactive terminal interface for a human trader to interact directly with the
 * high-frequency matching engine, pre-trade risk engine, and settlement ledger.
 * 
 * FEATURES SUPPORTED:
 *  1. Submitting Advanced Orders (LIMIT, MARKET, IOC, FOK, GTC, STOP, STOP_LIMIT, ICEBERG, POST_ONLY).
 *  2. Order Cancellation.
 *  3. Real-time L2 Market Depth View (Bid/Ask Order Book Ladder).
 *  4. Account Portfolio Statement (Cash Balance, Stock Positions, Realized PnL).
 */

#include <iostream>
#include <iomanip>
#include <string>
#include <array>

#include "core/types.h"
#include "core/clock.h"
#include "matching/matching_engine.h"
#include "risk/risk_engine.h"
#include "clearing/ledger.h"
#include "market_data/publisher.h"

using namespace exchange;

/**
 * @brief Displays the main control menu options.
 */
void display_menu() {
    std::cout << "\n=============================================================\n";
    std::cout << "             TRADER TERMINAL CONTROL PANEL                   \n";
    std::cout << "=============================================================\n";
    std::cout << " 1. Submit Order (LIMIT, MARKET, IOC, FOK, STOP, ICEBERG, etc.)\n";
    std::cout << " 2. Cancel Active Order\n";
    std::cout << " 3. View Order Book Depth (L2 BBO)\n";
    std::cout << " 4. View Account Balances & Portfolio Statement\n";
    std::cout << " 5. Exit\n";
    std::cout << "-------------------------------------------------------------\n";
    std::cout << "Select Option (1-5): ";
}

/**
 * @brief Displays the sub-menu for selecting advanced order types.
 */
void display_order_type_menu() {
    std::cout << "\n --- Select Order Type ---\n";
    std::cout << " 1. LIMIT       (Standard price-time limit order resting in book)\n";
    std::cout << " 2. MARKET      (Executes immediately at best available market price)\n";
    std::cout << " 3. IOC         (Immediate-or-Cancel: fill available, cancel remainder)\n";
    std::cout << " 4. FOK         (Fill-or-Kill: fill 100% immediately or cancel entire order)\n";
    std::cout << " 5. GTC         (Good-till-Cancelled limit order)\n";
    std::cout << " 6. STOP        (Stop Loss market order triggered when market reaches trigger price)\n";
    std::cout << " 7. STOP_LIMIT  (Stop Limit order submitted upon reaching trigger price)\n";
    std::cout << " 8. ICEBERG     (Partially hidden order showing visible peak quantity only)\n";
    std::cout << " 9. POST_ONLY   (Post to order book only; rejects if it would cross existing BBO)\n";
    std::cout << "Select Type (1-9): ";
}

int main() {
    // 1. Instantiate Core Engine Subsystems
    matching::MatchingEngine<100'000, 1'000, 10'000, 1'000> engine;
    risk::RiskEngine<1'000> risk_engine;
    clearing::ClearingLedger ledger;

    // 2. Prepare Trading Symbol ("NVDA")
    const core::Symbol symbol = core::make_symbol("NVDA");

    // 3. Register symbol using stack array.
    //    FIX: Using a fixed stack array instead of temporary std::vector to avoid rvalue span conversion issues.
    const core::Symbol symbols[] = {symbol};
    engine.prepare_symbols(symbols);

    // 4. Set up Trader Account #1 with initial funds
    const core::ParticipantId user_id = 1;
    ledger.deposit_cash(user_id, 500'000 * core::PRICE_SCALE); // $500,000 Initial Cash
    ledger.deposit_shares(user_id, symbol, 2'000);             // 2,000 initial NVDA shares
    risk_engine.set_cash_balance(user_id, 500'000 * core::PRICE_SCALE);
    risk_engine.set_position(user_id, symbol, 2'000);

    core::OrderId order_counter = 100;

    std::cout << "Welcome Trader [Participant ID #" << user_id << "]\n";
    std::cout << "Trading Symbol: " << core::symbol_to_string(symbol) << "\n";
    std::cout << "Initial Cash: $500,000.00 | Initial Stock Position: 2,000 shares\n";

    bool running = true;
    while (running) {
        display_menu();
        int choice = 0;
        if (!(std::cin >> choice)) break;

        switch (choice) {
        case 1: { // --- SUBMIT ADVANCED ORDER ---
            char side_char;
            std::cout << "Enter Side (B = BUY, S = SELL): ";
            std::cin >> side_char;
            core::Side side = (side_char == 'B' || side_char == 'b') ? core::Side::BUY : core::Side::SELL;

            display_order_type_menu();
            int type_choice = 1;
            std::cin >> type_choice;

            core::OrderType order_type = core::OrderType::LIMIT;
            switch (type_choice) {
                case 1: order_type = core::OrderType::LIMIT; break;
                case 2: order_type = core::OrderType::MARKET; break;
                case 3: order_type = core::OrderType::IOC; break;
                case 4: order_type = core::OrderType::FOK; break;
                case 5: order_type = core::OrderType::GTC; break;
                case 6: order_type = core::OrderType::STOP; break;
                case 7: order_type = core::OrderType::STOP_LIMIT; break;
                case 8: order_type = core::OrderType::ICEBERG; break;
                case 9: order_type = core::OrderType::POST_ONLY; break;
                default: std::cout << "Invalid order type selected. Defaulting to LIMIT.\n"; break;
            }

            core::Price price = 0;
            if (order_type != core::OrderType::MARKET && order_type != core::OrderType::STOP) {
                double price_val;
                std::cout << "Enter Price (e.g. 145.50): ";
                std::cin >> price_val;
                price = static_cast<core::Price>(price_val * core::PRICE_SCALE);
            }

            core::Quantity qty = 0;
            std::cout << "Enter Total Order Quantity: ";
            std::cin >> qty;

            core::Price trigger_price = 0;
            if (order_type == core::OrderType::STOP || order_type == core::OrderType::STOP_LIMIT) {
                double trigger_val;
                std::cout << "Enter Trigger Price (e.g. 140.00): ";
                std::cin >> trigger_val;
                trigger_price = static_cast<core::Price>(trigger_val * core::PRICE_SCALE);
            }

            core::Quantity display_qty = 0;
            if (order_type == core::OrderType::ICEBERG) {
                std::cout << "Enter Visible Display Peak Quantity (e.g. 100 out of " << qty << "): ";
                std::cin >> display_qty;
            }

            // Construct Inbound Order payload
            matching::InboundOrder req{
                .action = matching::RequestAction::ADD,
                .symbol = symbol,
                .order_id = ++order_counter,
                .side = side,
                .price = price,
                .qty = qty,
                .order_type = order_type,
                .timestamp = core::rdtsc(),
                .participant_id = user_id,
                .trigger_price = trigger_price,
                .display_qty = display_qty
            };

            // Step A: Pre-Trade Risk Check
            auto risk_status = risk_engine.check_order(req);
            if (risk_status != risk::RiskResult::ACCEPTED) {
                std::cout << "[RISK REJECTED] Order failed pre-trade risk checks!\n";
                break;
            }

            // Step B: Submit order to Matching Engine
            auto events = engine.process_order(req);
            std::cout << "\n[SUCCESS] Submitted Order #" << req.order_id 
                      << " | Events generated: " << events.size() << "\n";

            // Step C: Interpret output execution events
            for (const auto& ev : events) {
                switch (ev.type) {
                case matching::EventType::ORDER_ACCEPTED:
                    std::cout << "  -> EVENT: ORDER ACCEPTED (#" << ev.payload.order_accepted.order_id << ")\n";
                    break;
                case matching::EventType::ORDER_RESTED:
                    std::cout << "  -> EVENT: ORDER RESTED IN BOOK (#" << ev.payload.order_rested.order_id 
                              << " Qty: " << ev.payload.order_rested.qty << ")\n";
                    break;
                case matching::EventType::ORDER_REJECTED:
                    std::cout << "  -> EVENT: ORDER REJECTED (#" << ev.payload.order_rejected.order_id 
                              << " Reason Code: " << static_cast<int>(ev.payload.order_rejected.reason_code) << ")\n";
                    break;
                case matching::EventType::TRADE: {
                    const auto& t = ev.payload.trade;
                    ledger.settle_trade(t, t.buy_order_id, t.sell_order_id);
                    std::cout << "  -> EVENT: TRADE EXECUTED! Match #" << t.match_id 
                              << " | Qty: " << t.qty << " @ $" 
                              << (static_cast<double>(t.price) / core::PRICE_SCALE) << "\n";
                    break;
                }
                case matching::EventType::ORDER_FILLED:
                    std::cout << "  -> EVENT: ORDER FILLED (#" << ev.payload.order_filled.order_id << ")\n";
                    break;
                case matching::EventType::ORDER_PARTIALLY_FILLED:
                    std::cout << "  -> EVENT: ORDER PARTIALLY FILLED (#" << ev.payload.order_partially_filled.order_id 
                              << " Filled: " << ev.payload.order_partially_filled.filled_qty 
                              << ", Remaining: " << ev.payload.order_partially_filled.remaining_qty << ")\n";
                    break;
                case matching::EventType::ORDER_CANCELED:
                    std::cout << "  -> EVENT: ORDER CANCELED (#" << ev.payload.order_canceled.order_id << ")\n";
                    break;
                default:
                    break;
                }
            }
            break;
        }
        case 2: { // --- CANCEL ACTIVE ORDER ---
            core::OrderId cancel_id;
            std::cout << "Enter Order ID to cancel: ";
            std::cin >> cancel_id;

            matching::InboundOrder req{
                .action = matching::RequestAction::CANCEL,
                .symbol = symbol,
                .order_id = cancel_id,
                .timestamp = core::rdtsc()
            };

            auto events = engine.process_order(req);
            std::cout << "[CANCEL] Processed cancellation for Order #" << cancel_id 
                      << " (Events: " << events.size() << ")\n";
            break;
        }
        case 3: { // --- VIEW LEVEL 2 MARKET DEPTH LADDER ---
            const auto* book = engine.find_book(symbol);
            if (!book) {
                std::cout << "[BOOK] No active book for " << core::symbol_to_string(symbol) << "\n";
                break;
            }

            auto l2 = market_data::MarketDataPublisher::build_l2_snapshot(*book, 1, core::rdtsc());
            std::cout << "\n--- L2 MARKET DEPTH (" << core::symbol_to_string(symbol) << ") ---\n";
            std::cout << " ASKS (SELLERS):\n";
            for (int i = static_cast<int>(l2.ask_levels_count) - 1; i >= 0; --i) {
                std::cout << "   $" << std::fixed << std::setprecision(2) 
                          << (static_cast<double>(l2.asks[i].price) / core::PRICE_SCALE)
                          << " | Qty: " << l2.asks[i].qty << "\n";
            }
            std::cout << " -------------------------------\n";
            std::cout << " BIDS (BUYERS):\n";
            for (std::size_t i = 0; i < l2.bid_levels_count; ++i) {
                std::cout << "   $" << std::fixed << std::setprecision(2)
                          << (static_cast<double>(l2.bids[i].price) / core::PRICE_SCALE)
                          << " | Qty: " << l2.bids[i].qty << "\n";
            }
            break;
        }
        case 4: { // --- VIEW PORTFOLIO STATEMENT ---
            std::cout << "\n--- PORTFOLIO STATEMENT ---\n";
            std::cout << " Participant ID: " << user_id << "\n";
            std::cout << " Cash Balance: $" << std::fixed << std::setprecision(2)
                      << (static_cast<double>(ledger.get_cash(user_id)) / core::PRICE_SCALE) << "\n";
            std::cout << " Share Balance (" << core::symbol_to_string(symbol) << "): "
                      << ledger.get_position(user_id, symbol) << " shares\n";
            break;
        }
        case 5:
            running = false;
            break;
        default:
            std::cout << "Invalid choice.\n";
            break;
        }
    }

    std::cout << "Exiting Trader Terminal. Goodbye!\n";
    return 0;
}
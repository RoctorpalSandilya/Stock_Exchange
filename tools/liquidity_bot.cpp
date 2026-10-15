/**
 * @file liquidity_bot.cpp
 * @brief Automated Market Maker / Liquidity Provider Bot Simulation Tool.
 * 
 * WHAT THIS FILE DOES:
 * In a real financial exchange, liquidity is provided by Market Makers (MMs) who constantly
 * place two-sided quotes (bids to buy and asks to sell) around a central fair price (midpoint).
 * This tool automates this behavior to populate the matching engine with realistic orders.
 * 
 * LOW-LATENCY CONCEPTS DEMONSTRATED:
 *  - Stack-allocated symbol arrays (zero dynamic vector allocations on startup).
 *  - Direct microsecond latency timestamps using hardware TSC (rdtsc()).
 *  - Direct Level 1 (BBO) snapshot reading for real-time spread analysis.
 */

#include <iostream>
#include <array>
#include <thread>
#include <chrono>

#include "core/types.h"
#include "core/clock.h"
#include "matching/matching_engine.h"
#include "market_data/publisher.h"

using namespace exchange;

int main() {
    std::cout << "=================================================\n";
    std::cout << "       STOCK EXCHANGE - LIQUIDITY BOT STARTING     \n";
    std::cout << "=================================================\n\n";

    // 1. Instantiating the Matching Engine core for 100,000 max orders, 1,000 price levels,
    //    10,000 events, and 1,000 stop orders.
    matching::MatchingEngine<100'000, 1'000, 10'000, 1'000> engine;

    // 2. Creating the 8-character fixed-length symbol identifier for Apple ("AAPL").
    const core::Symbol aapl = core::make_symbol("AAPL");

    // 3. Registering trading symbols into the engine using a stack array.
    //    FIX: We use a fixed stack array instead of a temporary std::vector to avoid rvalue
    //    span lifetime issues and dynamic memory heap allocation.
    const core::Symbol symbols[] = {aapl};
    engine.prepare_symbols(symbols);

    // 4. Initializing market maker parameters.
    //    Prices are scaled by 10,000 (PRICE_SCALE) to store exact cents without floating-point inaccuracy.
    core::Price mid_price = 150 * core::PRICE_SCALE; // Midpoint starting price = $150.00
    core::OrderId current_order_id = 1000;
    const core::ParticipantId bot_id = 99; // Unique participant ID assigned to this Bot

    std::cout << "[BOT] Initialized Market Maker Bot for symbol: " 
              << core::symbol_to_string(aapl) << " at Midpoint Price: $" 
              << (mid_price / core::PRICE_SCALE) << ".00\n\n";

    // 5. Run 5 consecutive quoting cycles to simulate dynamic market activity.
    for (int cycle = 1; cycle <= 5; ++cycle) {
        std::cout << "--- Quoting Cycle #" << cycle << " ---\n";
        
        // --- STEP A: Post Buy (BID) Quotes below the midpoint ---
        // Places 3 limit buy orders at increasing distances below the current mid price ($0.10, $0.20, $0.30 below).
        for (int i = 1; i <= 3; ++i) {
            core::Price bid_price = mid_price - (i * 10 * 100); // Subtracting 10 cents * PRICE_SCALE factor
            matching::InboundOrder bid_order{
                .action = matching::RequestAction::ADD,
                .symbol = aapl,
                .order_id = ++current_order_id,
                .side = core::Side::BUY,
                .price = bid_price,
                .qty = 100 * static_cast<core::Quantity>(i),
                .order_type = core::OrderType::LIMIT,
                .timestamp = core::rdtsc(), // Hardware timestamp counter
                .participant_id = bot_id
            };

            // Process order directly in matching engine
            auto events = engine.process_order(bid_order);
            std::cout << " [BOT] Posted BID  Order #" << bid_order.order_id 
                      << " Qty: " << bid_order.qty << " @ $" 
                      << (static_cast<double>(bid_price) / core::PRICE_SCALE) 
                      << " (Events generated: " << events.size() << ")\n";
        }

        // --- STEP B: Post Sell (ASK) Quotes above the midpoint ---
        // Places 3 limit sell orders at increasing distances above the current mid price ($0.10, $0.20, $0.30 above).
        for (int i = 1; i <= 3; ++i) {
            core::Price ask_price = mid_price + (i * 10 * 100); // Adding 10 cents * PRICE_SCALE factor
            matching::InboundOrder ask_order{
                .action = matching::RequestAction::ADD,
                .symbol = aapl,
                .order_id = ++current_order_id,
                .side = core::Side::SELL,
                .price = ask_price,
                .qty = 100 * static_cast<core::Quantity>(i),
                .order_type = core::OrderType::LIMIT,
                .timestamp = core::rdtsc(),
                .participant_id = bot_id
            };

            auto events = engine.process_order(ask_order);
            std::cout << " [BOT] Posted ASK  Order #" << ask_order.order_id 
                      << " Qty: " << ask_order.qty << " @ $" 
                      << (static_cast<double>(ask_price) / core::PRICE_SCALE) 
                      << " (Events generated: " << events.size() << ")\n";
        }

        // --- STEP C: Read Level 1 (BBO) Market Data Feed ---
        // Extracts the top-of-book (Best Bid and Best Ask) to verify spread width.
        const auto* book = engine.find_book(aapl);
        if (book) {
            const auto l1 = market_data::MarketDataPublisher::build_l1_snapshot(*book, cycle, core::rdtsc());
            std::cout << " [MARKET DATA] BBO -> Best Bid: $" 
                      << (static_cast<double>(l1.bid_price) / core::PRICE_SCALE) 
                      << " (Qty: " << l1.bid_qty << ") | Best Ask: $" 
                      << (static_cast<double>(l1.ask_price) / core::PRICE_SCALE) 
                      << " (Qty: " << l1.ask_qty << ")\n";
        }

        // Adjust mid price slightly to simulate price discovery movement
        mid_price += (cycle % 2 == 0 ? 5000 : -2500); 
        std::cout << "\n";
    }

    std::cout << "[BOT] Liquidity Bot execution completed successfully.\n";
    return 0;
}
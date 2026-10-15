/**
 * @file exchange_main.cpp
 * @brief High-Frequency Matching Engine Server Orchestrator & Benchmark Entrypoint.
 * 
 * WHAT THIS FILE DOES:
 * Serves as the central server application that wires together all exchange subsystems:
 *  1. Sequencer (`Sequencer`): Generates monotonically increasing event sequence numbers.
 *  2. Matching Engine (`MatchingEngine`): High-speed limit order book matcher.
 *  3. Pre-Trade Risk Engine (`RiskEngine`): Sub-microsecond risk checks (buying power, price bounds).
 *  4. Clearing & Settlement Ledger (`ClearingLedger`): Debits/credits cash & share balances post-trade.
 *  5. Write-Ahead Log (`WalLogger`): Binary persistence engine for crash recovery playback.
 *  6. Ring Queue (`SPSCQueue`): Lock-free thread communication between network gateway and engine thread.
 */

#include <iostream>
#include <iomanip>
#include <array>
#include <thread>
#include <chrono>

#include "core/types.h"
#include "core/clock.h"
#include "core/spsc_queue.h"
#include "core/sequencer.h"
#include "matching/matching_engine.h"
#include "risk/risk_engine.h"
#include "clearing/ledger.h"
#include "market_data/publisher.h"
#include "network/gateway.h"
#include "storage/wal_logger.h"

using namespace exchange;

int main() {
    std::cout << "=========================================================\n";
    std::cout << "      HIGH-FREQUENCY ULTRA-LOW LATENCY STOCK EXCHANGE     \n";
    std::cout << "=========================================================\n\n";

    // 1. Initialize Core Exchange Subsystem Services
    core::Sequencer global_sequencer{1};
    matching::MatchingEngine<100'000, 1'000, 10'000, 1'000> matching_engine;
    risk::RiskEngine<1'000> risk_engine;
    clearing::ClearingLedger ledger;
    storage::WalLogger wal_logger("exchange_wal.dat");

    if (wal_logger.open()) {
        std::cout << "[SYSTEM] Write-Ahead Logger (WAL) initialized successfully.\n";
    }

    // 2. Register Trading Symbols into Matching Engine using stack-allocated array.
    //    FIX: Stack array prevents temporary std::vector rvalue span conversion issues.
    const core::Symbol aapl = core::make_symbol("AAPL");
    const core::Symbol nvda = core::make_symbol("NVDA");
    const core::Symbol symbols[] = {aapl, nvda};
    matching_engine.prepare_symbols(symbols);
    std::cout << "[SYSTEM] Prepared Order Books for AAPL & NVDA.\n";

    // 3. Fund Participant Accounts for Testing
    const core::ParticipantId trader_a = 101; // Buyer Account
    const core::ParticipantId trader_b = 102; // Seller Account

    ledger.deposit_cash(trader_a, 1'000'000 * core::PRICE_SCALE); // $1,000,000 cash balance
    ledger.deposit_shares(trader_b, aapl, 10'000);               // 10,000 shares of AAPL
    
    risk_engine.set_cash_balance(trader_a, 1'000'000 * core::PRICE_SCALE);
    risk_engine.set_position(trader_b, aapl, 10'000);

    std::cout << "[SYSTEM] Funded Trader A ($1M cash) and Trader B (10k AAPL shares).\n\n";

    // 4. Lock-free SPSC Ring Queue for Network Gateway -> Engine Pipeline
    core::SPSCQueue<network::ClientOrderRequest, 1024> order_queue;

    // 5. Simulate Inbound High-Frequency Trading Binary Network Packets
    std::cout << "[PIPELINE] Pushing Simulated Binary Network Orders into SPSC Ring Queue...\n";

    network::ClientOrderRequest req1{
        .msg_type = network::MsgType::NEW_ORDER_REQ,
        .action = 0, // ADD
        .symbol = aapl,
        .order_id = 1,
        .side = core::Side::SELL,
        .price = 150 * core::PRICE_SCALE, // Sell 100 shares @ $150.00
        .qty = 100,
        .order_type = core::OrderType::LIMIT,
        .participant_id = trader_b
    };

    network::ClientOrderRequest req2{
        .msg_type = network::MsgType::NEW_ORDER_REQ,
        .action = 0, // ADD
        .symbol = aapl,
        .order_id = 2,
        .side = core::Side::BUY,
        .price = 150 * core::PRICE_SCALE, // Buy 100 shares @ $150.00 (Crosses with Order #1)
        .qty = 100,
        .order_type = core::OrderType::LIMIT,
        .participant_id = trader_a
    };

    order_queue.push(req1);
    order_queue.push(req2);

    // 6. Process Queue Items inside Main Engine Processing Loop
    std::cout << "\n[ENGINE] Processing Orders from SPSC Queue...\n";
    network::ClientOrderRequest incoming_req;

    while (order_queue.pop(incoming_req)) {
        const auto t_start = core::rdtsc(); // Capture high-resolution start tick
        
        // Step A: Parse binary wire packet into engine format
        const auto order = network::GatewayParser::parse_request(incoming_req, t_start);

        // Step B: Pre-Trade Risk Check
        const auto risk_res = risk_engine.check_order(order);
        if (risk_res != risk::RiskResult::ACCEPTED) {
            std::cout << " [RISK REJECT] Order ID #" << order.order_id << " failed risk checks!\n";
            continue;
        }

        // Step C: Persist order command to Write-Ahead Log
        wal_logger.append(global_sequencer.next(), t_start, order);

        // Step D: Execute in Matching Engine Core
        const auto events = matching_engine.process_order(order);
        const auto t_end = core::rdtsc(); // Capture end tick
        const double latency_ns = static_cast<double>(t_end - t_start);

        std::cout << " [EXECUTION] Processed Order #" << order.order_id 
                  << " | Side: " << (order.side == core::Side::BUY ? "BUY " : "SELL")
                  << " | Qty: " << order.qty << " @ $" << (order.price / core::PRICE_SCALE)
                  << " | Latency: " << latency_ns << " cycles (~" << (latency_ns / 3.0) << " ns)"
                  << " | Events Generated: " << events.size() << "\n";

        // Step E: Route Trade Events to Clearing & Settlement Ledger
        for (const auto& ev : events) {
            if (ev.type == matching::EventType::TRADE) {
                const auto& trade = ev.payload.trade;
                ledger.settle_trade(trade, trader_a, trader_b);
                std::cout << "   [SETTLEMENT] Trade #" << trade.match_id << " Settled! Price: $"
                          << (static_cast<double>(trade.price) / core::PRICE_SCALE)
                          << " | Qty: " << trade.qty << "\n";
            }
        }
    }

    // 7. Verify Account Balances after Settlement
    std::cout << "\n=========================================================\n";
    std::cout << "                 POST-TRADE ACCOUNT SUMMARY              \n";
    std::cout << "=========================================================\n";
    std::cout << " Trader A (Buyer) Cash: $" << std::fixed << std::setprecision(2)
              << (static_cast<double>(ledger.get_cash(trader_a)) / core::PRICE_SCALE)
              << " | AAPL Shares: " << ledger.get_position(trader_a, aapl) << "\n";

    std::cout << " Trader B (Seller) Cash: $" << std::fixed << std::setprecision(2)
              << (static_cast<double>(ledger.get_cash(trader_b)) / core::PRICE_SCALE)
              << " | AAPL Shares: " << ledger.get_position(trader_b, aapl) << "\n";

    std::cout << "\n[SYSTEM] Exchange Engine run completed cleanly.\n";
    return 0;
}
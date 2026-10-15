#pragma once

#include <fstream>
#include <string>
#include <vector>
#include "core/types.h"
#include "matching/matching_engine.h"
#include "matching/events.h"

namespace exchange::storage {

/**
 * @brief Header for WAL Log File.
 */
struct alignas(16) WalHeader {
    char magic[4] = {'S', 'E', 'W', 'L'}; // Stock Exchange WAL Log
    std::uint32_t version{1};
    std::uint64_t created_timestamp{0};
};

/**
 * @brief Fixed-size WAL Log Record Entry.
 */
struct alignas(64) WalRecord {
    core::SequenceNumber sequence_number{0};
    core::Timestamp timestamp{0};
    matching::InboundOrder order{};
};

static_assert(std::is_trivially_copyable_v<WalRecord>);

/**
 * @brief High-Performance Write-Ahead Logger (WAL).
 * 
 * Appends inbound order commands synchronously or asynchronously to disk to guarantee
 * durability. Provides crash recovery playback capabilities to restore matching engine state.
 */
class WalLogger {
public:
    explicit WalLogger(std::string log_filename = "exchange_wal.dat")
        : filename_(std::move(log_filename)) {}

    ~WalLogger() {
        close();
    }

    /**
     * @brief Opens WAL log file for writing.
     */
    bool open() noexcept {
        file_.open(filename_, std::ios::binary | std::ios::out | std::ios::app);
        if (!file_.is_open()) return false;

        // Write header if file is empty
        file_.seekp(0, std::ios::end);
        if (file_.tellp() == 0) {
            WalHeader header{};
            file_.write(reinterpret_cast<const char*>(&header), sizeof(header));
            file_.flush();
        }
        return true;
    }

    /**
     * @brief Appends an inbound order command record to the disk journal.
     */
    inline void append(core::SequenceNumber seq, core::Timestamp timestamp, const matching::InboundOrder& order) noexcept {
        if (!file_.is_open()) return;
        WalRecord rec{
            .sequence_number = seq,
            .timestamp = timestamp,
            .order = order
        };
        file_.write(reinterpret_cast<const char*>(&rec), sizeof(rec));
        file_.flush(); // Ensures immediate persistence
    }

    /**
     * @brief Closes log file.
     */
    void close() noexcept {
        if (file_.is_open()) {
            file_.flush();
            file_.close();
        }
    }

    /**
     * @brief Replays all recorded log entries from file for crash recovery.
     * @return Vector of restored InboundOrder commands.
     */
    [[nodiscard]] static std::vector<matching::InboundOrder> replay(const std::string& filename) {
        std::vector<matching::InboundOrder> orders;
        std::ifstream in(filename, std::ios::binary | std::ios::in);
        if (!in.is_open()) return orders;

        WalHeader header;
        in.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (in.gcount() < static_cast<std::streamsize>(sizeof(header))) return orders;

        WalRecord rec;
        while (in.read(reinterpret_cast<char*>(&rec), sizeof(rec))) {
            orders.push_back(rec.order);
        }
        return orders;
    }

private:
    std::string filename_;
    std::ofstream file_;
};

} // namespace exchange::storage

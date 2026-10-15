#pragma once

#include <cstdint>
#include <atomic>
#include <type_traits>
#include "core/types.h"

namespace exchange::core {

/**
 * @brief High-performance Monotonic Sequencer for Order and Event Sequencing.
 * 
 * In ultra-low-latency financial exchanges, every event (order acceptance, fill,
 * trade, cancellation, rejection) must be assigned a strictly increasing, contiguous
 * sequence number. This sequence number enables:
 *  1. Deterministic market data feed re-ordering and gap detection for clients.
 *  2. Reliable Write-Ahead Logging (WAL) and crash recovery state playback.
 *  3. Audit trail compliance.
 * 
 * Low-Latency Design Notes:
 *  - Cache-line aligned (`alignas(64)`) to prevent false sharing when accessed by single thread.
 *  - Standard layout with zero dynamic allocation.
 *  - Provides non-atomic single-threaded mode (fastest, no bus locking instructions)
 *    and atomic mode for multi-threaded sequencing.
 */
class alignas(64) Sequencer {
public:
    constexpr explicit Sequencer(SequenceNumber initial_sequence = 1) noexcept
        : sequence_(initial_sequence) {}

    /**
     * @brief Generates the next sequence number (non-atomic, single-threaded mode).
     * @return Next contiguous SequenceNumber.
     */
    [[nodiscard]] inline SequenceNumber next_sequence() noexcept {
        return sequence_++;
    }

    /**
     * @brief Generates the next sequence number (alias for next_sequence).
     */
    [[nodiscard]] inline SequenceNumber next() noexcept {
        return next_sequence();
    }

    /**
     * @brief Atomically generates the next sequence number for multi-producer thread scenarios.
     * @return Next contiguous SequenceNumber.
     */
    [[nodiscard]] inline SequenceNumber next_atomic() noexcept {
        return std::atomic_ref<SequenceNumber>(sequence_).fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Reads the current sequence number without incrementing.
     */
    [[nodiscard]] inline SequenceNumber current() const noexcept {
        return sequence_;
    }

    /**
     * @brief Resets the sequence counter to a specific starting sequence number.
     */
    inline void reset(SequenceNumber new_sequence = 1) noexcept {
        sequence_ = new_sequence;
    }

private:
    SequenceNumber sequence_{1};
};

static_assert(std::is_standard_layout_v<Sequencer>, "Sequencer must be standard layout");

} // namespace exchange::core

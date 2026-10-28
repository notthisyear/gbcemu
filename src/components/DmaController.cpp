#include "DmaController.h"

namespace gbcemu {

void DmaController::request_transfer(std::uint8_t const source_high_byte) {
    m_pending_source_high_byte = source_high_byte; // latest write wins
    m_pending_delay = kStartupDelay;
}

std::optional<DmaByteTransfer> DmaController::process() {
    if (m_is_transferring && m_current_byte_offset == kDmaTransferNumberOfBytes)
        m_is_transferring = false;

    if (m_pending_delay > 0U && --m_pending_delay == 0U) {
        // A pending transfer replaces any active one
        m_current_dma_source_high_byte = m_pending_source_high_byte;
        m_current_byte_offset = 0U;
        m_is_transferring = true;
    }

    if (!m_is_transferring)
        return std::nullopt;

    std::uint8_t const offset{ m_current_byte_offset++ };
    return DmaByteTransfer{ static_cast<std::uint16_t>((m_current_dma_source_high_byte << 8) | offset), static_cast<std::uint16_t>(0xFE00 + offset) };
}

bool DmaController::is_transferring() const { return m_is_transferring; }

}

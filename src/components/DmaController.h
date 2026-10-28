#pragma once

#include <cstdint>
#include <optional>

namespace gbcemu {

struct DmaByteTransfer final {
    std::uint16_t source;
    std::uint16_t destination;
};

class DmaController final {
  public:
    // Called by the MMU on a write to the DMA register
    void request_transfer(std::uint8_t const source_high_byte);

    // Called each M-cycle
    std::optional<DmaByteTransfer> process();

    // Used by the MMU to block OAM accesses
    bool is_transferring() const;

  private:
    static constexpr std::uint8_t kDmaTransferNumberOfBytes{ 160U };
    static constexpr std::uint8_t kStartupDelay{ 2U };

    bool m_is_transferring{ false };
    std::uint8_t m_pending_delay{ 0U };
    std::uint8_t m_pending_source_high_byte{ 0U };
    std::uint8_t m_current_byte_offset{ 0U };
    std::uint8_t m_current_dma_source_high_byte{ 0U };
};
}
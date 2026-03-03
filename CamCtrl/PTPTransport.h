#pragma once

#include <cstdint>
#include <vector>
#include <memory>


// PTP vendor escape constants used by transports (standard types)
constexpr std::uint32_t ESCAPE_PTP_VENDOR_COMMAND = 0x0100;
// Keep these values aligned with example-v3-windows/CameraControlPTP/PTPDef.h
constexpr std::uint32_t PTP_NEXTPHASE_READ_DATA = 3;
constexpr std::uint32_t PTP_NEXTPHASE_WRITE_DATA = 4;
constexpr std::uint32_t PTP_NEXTPHASE_NO_DATA = 5;
constexpr size_t PTP_MAX_PARAMS = 5;


/**
 * @file PTPTransport.h
 * @brief Transport abstraction for PTP vendor escape operations.
 *
 * This header declares a small, transport-agnostic interface used by the
 * wrapper implementation to send PTP vendor escape commands. Implementations
 * may wrap platform-specific mechanisms (WIA, libusb, vendor SDKs, etc.).
 */

/**
 * @brief Lightweight escape result used by transport implementations.
 */
struct PTP_EscapeResult
{
    // Transport implementations should use native error codes (e.g. HRESULT)
    // but we expose a simple 32-bit integer here to remain platform-agnostic.
    std::int32_t hr = 0;
    std::uint16_t responseCode = 0;
    std::vector<std::uint8_t> payload;
};

/**
 * @brief Abstract transport interface for sending PTP vendor escape commands.
 *
 * Implementations perform the actual data exchange with the device and return
 * a @c PTP_EscapeResult describing the outcome. Calls are expected to be
 * blocking and reentrant if the underlying transport supports it.
 */
class IPTPTransport
{
public:
    virtual ~IPTPTransport() = default;

    /**
     * @brief Perform a vendor escape (blocking).
     *
     * @param opcode PTP vendor opcode to send (WORD).
     * @param params Optional numeric parameters (device-specific meanings).
     * @param writeData Optional binary payload to write.
     * @param writeSize Size of @p writeData in bytes.
     * @return A populated @c PTP_EscapeResult describing HRESULT/response/payload.
     */
    virtual PTP_EscapeResult Escape(std::uint16_t opcode,
                                    const std::vector<std::uint32_t> &params,
                                    const std::uint8_t *writeData,
                                    size_t writeSize) = 0;
};

using IPTPTransportPtr = std::shared_ptr<IPTPTransport>;

inline bool PTP_HR_SUCCEEDED(std::int32_t hr) { return hr >= 0; }
inline bool PTP_HR_FAILED(std::int32_t hr) { return hr < 0; }

#pragma once

#include <cstdint>
#include "PTPTransport.h"

/**
 * @brief Minimal blocking-style camera control interface for non-wide-char usage.
 *
 * Implementations provide a synchronous (blocking) API for basic camera control
 * operations and a small, numeric status snapshot. Designed to be used from
 * single-threaded callers or protected externally by the caller.
 */
class ISimpleCamCtrl
{
public:
	/**
	 * @brief Raw exposure parameters (values are raw PTP codes).
	 */
	struct ExposureParams
	{
		std::uint32_t shutter_speed = 0; /**< Raw PTP value for shutter speed */
		std::uint16_t f_number = 0;		 /**< Raw PTP value for aperture (f-number) */
		std::uint32_t iso = 0;			 /**< Raw PTP value for ISO */
		std::int32_t exposure_comp = 0;	 /**< Raw PTP value for exposure compensation */
	};

	virtual ~ISimpleCamCtrl() = default;

	/** @name Lifecycle (blocking) */
	/** @{ */
	/**
	 * @brief Establish connection / prepare transport and session.
	 * @return true on success, false on failure.
	 */
	virtual bool Connect() = 0;

	/**
	 * @brief Connect using an externally-provided transport implementation.
	 *
	 * Ownership of the @c IPTPTransportPtr is shared; the transport instance
	 * must remain valid for the lifetime of the connection.
	 * @param transport Shared pointer to an @c IPTPTransport implementation.
	 * @return true on success.
	 */
	bool SetPtpTransport(IPTPTransportPtr transport);

	/**
	 * @brief Tear down connection and free resources. Blocking.
	 */
	virtual void Disconnect() = 0;

	/**
	 * @brief Returns connection state.
	 * @return true if connected and ready to use.
	 */
	virtual bool IsConnected() const = 0;
	/** @} */

	/**
	 * @brief Update (fetch) cached device state. Blocking.
	 *
	 * Implementations should refresh their internal snapshot so subsequent
	 * getters return up-to-date values.
	 * @return true on success.
	 */
	virtual bool UpdateStatus() = 0;

	/**
	 * @brief Get last-updated exposure parameters.
	 * @param out_params Output parameter that will be filled with the last known values.
	 * @return true if valid parameters are available (e.g., after successful UpdateStatus()).
	 */
	virtual bool GetExposureParams(ExposureParams &out_params) const = 0;

	/**
	 * @brief Get last-updated exposure mode (raw PTP value).
	 * @return Raw exposure mode code.
	 */
	virtual std::uint32_t GetExposureMode() const = 0;

	/**
	 * @brief Focus (half-press) control.
	 * @return true on success.
	 */
	virtual bool FocusStart() = 0;
	virtual bool FocusEnd() = 0;

	/**
	 * @brief Shutter (full-press) control.
	 * @return true on success.
	 */
	virtual bool ShutterStart() = 0;
	virtual bool ShutterEnd() = 0;

	/**
	 * @brief Movie recording control.
	 * @return true on success.
	 */
	virtual bool MovieRecStart() = 0;
	virtual bool MovieRecEnd() = 0;

	/**
	 * @brief Change exposure parameters with raw PTP values.
	 * @param params Desired exposure parameters; implementations may apply a subset
	 * of fields depending on device support. Any fields outside the exposure
	 * parameters in the struct are ignored.
	 * @return true on success.
	 */
	virtual bool SetExposureParams(const ExposureParams &params) = 0;

	/** @name Movie Recording (blocking) */
	/** @{ */
    /**
     * @brief Movie recording state accessor.
     * @return true if the device is currently recording video.
     */
    virtual bool IsMovieRecording() const = 0;

    /** @} */
};

// Lite header.
// Defines a minimal blocking-style camera control interface for non-wide-char usage.
#pragma once

#include <cstdint>
#include "PTPTransport.h"

#ifndef SONY_FOCUS_POSITION_TYPE_NONE
#define SONY_FOCUS_POSITION_TYPE_NONE 0x00000000U
#define SONY_FOCUS_POSITION_TYPE_ABSOLUTE 0x00000001U
#define SONY_FOCUS_POSITION_TYPE_FOCAL_DISTANCE_METER 0x00000002U
#define SONY_FOCUS_POSITION_TYPE_FOLLOW_FOCUS 0x00000004U
#define SONY_FOCUS_POSITION_TYPE_AF_AREA_POINT 0x00000008U
#define SONY_FOCUS_POSITION_TYPE_SONY_MASK 0x0000000FU
#endif

/**
 * @brief Minimal blocking-style camera control interface for non-wide-char usage.
 *
 * Implementations provide a synchronous (blocking) API for basic camera control
 * operations and a small, numeric status snapshot. Designed to be used from
 * single-threaded callers or protected externally by the caller.
 * 
 */
class ISimpleCamCtrl
{
public:
	/**
	 * @brief Exposure parameters in physical values.
	 */
	struct ExposureParams
	{
		double shutter_speed = 0.0; /**< Reciprocal shutter speed in 1/sec (e.g. 60 means 1/60 sec) */
		double f_number = 0.0;		 /**< Aperture value (e.g. 2.8 for F2.8) */
		std::uint32_t iso = 0;			 /**< Raw PTP value for ISO */
		std::int32_t exposure_comp = 0;	 /**< Raw PTP value for exposure compensation */
		double focal_length = 0.0; /**< Lens focal length in millimeters */	
	};

	virtual ~ISimpleCamCtrl() = default;

	/**
	 * @brief Vendor-typed focus position snapshot.
	 *
	 * The shape is vendor-specific; to keep the public API generic we expose a
	 * small numeric snapshot and a type-mask. For now we include SONY-specific
	 * type bits (SONY_*) to allow implementations to report Sony-only fields.
	 */
	struct FocusPositionInfo
	{
		std::uint32_t sony_type_mask = SONY_FOCUS_POSITION_TYPE_NONE;
		std::uint16_t absolute_target = 0; /**< Lens absolute focus drive target (raw vendor unit), not AF area point. */
		std::uint16_t absolute_current = 0; /**< Lens absolute focus drive current value (raw vendor unit), not AF area point. */
		double focal_distance_meters = 0.0; /**< Focal distance in meters (SI). When reported by the device, a raw value of 0xFFFFFFFF indicates infinity. */
		std::uint32_t follow_focus_current = 0;
		double af_area_x = 0.0; /**< AF area point x position as a percentage of frame width in [0, 100]. */
		double af_area_y = 0.0; /**< AF area point y position as a percentage of frame height in [0, 100]. */
		std::uint8_t focus_mode_status = 0;
		std::uint8_t focus_tracking_status = 0;
		bool has_absolute_target = false;
		bool has_absolute_current = false;
		bool has_focal_distance_meter = false;
		bool has_follow_focus_current = false;
		bool has_af_area_position = false;
		bool has_focus_mode_status = false;
		bool has_focus_tracking_status = false;
	};

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
	 * must remain valid for the lifetime of the control object or until replaced
	 * by another @c SetPtpTransport() call.
	 *
	 * A transport may outlive an active session. Implementations should treat
	 * @c Disconnect() as "close the active session" rather than "remove the
	 * transport"; passing @c nullptr here is the explicit way to remove the
	 * current transport.
	 * @param transport Shared pointer to an @c IPTPTransport implementation.
	 * @return true on success.
	 */
	virtual bool SetPtpTransport(IPTPTransportPtr transport) = 0;

	/**
	 * @brief Tear down the active session. Blocking.
	 *
	 * This call should close any live protocol/session state but does not imply
	 * removing an injected transport. To remove the transport itself, call
	 * @c SetPtpTransport(nullptr).
	 */
	virtual void Disconnect() = 0;

	/**
	 * @brief Returns connection state.
	 * @return true if connected and ready to use.
	 */
	virtual bool IsConnected() const = 0;

	/**
	 * @brief Get a user-facing device name if available.
	 *
	 * Implementations may return an empty string when no friendly name is
	 * available from the backend.
	 */
	virtual std::string GetFriendlyName() const { return std::string(); }
	/** @} */

	/**
	 * @brief Update (fetch) cached device state. Blocking.
	 *
	 * Implementations should refresh their internal snapshot so subsequent
	 * getters return up-to-date values.
	 *
	 * @note Implementations MAY perform auto-refresh in background (e.g. a
	 * polling worker). In that case callers can use getters directly as the
	 * cache is continuously updated. If the implementation does not do
	 * auto-refresh, callers should call UpdateStatus() explicitly when
	 * fresh values are required.
	 * @return true on success.
	 */
	virtual bool UpdateStatus() = 0;

	/**
	 * @brief Get last-updated exposure parameters.
	 * @param out_params Output parameter that will be filled with the last known values.
	 * @note This API returns cached values from the last successful
	 *       UpdateStatus() call. Without UpdateStatus(), returned values may be
	 *       stale.
	 * @return true if valid parameters are available (e.g., after successful UpdateStatus()).
	 */
	virtual bool GetExposureParams(ExposureParams &out_params) const = 0;

	/**
	 * @brief Retrieve cached focus-related information.
	 * @param out_info Output snapshot filled from the last UpdateStatus() call.
	 * @return true if at least one focus-related field is available.
	 */
	virtual bool GetFocusPositionInfo(FocusPositionInfo &out_info) const = 0;

	/**
	 * @brief Best-effort setter for lens absolute focus position (vendor raw units).
	 *
	 * This controls lens focus drive position (for example Sony 0xE042) and is
	 * different from AF area point coordinates (x,y).
	 *
	 * Implementations should keep this call non-blocking where possible and
	 * avoid long API locks; the semantics are "best-effort" — failure or
	 * timeout is acceptable and should be reported via the return value.
	 */
	virtual bool SetFocusPositionBestEffort(std::uint16_t raw_position) = 0;

	/**
	 * @brief Best-effort setter for AF area point coordinates.
	 *
	 * Callers specify the point as normalized percentages of the frame: x is
	 * the width percentage and y is the height percentage, both in [0, 100].
	 * Implementations are responsible for converting the normalized values to
	 * the device/vendor-specific coordinate range before sending the command.
	 *
	 * @param x_percent AF point x position as a percentage of frame width.
	 * @param y_percent AF point y position as a percentage of frame height.
	 * @return true on success.
	 */
	virtual bool SetAfAreaPositionBestEffort(double x_percent, double y_percent) = 0;

	/**
	 * @brief Best-effort setter for AF area box size and position.
	 *
	 * Callers specify the AF box dimensions and location as normalized frame
	 * percentages in [0, 100]. Implementations are responsible for converting
	 * those percentages to the device/vendor-specific coordinate range before
	 * sending the command.
	 *
	 * @param height_percent AF box height as a percentage of frame height.
	 * @param width_percent AF box width as a percentage of frame width.
	 * @param x_percent AF box x position as a percentage of frame width.
	 * @param y_percent AF box y position as a percentage of frame height.
	 * @return true on success.
	 */
	virtual bool SetAfFreeSizeAndPositionBestEffort(double height_percent,
	                                                double width_percent,
	                                                double x_percent,
	                                                double y_percent) = 0;

	/**
	 * @brief Best-effort setter for AF area mode using vendor raw codes.
	 * @param raw_area_mode Vendor AF area mode code (e.g. Sony Flexible Spot M).
	 * @return true on success.
	 */
	virtual bool SetAfAreaModeBestEffort(std::uint16_t raw_area_mode) = 0;

	/**
	 * @brief Best-effort setter for focus mode using vendor raw codes (e.g. Sony MF/AF).
	 */
	virtual bool SetFocusModeBestEffort(std::uint32_t raw_mode) = 0;

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
	 * @brief Change exposure parameters using physical values.
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

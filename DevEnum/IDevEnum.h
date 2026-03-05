// Lite header.
// Defines prototype-only interface for generic device enumeration and opening.
// This module keeps stdlib-only dependency boundary.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief 类型擦除设备句柄。
 */
using TOpaqueDeviceHandle = std::shared_ptr<void>;

/**
 * @brief 设备展示信息（通用且宽松）。
 *
 * 不假设所有字段都可用：
 * - 不可用时可置为空字符串。
 * - 例如 Windows 下可将 COM 路径写入 `sDeviceLocation`。
 */
struct TDevEnumDeviceInfo
{
	std::int32_t nDeviceIndex = -1;
	std::string sDeviceName;
	std::string sDeviceType;
	std::string sDeviceLocation;
	std::vector<std::pair<std::string, std::string>> vExtraFields;
};

/**
 * @brief 通用设备枚举接口（仅原型）。
 *
 * 仅提供三项能力：
 * 1) Refresh()
 * 2) ListDevices()
 * 3) OpenByIndex()
 *
 * @tparam TCreateParams 创建设备所需参数类型，由具体实现决定其内容。
 * @tparam TDeviceInfo 枚举返回的设备展示结构体类型。
 */
template <typename TCreateParams, typename TDeviceInfo = TDevEnumDeviceInfo>
class IDevEnum
{
public:
	virtual ~IDevEnum() = default;

	/**
	 * @brief 刷新设备快照。
	 * @return true 表示刷新成功。
	 */
	virtual bool Refresh() = 0;

	/**
	 * @brief 返回可展示的设备信息列表。
	 * @return 当前快照下的设备信息集合。
	 */
	virtual std::vector<TDeviceInfo> ListDevices() const = 0;

	/**
	 * @brief 按索引打开设备并返回类型擦除句柄。
	 *
	 * caller 在已知实现上下文中执行强制转换。
	 * @param nDeviceIndex 设备索引。
	 * @param stCreateParams 创建参数。
	 * @return 成功时返回设备句柄，失败时返回空句柄。
	 */
	virtual TOpaqueDeviceHandle OpenByIndex(std::int32_t nDeviceIndex, const TCreateParams& stCreateParams) = 0;
};
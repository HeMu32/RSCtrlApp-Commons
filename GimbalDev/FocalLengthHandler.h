#ifndef FOCALLENGTHHANDLER_H
#define FOCALLENGTHHANDLER_H

#include <vector>
#include <utility>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <cstdio>

#include <QSettings>
#include <QDebug>


#define SETTINGS_INI_PATH "calibrationConfig.ini"

class FocalLengthHandler
{
private:
    // 用于映射焦距和电机位置的数据点
    std::vector<std::pair<int16_t, int16_t>> vecMotorCalPoints;

    float fCropRatio    = 1.1;      // Full frame 16:9
    float fAspetRatio   = 1.778;    // Full frame 16:9
    
    // 添加互斥锁用于线程安全
    mutable std::mutex mtx;

public:
    FocalLengthHandler();
    ~FocalLengthHandler() = default;

    /// @brief          设置焦距到对焦电机位置的映射点
    /// @param uiFocal  焦距
    /// @param uiPos    对焦电机位置，范围 0 ~ 4095
    /// @return         成功返回0，失败返回负值
    int set_FocalLen_to_FocusMotoPos(uint16_t uiFocal, uint16_t uiPos);

    /// @brief          从对焦电机位置解析焦距
    /// @param uiPos    对焦电机位置，范围 0 ~ 4095
    /// @return         当前位置的焦距，错误时返回负值
    int get_FocalLen_from_FocusMotorPos(uint16_t uiPos) const;

    /// @brief          获取对焦电机校准数据
    /// @return         校准数据点的向量
    std::vector<std::pair<int16_t, int16_t>> get_FocusMotorCalData();

    /// @brief  Getter of crop ratio and aspect ratio
    /// @return 
    std::pair<float, float> get_Crop_and_Aspect() const;

    /// @brief Set crop and aspect
    void set_Crop_and_Aspect(float fCrop, float fAspect);

    /// @brief          清除对焦电机校准数据
    void clear_FocusMotorCalData();

    // 
    void saveCalibrationsToFile();
    
    // 
    void loadCalibrationsFromFile();
};

#endif // FOCALLENGTHHANDLER_H
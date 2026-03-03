#include "FocalLengthHandler.h"

FocalLengthHandler::FocalLengthHandler()
{
    // 构造函数
    // 在构造函数中加载校准数据
    loadCalibrationsFromFile();
}

FocalLengthHandler::~FocalLengthHandler()
{
    // 析构函数
    // 在析构函数中保存校准数据
    // saveCalibrationsToFile();
}

int FocalLengthHandler::set_FocalLen_to_FocusMotoPos(uint16_t uiFocal, uint16_t uiPos)
{
    if (uiPos > 4095)
        return -1;
    
    std::pair<uint16_t, uint16_t> DataPoint = {uiFocal, uiPos};
    
    // 加锁保护数据访问
    std::lock_guard<std::mutex> lock(mtx);
    
    if (vecMotorCalPoints.empty())
    {   // empty list
        vecMotorCalPoints.insert(vecMotorCalPoints.begin(), DataPoint);
        return 0;
    }
    
    // Check if the position already exists
    for (unsigned ui = 0; ui < vecMotorCalPoints.size(); ui++)
    {
        if (uiPos == vecMotorCalPoints.at(ui).second)
        {
            // If already exists, update the focal length
            vecMotorCalPoints.at(ui).first = uiFocal;
            return 0;
        }
    }
    
    if (uiPos < vecMotorCalPoints.at(0).second)
    {   // if input positoin smaller than the smallest position mark
        vecMotorCalPoints.insert(vecMotorCalPoints.begin(), DataPoint);
        return 0;
    }
    for (unsigned ui = 0; ui < vecMotorCalPoints.size(); ui++)
    {
        if (uiPos >= vecMotorCalPoints.at(ui).second)
        {   // 如果找到一个位置，其值小于等于当前值，且下一个位置（如果存在）大于当前值
            // 如果是最后一个元素或者下一个元素的值大于当前值，则插入到这个位置之后
            if (ui == vecMotorCalPoints.size() - 1 || uiPos < vecMotorCalPoints.at(ui + 1).second)
            {
                vecMotorCalPoints.insert(vecMotorCalPoints.begin() + ui + 1, DataPoint);
                return 0;
            }
        }
    }
#ifdef _DEBUG
    for (unsigned ui = 0; ui < vecMotorCalPoints.size(); ui++)
    {   // print to console, not reachable in normal states as all code above has a return statement
        printf("No.%u %u %u\n", 
                ui,
                vecMotorCalPoints.at(ui).first, 
                vecMotorCalPoints.at(ui).second);
    }
#endif
    return -1;
}

int FocalLengthHandler::get_FocalLen_from_FocusMotorPos(uint16_t uiPos)
{
    if (uiPos > 4095)
        return -1;
    
    // 加锁保护数据访问
    std::lock_guard<std::mutex> lock(mtx);
    
    if (vecMotorCalPoints.empty())
        return -1;
        
    std::pair<int16_t, int16_t> *pLarger    = NULL;     // the closest larger  (or equal) one in records
    std::pair<int16_t, int16_t> *pSmaller   = NULL;     // the closest smaller (or equal) one in records
    
    for (std::pair<int16_t, int16_t> &item : vecMotorCalPoints)
    {
        if (item.second >= uiPos)
        {   // 找到大于等于输入值的记录
            if (pLarger == NULL || item.second < pLarger->second)
            {   // 找到更接近的大于等于值
                pLarger = &item;
            }
        }
        if (item.second <= uiPos)
        {   // 找到小于等于输入值的记录
            if (pSmaller == NULL || item.second > pSmaller->second)
            {   // 找到更接近的小于等于值
                pSmaller = &item;
            }
        }
    }

    if ((!pLarger) && (!pSmaller))
        return -1;
    
    if (!pSmaller)
        return pLarger->first;
    
    if (!pLarger)
        return pSmaller->first;
    
    if (pLarger->second == pSmaller->second)
        return pSmaller->first;  // avoid divide by 0
        
    int ret = (int)(((float)(pLarger->first - pSmaller->first) / (pLarger->second - pSmaller->second)) * 
                    (uiPos - pSmaller->second) + 
                    pSmaller->first);
            
    return ret;

/*  
    if (uiPos < vecMotorCalPoints.at(0).second)
    {   // smaller than the first mark: out of range, return the focal of the first mark
        return vecMotorCalPoints.at(0).first;
    }
    for (unsigned ui = 0; ui < vecMotorCalPoints.size(); ui++)
    {   // go thru the list
        if (uiPos > vecMotorCalPoints.at(ui).second)
        {   // if input position bigger than the current seeked position mark
            if (ui >= vecMotorCalPoints.size() - 1)
            {   // if bigger than last mark: out of range, return the focal of the last mark
                return vecMotorCalPoints.at(ui).first;
            }

            std::pair<int16_t, int16_t> point1 = vecMotorCalPoints.at(ui);
            std::pair<int16_t, int16_t> point2 = vecMotorCalPoints.at(ui + 1);

            printf ("No.%u : %d %d \n", ui, point1.first, point1.second);
            printf ("No.%u : %d %d \n", ui + 1, point2.first, point2.second);
            printf ("\n");

            if (point2.second == point1.second) 
                continue;

            int ret = (int)(((float)(point2.first - point1.first) / (point2.second - point1.second)) * \
                             (uiPos - point1.second) + \
                             point1.first);
            
            return ret;
        }
    }
*/

    return -1;
}

std::vector<std::pair<int16_t, int16_t>> FocalLengthHandler::get_FocusMotorCalData()
{
    // 加锁保护数据访问
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<std::pair<int16_t, int16_t>> ret = vecMotorCalPoints;
    return ret;
}

std::pair<float, float> FocalLengthHandler::get_Crop_and_Aspect()
{
    // 加锁保护数据访问
    std::lock_guard<std::mutex> lock(mtx);
    return {fCropRatio, fAspetRatio};
}

void FocalLengthHandler::set_Crop_and_Aspect(float fCrop, float fAspect)
{
    // 加锁保护数据访问
    std::lock_guard<std::mutex> lock(mtx);
    fCropRatio    = fCrop;
    fAspetRatio   = fAspect;
    return;
}

void FocalLengthHandler::clear_FocusMotorCalData()
{
    // 加锁保护数据访问
    std::lock_guard<std::mutex> lock(mtx);
    vecMotorCalPoints.clear();
    this->fCropRatio    = 1.1;      // Reset to full frame 16:9
    this->fAspetRatio   = 1.778;    // Reset to full frame 16:9
}

// 新增方法：保存校准数据到文件
void FocalLengthHandler::saveCalibrationsToFile()
{
    // 获取需要保存的数据
    std::vector<std::pair<int16_t, int16_t>> motorCalData;
    std::pair<float, float> cropAndAspect;
    
    {
        // 加锁保护数据访问
        std::lock_guard<std::mutex> lock(mtx);
        motorCalData = vecMotorCalPoints;
        cropAndAspect = {fCropRatio, fAspetRatio};
    }

    // 创建 QSettings 对象，指定 .ini 文件路径
    QSettings settings(SETTINGS_INI_PATH, QSettings::IniFormat);

    // 保存 motorCalData
    settings.beginGroup("MotorCalibration");
    QStringList motorCalList;
    for (const auto &point : motorCalData)
    {
        motorCalList.append(QString("%1,%2").arg(point.first).arg(point.second));
    }
    settings.setValue("Points", motorCalList);
    settings.endGroup();

    // 保存 CropRatio 和 AspectRatio
    settings.beginGroup("CropAndAspect");
    settings.setValue("CropRatio", cropAndAspect.first);
    settings.setValue("AspectRatio", cropAndAspect.second);
    settings.endGroup();
    
    qDebug() << "FocalLenHandler: Calibration data saved to file successfully.";
}

// 新增方法：从文件加载校准数据
void FocalLengthHandler::loadCalibrationsFromFile()
{
    // 创建 QSettings 对象，指定 .ini 文件路径
    QSettings settings(SETTINGS_INI_PATH, QSettings::IniFormat);

    // 获取所有的 MotorCalibration 子组
    QStringList motorCalibrationGroups = settings.childGroups();
    for (const QString &group : motorCalibrationGroups)
    {
        if (group.startsWith("MotorCalibration"))
        {
            settings.beginGroup(group);
            QStringList motorCalList = settings.value("Points").toStringList();
            for (const QString &pointStr : motorCalList)
            {
                QStringList parts = pointStr.split(",");
                if (parts.size() == 2)
                {
                    bool okX, okY;
                    int16_t x = parts[0].toInt(&okX); // 第一个值作为 focalLengthValue
                    int16_t y = parts[1].toInt(&okY); // 第二个值作为 m_zoomvalue

                    if (okX && okY)
                    {
                        // 调用 set_FocalLen_to_FocusMotoPos
                        set_FocalLen_to_FocusMotoPos(static_cast<uint16_t>(x), static_cast<uint16_t>(y));
                    }
                    else
                    {
                        qDebug() << "FocalLenHandler: Invalid integer conversion for:" << pointStr;
                    }
                }
                else
                {
                    qDebug() << "FocalLenHandler: Invalid format for point string:" << pointStr;
                }
            }
            settings.endGroup();
        }
    }

    // 加载 CropRatio 和 AspectRatio 数据
    settings.beginGroup("CropAndAspect");
    QVariant cropRatioVariant = settings.value("CropRatio");
    QVariant aspectRatioVariant = settings.value("AspectRatio");

    float cropRatioValue = cropRatioVariant.toFloat();
    float aspectRatioValue = aspectRatioVariant.toFloat();
    settings.endGroup();

    // 调用 set_Crop_and_Aspect
    if (cropRatioValue > 0 && aspectRatioValue > 0) {
        set_Crop_and_Aspect(cropRatioValue, aspectRatioValue);
    }

    qDebug() << "FocalLenHandler: Calibration data loaded from file successfully.";
}

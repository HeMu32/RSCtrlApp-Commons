# ADR-006: GimbalDev 接口设计

- 状态: Accepted
- 日期: 2026-03-10

## 背景

母项目 `RSCtrlApp` 中的 `GimbalInstanceHandler` 直接持有 DJI Ronin SDK 实例，耦合程度很高：
- 指令方法（`slotMoveTo`、`slotJoystickPushMovement` 等）通过 Qt Signal/Slot 与 UI 层绑定，无法脱离 Qt 事件循环复用。
- 位置更新已经是 callback 驱动（`set_position_update_callback`），但位置数据仍以轮询（`queryPosition` + 374ms 节流）补充，混用两种模式。
- 角度限位逻辑、`FocalLengthHandler` 的校准数据持久化均在同一个类中处理，职责不清。

重构目标是在 `RSCtrlApp-Commons/GimbalDev` 中抽象出与具体 SDK 无关的 `IGimbalDev` 接口，供母项目和未来其他接入层统一使用。

## 决策驱动因素

- **可拓展性**：未来可能接入非 DJI 云台；接口层不应包含任何 SDK 专属类型。
- **异步优先**：所有数据读取（位置、状态、对焦标定结果）通过 callback 推送，调用方不阻塞等待。
- **指令 fire-and-forget**：发送指令不返回结果值，结果（成功/失败/超时）通过状态 callback 或错误 callback 回传。
- **与 Qt 解耦**：接口头文件本身不依赖 Qt。`FocalLengthHandler` 目前依赖 `QSettings`，但作为独立工具类放置在实现层，不污染 `IGimbalDev` 接口。
- **限位逻辑内聚**：角度限位属于协议层约束，应在实现内部处理，接口层只提供配置入口，不暴露裁剪逻辑。

## 决策

### 1. 回调结构体 `TGimbalDevCallbacks`

所有异步数据通过统一回调结构体注入，在 `Open()` 或 `SetCallbacks()` 时一次性设置：

```cpp
struct TGimbalDevCallbacks
{
    // 云台位置更新（单位：0.1°，与 DJI Ronin SDK 保持一致）
    std::function<void(int16_t nYaw, int16_t nRoll, int16_t nPitch)> fnPositionUpdate;

    // 连接状态变化
    std::function<void(EGimbalDevState eState)> fnStateChanged;

    // 错误通知
    std::function<void(EGimbalDevError eError, const std::string& sMsg)> fnError;

    // 对焦电机自动标定完成（可选）
    std::function<void()> fnFocusMotorCalComplete;
};
```

**理由**：与 `ILiveInput` 的 `TLiveInputCallbacks` 模式一致，保持 Commons 层设计风格统一。  
所有字段均为 `std::function`，未设置时保持为空（实现层调用前检查是否有效）。

### 2. 生命周期方法

| 方法 | 语义 |
|---|---|
| `Open(nDevIndex, nCanIndex, callbacks)` | 建立连接，注册回调，成功后状态变为 `Connected` |
| `Close()` | 断开连接，释放资源，幂等 |
| `IsOpen()` | 连接状态快捷查询 |
| `GetState()` | 返回完整 `EGimbalDevState` |

`Open()` 返回 `EGimbalDevError`，`Ok` 表示连接成功。不采用纯 bool，原因是失败原因有助于调用方展示诊断信息。

### 3. 运动指令（fire-and-forget）

```
CmdMoveTo(nYaw, nRoll, nPitch, nTimeMs)   // 绝对角度, 移动到指定朝向
CmdJoystickMove(nX, nY)                    // 摇杆, 相对值时域增量移动
```

指令方法返回 `void`，不阻塞。若设备未连接，实现内部静默忽略或通过 `fnError` 通知（实现自定义）。

**不在接口层定义移动模式**（ABSOLUTE/SPEED）：模式由实现层在 `Open()` 内部固定或通过扩展子类配置，原因是 DJI Ronin SDK 中测试稳定的模式仅 `ABSOLUTE_CONTROL`，过早抽象会引入无法验证的分支。

### 4. 对焦电机指令

```
CmdFocusMotorAutoCal()            // 触发自动标定流程，完成时回调 fnFocusMotorCalComplete
CmdSetFocusMotorPos(uiPos)        // 直接设置电机位置，范围 0~4095
CmdFocusMotorMoveRel(nSpeed)      // 设置增量速度/时域相对位置移动，下层按设备支持作时域积分
```

### 5. 角度限位配置

```cpp
void SetAxisLimits(int16_t nYawA, int16_t nYawB,
                   int16_t nRolA, int16_t nRolB,
                   int16_t nPitA, int16_t nPitB);  // 单位: 0.1°，A/B 同为 0 表示无限位
void GetAxisLimits(int16_t& nYawA, int16_t& nYawB,
                   int16_t& nRolA, int16_t& nRolB,
                   int16_t& nPitA, int16_t& nPitB) const;
void ClearAxisLimits();  // 清除全部限位
```

限位裁剪逻辑在实现层的指令发送路径中统一执行，调用方无感知。
单位统一使用 **0.1°**（与 DJI Ronin SDK 的原生单位一致），避免在接口层做整数/浮点转换。

### 6. FocalLengthHandler 的位置

`FocalLengthHandler` **不纳入 `IGimbalDev` 接口**，理由：
- 并非所有云台实现都具备可程控对焦电机。
- 焦距映射属于镜头标定数据，与云台运动控制正交，合并在接口中会破坏单一职责原则。
- 实现类（如 `DJIRoninDev`）可以持有 `FocalLengthHandler` 成员，并在 `CmdSetFocusMotorPos` 中使用其插值结果。

调用方若需读取焦距，通过实现类的扩展方法或单独暴露的 `FocalLengthHandler` 引用获取。

### 7. 位置查询主动触发（QueryPosition）

保留 `QueryPosition()` 作为可选的主动查询接口，结果依然通过 `fnPositionUpdate` 回调返回，不阻塞：

```cpp
void CmdQueryPosition();  // 触发一次位置查询，结果通过 fnPositionUpdate 回调
```

实现层可在内部做节流（如原 374ms 限制），接口层不规定节流策略。

## 备选方案

### 方案 A：保留 Qt Signal/Slot 作为主回调机制

- 优点：与母项目 UI 层集成最直接。
- 缺点：接口层引入 Qt 依赖，`IGimbalDev.h` 需要 `QObject` 继承，无法在非 Qt 上下文复用。
- **结论：不采用**。调用方可在 `fnPositionUpdate` 等回调内自行 `emit signal`，实现 Qt 集成。

### 方案 B：所有指令也返回 `std::future`

- 优点：调用方可选择等待指令完成确认。
- 缺点：DJI Ronin SDK 本身无同步确认机制，返回的 `future` 无法携带真实完成语义，会产生误导性 API。
- **结论：不采用**。保持 fire-and-forget，用 `fnError` 和 `fnStateChanged` 处理异常反馈。

### 方案 C：将 FocalLengthHandler 纳入接口（作为可选能力 Capability 标志）

- 优点：接口描述更完整。
- 缺点：增加接口复杂度，且大多数云台实现不具备此能力，会导致大量空实现。
- **结论：不采用**。由实现类自行扩展。

## 影响

- 正向：
    - 接口层零 Qt / 零 SDK 依赖，可在单元测试或非 Qt 上下文 mock。
    - 所有数据回流均异步，与 Commons 其他模块（`ILiveInput`、`FrameDispatcher`）风格一致。
    - 限位逻辑集中在实现层，接口稳定，上层代码无需感知裁剪细节。
    - `FocalLengthHandler` 保持独立，可被镜头控制层（`CamCtrl`）或 `GimbalDev` 实现共享。

- 负向：
    - 调用方需自行维护回调生命周期（注意 `this` 指针悬空风险）。
    - 无内建的指令完成确认，依赖 `fnPositionUpdate` 间接推断运动是否到位。

## 落地范围

- 新增/更新：`GimbalDev/IGimbalDev.h`（接口定义）
- 后续实现：`GimbalDev/DJIRoninDev.h/.cpp`（DJI Ronin 具体实现，引用现有 `DJIR_SDK`）
- `FocalLengthHandler.h/.cpp` 保留在 `GimbalDev/` 目录下，由 `DJIRoninDev` 持有和使用

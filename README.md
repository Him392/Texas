# 德克萨斯 源石剑 自制道具（重制版）

明日方舟「德克萨斯」源石剑的自制道具，包含 3D 打印手柄、亚克力剑身以及 ESP32 LED 控制固件。

视频链接：https://www.bilibili.com/video/BV1i34y1r77z

## 3D 预览

![预览图](屏幕截图%202021-03-21%20145915.png)

## 文件说明

### 手柄部分（3D 打印）
| 文件 | 说明 |
|------|------|
| `Texas_1PCB.STL` / `.SLDPRT` | 手柄主体（含 PCB 安装位） |
| `Texas_2SW.STL` / `.SLDPRT` | 手柄按键盖 |

### 剑身部分
| 文件 | 说明 |
|------|------|
| `Texas_A＆B_V2.DWG` | CAD 文件，用于淘宝定制亚克力板 |
| `Texas_A＆B_V2.SLDPRT` | SolidWorks 原始模型 |

### PCB
| 文件 | 说明 |
|------|------|
| `PCB(LCEDA).zip` | 立创 EDA PCB 工程文件 |

---

## 固件

固件基于 **ESP32-PICO** 平台（PlatformIO + Arduino 框架），LED 通过 LEDC PWM 驱动，支持 Gamma 校正，提供两个版本：

### ESP32_AP — WiFi 热点控制版

- ESP32 开机后创建名为 **德克萨斯的源石剑-B** 的开放热点
- 手机/电脑连接热点后，访问 `192.168.4.1` 即可打开控制页面
- 控制功能：
  - **模式**：关闭 / 常亮 / 呼吸灯 / 闪烁
  - **亮度**调节滑块（常亮 / 闪烁模式下有效）
  - **实体按键**启用 / 禁用
  - 实时显示电池电压与 USB 充电状态
- 实体按键逻辑：
  - **短按**：关闭 ↔ 常亮 切换
  - **长按（250 ms）**：常亮 ↔ 呼吸灯 切换

### ESP32_BLE — 蓝牙 BLE 控制版

- 蓝牙广播名称：**Texas-Sword-B**
- 通过 BLE 特征值读写控制，断连后自动重新广播
- 支持通知推送：按键操作、USB 状态、电池电压实时同步给已连接设备
- BLE Service UUID：`4fafc201-1fb5-459e-8fcc-c5c9c331914b`
- 控制特征值：

| 功能 | UUID |
|------|------|
| 模式 | `beb5483e-36e1-4688-b7f5-ea07361b26a8` |
| 亮度 | `8c3bcf52-4752-46ac-a4de-a89e830e2f5f` |
| 实体按键 | `f0a3cc1e-eeed-426c-9a4f-ee72f778c1ee` |
| 电池电压 | `1de83dc6-3474-45e0-a2ef-cf4c01dcdc8c` |
| USB 状态 | `6bdab28d-19cd-488f-a42e-be255dfda983` |

### 引脚定义（两版通用）

| 信号 | GPIO |
|------|------|
| LED PWM | 12 |
| 实体按键 | 2 |
| USB 电源检测 | 36 |
| 电池电压 ADC | 37 |

---

## 开源协议

<a rel="license" href="http://creativecommons.org/licenses/by-nc-sa/4.0/"><img alt="Creative Commons License" style="border-width:0" src="https://i.creativecommons.org/l/by-nc-sa/4.0/88x31.png" /></a><br />
本项目采用 [CC BY-NC-SA 4.0](http://creativecommons.org/licenses/by-nc-sa/4.0/) 授权。

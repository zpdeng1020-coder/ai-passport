# 随身安全哨兵 (AI Security Sentinel)

> **基于 FoloToy AI Passport 打造的个人随身便携无线安全与反追踪防御系统**  
> 支持 Apple AirTag (Find My)、三星 SmartTag、Tile、华为信标持续伴随侦测、冷热寻物探物、无线偷拍排查与智能熄屏长续航。

---

## 一、 项目简介

**随身安全哨兵 (AI Security Sentinel)** 专为差旅出行、租房入住、公共通勤等场景设计。无需依赖网络或手机 App，通过硬件本地被动空口监听与主动信道探测，时刻守卫您的个人隐私与行踪安全。

它将 AI Passport 化身为一台全天候贴身守护的个人安全雷达：
1. **防追踪雷达**：静默监测身边是否有未知的 Apple AirTag、三星 SmartTag、Tile 或华为信标持续跟随；
2. **冷热寻物探物**：类似盖革计数器的直观声光寻物仪，靠近物理目标时信号条变红、声音急促，帮您盲找隐藏在背包、外套或车底的追踪信标；
3. **无线偷拍排查**：四维立体无线特征分析，快速甄别酒店、更衣室内的隐藏 Wi-Fi、安防芯片模组与微型针孔摄像机；
4. **智能熄屏省电**：随身放包里时自动熄屏待机（续航延长至 26~35 小时），后台全速侦听不中断，遇险瞬间自动亮屏报警，并具备口袋防误触机制。

---

## 二、 4 大核心功能与操作指南

```
┌─────────────────────────────────────────────────────────────┐
│                      安全哨兵功能主菜单                       │
│      [1. 防追踪雷达]               [2. 冷热寻物]             │
│      [3. 偷拍排查]                 [4. 白名单]               │
│                   ₍ᐢ. .ᐢ₎ (跳跃吉祥物)                      │
│                  上下:选择功能 | OK:进入功能                  │
└─────────────────────────────────────────────────────────────┘
```

### 1. 防追踪雷达 (demo_radar)
- **功能**：后台静默侦听空口广播包。基于时间窗口滑动算法，排除地铁、商场擦肩而过的路人信标，仅对长时间近距离持续跟随的陌生信标触发警报。
- **界面指示**：顶部显示安全/告警状态标牌，中间显示实时抓包数与目标统计，下方展示信标卡片（目标类型、MAC 地址、瞬时 RSSI、邻近度、伴随时长与威胁评估）。
- **按键操作**：
  - `UP` / `DOWN`：切换浏览当前捕捉到的信标目标；
  - `OK`（短按）：将当前选中的目标加入/移出信任白名单；
  - `OK`（长按）：返回安全哨兵主菜单。

### 2. 冷热寻物 (demo_hotcold)
- **功能**：当防追踪雷达提示周围有跟随信标时，进入本模式进行物理定位。
- **界面与声效**：根据实时 RSSI 强度驱动 0~100% 动态进度条（蓝冷 → 黄微温 → 橙温 → 红极热），同时通过扬声器发出自适应盖革脉冲音（距离越近频率越高，1000Hz~3000Hz），实现精准盲找。
- **按键操作**：
  - `UP` / `DOWN`：切换寻物目标；
  - `OK`（长按）：返回主菜单。
- **熄屏策略**：寻物过程中**强制全程常亮（抑制熄屏）**，绝不打断寻物视线。

### 3. 无线偷拍排查 (demo_wifi_spy)
- **功能**：一键执行 2.4GHz 全频段 14 信道主动探测，排查暗藏的无线偷拍摄像机与隐藏热点。
- **排查结果**：全量展示发现的所有可疑目标，详细标注热点名称/隐藏标记、判定原因、芯片厂商与 OUI、信号强度与工作信道。
- **按键操作**：
  - `UP` / `DOWN`：上下平滑滑动浏览全部扫描结果；
  - `OK`（短按）：重新执行全信道排查；
  - `OK`（长按）：返回主菜单。

### 4. 信任白名单 (demo_whitelist)
- **功能**：集中管理用户已加白的熟人或自己的 AirTag、手环或耳机，白名单设备永不触发跟随告警。
- **按键操作**：
  - `UP` / `DOWN`：上下滑动查看全部白名单条目；
  - `OK`（短按）：一键清空白名单；
  - `OK`（长按）：返回主菜单。

---

## 三、 系统架构与权威设计出处

本项目遵循严格的标准协议与工程实现，所有判定常量与算法均具备行业权威规范支撑：

### 1. BLE 追踪信标判定出处与协议标准
- **Bluetooth SIG 官方分配公司 ID (Assigned Numbers)**：
  - `0x004C`：**Apple, Inc.**（所有苹果 BLE 广播起始标识）；
  - `0x0075`：**Samsung Electronics Co. Ltd.**（三星 SmartTag / SmartTag2）；
  - `0x011A`：**Tile, Inc.**；
  - `0x027D`：**Huawei Device Co., Ltd.**（华为信标）。
- **Tile 专有服务 UUID**：Bluetooth SIG 官方 16-bit Service UUID 注册表之 `0xFEED`。
- **Apple Find My (AirTag) 载荷规范**：
  - **IETF 国际标准草案**：Apple 与 Google 联合制定的官方规范《*Detecting Unwanted Location Trackers*》(DULT)；
  - **SEEMOO 实验室规范**：达姆施塔特工业大学 OpenHaystack / AirGuard 规范。苹果离线广播固定为子类型 `0x12`，随附 `0x19`（25 字节载荷：公钥片段+状态+Hint）。
- **Apple 随身设备规范**：
  - `0x10`：Apple Nearby Info / Action（苹果 Continuity 邻近协同协议）；
  - `0x07`：AirPods Proximity Pairing（开盒弹窗配对广播）；
  - `0x05`：AirDrop（隔空投送发现广播）。

### 2. 持续伴随威胁评估模型 (Threat Timeline State Machine)
单纯偶遇不报警，唯有持续伴随方为威胁：
- **安全 (SAFE)**：未发现追踪信标，或信标仅短暂出现（< 3 分钟）；
- **提示 (NOTICE)**：近场（RSSI $\ge -75$ dBm）出现陌生信标；
- **高危报警 (ALERT)**：陌生信标在持续 **5 分钟** 的滑动时间窗口内被连续捕获且平滑 RSSI 处于近场，触发高危警报、蜂鸣器脉冲并**瞬间点亮屏幕**。

### 3. 无线偷拍排查四维立体判决模型
针对酒店针孔摄像头的现实部署方式，构建四维检测模型：
1. **隐藏 SSID 强信号排查 (`SPY_REASON_HIDDEN_SSID`)**：微型摄像机多关闭 SSID 广播防手机察觉，开启 `show_hidden = true` 抓取隐藏 Beacon，信号 $\ge -70$ dBm 判定为高疑同室隐藏设备；
2. **21 种摄像机前缀指纹库 (`SPY_REASON_CAMERA_PREFIX`)**：匹配 `LOOKCAM`, `IWFCAM`, `HDWIFICAM`, `V380`, `IPCAM`, `MINICAM`, `CARECAM`, `SPY`, `XVR`, `DVR`, `CAM-`, `IPC-`, `TUYA`, `SMARTLIFE`, `HISILICON`, `WIFICAM` 等常见出厂热点前缀；
3. **安防模组 IEEE MAC OUI 识别 (`SPY_REASON_VENDOR_OUI`)**：提取 BSSID 前 3 字节对比芯片厂商硬件指纹：
   - **涂鸦智能 (Tuya Smart)**：`10:5A:F7`, `20:F8:5E`, `7C:F6:66`, `68:57:2D`, `84:F3:EB`, `A0:92:08`, `D8:1F:12`
   - **雄迈安防 (Xiongmai)**（公模微型摄像头核心供应商）：`00:12:12`, `00:12:16`, `00:12:17`, `00:12:18`
   - **海康威视 (Hikvision)**：`44:19:B6`, `70:B3:D5`, `BC:54:51`, `18:68:CB`
   - **大华安防 (Dahua)**：`38:AF:29`, `E0:50:8B`, `4C:11:BF`, `90:02:A9`
   - **乐鑫 IoT 模组 (DIY/改装偷拍)**：`24:0A:C4`, `30:AE:A4`, `A4:CF:12`, `DC:4F:22`
4. **贴身超强信号源预警 (`SPY_REASON_SUSPICIOUS_STRONG`)**：近场信号 $\ge -45$ dBm（1~2 米内）标记异常强源。

### 4. 智能熄屏省电与防误触状态机
- **场景区分矩阵**：
  - **允许熄屏**：防追踪雷达随身放包、主菜单、白名单、偷拍静态结果页（35 秒微暗 20%，45 秒完全熄灭 0%）；
  - **禁止熄屏（常亮抑制）**：冷热寻物手持探物期间全程常亮、Wi-Fi 全信道扫描期间常亮、高危报警激活期间常亮。
- **熄屏期间后台全速侦听**：熄屏仅由 LEDC 将背光降为 0%，MCU 与 NimBLE 蓝牙扫描任务在后台 100% 持续抓包运算；
- **高危告警自动唤醒**：后台一旦检测到高危跟随，瞬间调用 `tracker_power_wake()` 恢复背光 100% 并鸣响报警；
- **口袋防误触设计**：在完全熄屏状态下，从口袋掏出设备按下的**第一下按键仅负责唤醒屏幕，事件被电源管理器拦截消费，绝不透传给底层业务**，杜绝误清空白名单或误切功能。
- **功耗收益**：整机工作电流从 55~70mA 大幅降低至 15~20mA，单次充电续航提升至 **26~35 小时**。

---

## 四、 官方代码规范与安全审计对齐

1. **按键规范遵循 (`coding-conventions.zh_CN.md`)**：
   - 长按确定键统一由 `main.c` 顶层拦截，退出各子功能模块并返回主菜单；
   - 子页面按键与主菜单按键职责彻底解耦。
2. **出厂关键保护分区 (`partitions.csv`)**：
   - 出厂唯一身份分区 `cardid@0x356000` 受到绝对保护，固件仅占用 `0x10000 ~ 0x1f09d0`，留有大于 1.4MB 的绝对安全间距。
3. **中文字库与内存安全**：
   - 提取自官方验证的 `lv_font_cn_16` 3755 常用汉字纯 const Flash 字库，不占用宝贵的内部 SRAM；
   - 配合 `safe_text_filter` 杜绝方块豆腐块（Tofu）缺字乱码。

---

## 五、 编译、测试与烧录

### 1. 本地 Host 单元测试
无需硬件即可在 PC/Mac 上瞬间运行全部核心逻辑回归测试：
```bash
# 测试核心反追踪状态机与 Wi-Fi 判定
gcc -Wall -Wextra -I. -Imain tests/test_tracker_engine.c main/tracker_engine.c main/tracker_wifi_spy.c -o test_runner && ./test_runner && rm -f test_runner

# 测试智能电源管理与熄屏状态机
gcc -Wall -Wextra -I. -Imain tests/test_tracker_power.c main/tracker_power.c -o test_power_runner && ./test_power_runner && rm -f test_power_runner
```

### 2. 固件编译 (ESP-IDF v5.5.3)
```bash
. ~/esp/esp-idf-v5.5.3/export.sh
idf.py build
```

### 3. 实机烧录
```bash
python3 -m esptool --chip esp32c3 --port /dev/cu.usbmodem101 -b 460800 \
  --before default_reset --after hard_reset write_flash \
  --flash-mode dio --flash-size 8MB --flash-freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/FoloToy-AI-Passport.bin
```

---

## 六、 开源许可与致谢

- 本项目基于 [folotoy/ai-passport](https://github.com/folotoy/ai-passport) 官方基线开发；
- 协议标准参考：Bluetooth SIG Assigned Numbers、IETF DULT 规范、TU Darmstadt SEEMOO 实验室 OpenHaystack & AirGuard 开源项目；
- 遵循 Apache-2.0 / MIT 开源许可证。

# FoloToy AI Passport 智能体接手开发全景指南与规范手册

> **版本**：v1.0.0 (Production Playbook)  
> **适用对象**：接手维护当前项目（如随身安全哨兵）或基于 FoloToy AI Passport 硬件平台开发全新玩法/应用的 AI Agent。  
> **核心宗旨**：**“开发前必读官方源码，开发后必做闭环审计；严守安全红线与物理边界，拒绝凭空捏造与臆想开发。”**

---

## 目录
1. [项目背景与本手册使命](#一-项目背景与本手册使命)
2. [八大绝对禁令与核心红线（Strict Prohibitions）](#二-八大绝对禁令与核心红线strict-prohibitions)
3. [硬件环境、分区表与内存架构深度解析](#三-硬件环境分区表与内存架构深度解析)
4. [官方框架与开发规范（开发前必读）](#四-官方框架与开发规范开发前必读)
   - 4.1 中文字库渲染与“豆腐块乱码”根治范式
   - 4.2 按键交互解耦与“长按顶层拦截”原则
   - 4.3 列表与页面滚动视图规范
   - 4.4 蓝牙 (NimBLE) 与 Wi-Fi 内存平衡
5. [四大核心功能实现与权威参考标准](#五-四大核心功能实现与权威参考标准)
   - 5.1 防追踪雷达与 IETF DULT / Bluetooth SIG 规范
   - 5.2 偷拍排查物理边界与四维立体判决模型
   - 5.3 智能熄屏电源管理与口袋防误触状态机
   - 5.4 盖革冷热寻物音频变频发生器
6. [标准化研发流程与质量把控](#六-标准化研发流程与质量把控)
7. [本地仿真、Host 单元测试与安全烧录规范](#七-本地仿真host-单元测试与安全烧录规范)
8. [多渠道发布全流程与凭据安全](#八-多渠道发布全流程与凭据安全)
   - 8.1 FoloToy 官方社区发布规范
   - 8.2 GitHub 开源协同与 Fine-grained PAT 权限避坑
9. [新 Agent 接手自查清单 (Checklist)](#九-新-agent-接手自查清单-checklist)

---

## 一、 项目背景与本手册使命

FoloToy AI Passport 是一款基于 ESP32-C3 芯片、搭载 1.54 寸彩色 LCD 屏幕、物理按键、无源蜂鸣器以及内置锂电池的便携式微型极客终端。

在过往的实际开发过程中，AI Agent 曾因**“未读官方源码即写代码”、“自作主张臆想协议”、“测试不充分宣称完成”、“破坏字库导致乱码”**等问题遭遇严重挫折并引发用户多次严厉指正。

本手册旨在真实、完整地复盘开发过程中的全部关键踩坑节点，提炼出不可动摇的**“红线禁令”**与**“标准化作业流程”**。任何新接手的 Agent，必须在动手前全文通读本手册。

---

## 二、 八大绝对禁令与核心红线（Strict Prohibitions）

> [!CAUTION]
> 以下八条为绝对禁令，无论在任何场景下，**违反任何一条即视为重大开发事故**。

### 禁令 1：严禁全盘擦除 Flash 与破坏 `0x356000` 出厂硬件身份分区！
* **背景**：AI Passport 具备官方身份识别机制，出厂硬件证书与卡片唯一标识存放在 `0x356000` 分区（`cardid`，大小 8KB）。如果该分区丢失或被损坏，设备将永久丧失官方联网和认证能力，变成“不可逆砖头”。
* **严禁操作**：
  * **严禁** 执行 `esptool erase_flash`！
  * **严禁** 将应用程序固件（`app.bin`）的偏移地址设置到破坏 `0x356000` 的范围！
* **强制要求**：
  * 在对任何新物理设备进行首次烧录前，**必须先备份出厂证书**：
    ```bash
    python3 -m esptool --chip esp32c3 --port <PORT> read_flash 0x356000 0x2000 backup_cardid.bin
    ```
  * 固件烧录必须严格按各分区偏移地址分别写入：
    ```bash
    0x0 build/bootloader/bootloader.bin \
    0x8000 build/partition_table/partition-table.bin \
    0x10000 build/FoloToy-AI-Passport.bin
    ```
  * `backup_cardid.bin` 包含设备专属私钥凭据，**严禁提交入 Git 仓库**（必须写入 `.gitignore`）。

### 禁令 2：严禁“未经阅读官方源码即写代码”与“未经审计即宣称已完成”！
* **用户的黄金训诫**：
  > **“你在开发前，要阅读官方源代码；在开发后要审计，也要阅读源代码，才能知道功能在上线前是不是正常。”**
* **反面教材**：此前 Agent 曾凭空设想 UI 结构，甚至在代码编译不通过或根本没有实现某项功能时，即向用户汇报“已全部实现”，在用户实机验证后被当场戳穿。
* **强制要求**：
  1. 开发任何功能前，**必须**使用 `view_file` 或 `grep_search` 阅读官方已有的实现文件（如 `main/main.c`、`components/`、官方 demo 等），确认其头文件依赖、宏定义和调用范式。
  2. 代码写完后，**必须逐行进行二次代码审计 (Self-Audit)**，比对实际代码与用户需求是否 100% 吻合。

### 禁令 3：严禁随意更改语言体系或制造“豆腐块（Tofu）”汉字乱码！
* **反面教材**：
  * 第一次报错中文乱码时，Agent 竟自作主张直接将所有界面文字全部改成英文，引发用户严厉批评（“完全没有中文了这次，你开发前不看官方源代码吗？？？？”）。
  * 随意使用未经字库索引的生僻汉字，导致 LCD 屏幕满屏显示方块 `□`。
* **强制要求**：
  * 深入官方源码发现：官方在 `main/lv_font_cn_16.c` 中内置了经过字模压缩的 **3755 常用汉字 Flash 字库**（`lv_font_cn_16`）。
  * 所有 UI 中文标签（`lv_label`）必须统一声明字体：
    ```c
    extern const lv_font_t lv_font_cn_16;
    lv_obj_set_style_text_font(label, &lv_font_cn_16, 0);
    ```
  * 使用的中文词汇必须落在 3755 常用汉字范围内；必要时加入 `safe_text_filter` 处理动态文本，严禁显示英文或直接退化！

### 禁令 4：严禁凭空臆造网络与蓝牙判定常量（必须有官方协议背书）！
* **反面教材**：随手定义一个 `0x1234` 宣称是某品牌追踪器，被用户质问：“这些设备的判定方式是有官方支持的吗？还是你随意设置的？”
* **强制要求**：
  * 涉及所有硬件协议、无线侦测、厂商识别等功能，必须严格对应国际标准：
    * **Bluetooth SIG Assigned Numbers**（公司分配 ID 与 16-bit Service UUID）；
    * **Apple / Google IETF DULT** (Detecting Unwanted Location Trackers) 官方规范；
    * **IEEE 官方 MAC OUI 数据库**（安防摄像头芯片硬件地址分配）；
    * 严禁臆造，必须在文档和代码注释中给出规范出处！

### 禁令 5：严禁对网络排查能力做虚假宣传（必须实事求是阐明物理边界）！
* **反面教材**：宣称“单机无需联网即可破解酒店 Wi-Fi 抓取局域网内所有手机传输的实时视频流”。
* **现实物理边界**：
  * ESP32-C3 是单频段 2.4GHz 芯片，在未获得酒店 Wi-Fi 密码的前提下，无法解密 WPA2/WPA3 链路层加密报文。
  * 实事求是的技术路径是：**空口特征扫描 + 隐藏 SSID 抓取 + 常见摄像机出厂热点指纹匹配 + IEEE OUI 芯片模组比对 + 贴身超强信号告警**。

### 禁令 6：严禁未经讨论与用户确认，擅自进行系统级关键设计变更！
* **反面教材**：用户提出省电需求，Agent 自行拍脑袋定义熄屏逻辑。
* **用户指令**：
  > **“查看源码后与我讨论方案，我确认后再操作。”**
* **强制要求**：
  * 涉及电源管理、按键重定义、存储架构改动等重大方案，必须先输出清晰的方案对比矩阵与利弊分析，等待用户回复确认后再进入代码实现阶段。

### 禁令 7：严禁熄屏按键直接透传至业务层（必须具备“口袋防误触”设计）！
* **背景**：设备放在口袋或背包中，容易产生挤压。如果熄屏状态下任何按键直接透传给底层界面，会导致用户无意识中误清空白名单、误切换功能甚至误格式化。
* **强制要求**：
  * 在熄屏待机状态下，**第一下按键仅负责唤醒屏幕，事件必须被电源管理器消费（拦截），绝不透传给底层业务**。只有在屏幕处于亮屏状态时，按键才可正常响应业务逻辑。

### 禁令 8：严禁在公开仓库或提交记录中泄露用户敏感凭证！
* **背景**：发布流程涉及用户的 GitHub Personal Access Token (PAT)、FoloToy 平台兑换码（One-time code）以及硬件出厂 bin 文件。
* **强制要求**：
  * 推送完毕后，必须使用 `git remote set-url` 抹除 remote URL 中的 Token；
  * 严禁将含有 Token 的命令明文写入 `README.md` 或项目文件；
  * `backup_cardid.bin` 必须加入 `.gitignore`。

---

## 三、 硬件环境、分区表与内存架构深度解析

### 1. 硬件核心技术参数
* **MCU**：ESP32-C3-MINI-1（单核 RISC-V 32 位处理器，主频最高 160MHz）。
* **存储资源**：
  * **SRAM**：仅 400KB 内置 SRAM（无外部 PSRAM！可用内存极其紧张，严禁在堆上随意分配大内存）。
  * **Flash**：8MB SPI Flash（支持 XIP 指令与只读常量直接存取）。
* **外设**：
  * 屏幕：1.54 寸 SPI 彩屏，分辨率 240×240，驱动 IC 为 ST7789。
  * 按键：3 向按键（`UP`, `DOWN`, `ENTER/OK`），支持短按、长按事件。
  * 音频：单 GPIO 驱动的无源蜂鸣器（LEDC PWM 产生方波）。
  * 无线：2.4GHz Wi-Fi (802.11 b/g/n) + BLE 5.0 (NimBLE 协议栈)。

### 2. 分区表结构 (`partitions.csv`)

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x300000,
cardid,   data, 0x40,    0x356000,0x2000,
storage,  data, spiffs,  0x358000,0x4A8000,
```

#### 关键安全界限分析：
1. **`factory` (0x10000, 3MB)**：当前固件体积通常在 1.4MB~1.8MB 之间，占用至 `~0x1f0000` 附近。
2. **`cardid` (0x356000, 8KB)**：出厂唯一设备证书分区，距离 `factory` 结尾有大于 **1.4MB** 的缓冲空间。**无论如何更新代码，固件大小绝不可超过 3MB，且切勿破坏 0x356000**。

---

## 四、 官方框架与开发规范（开发前必读）

### 4.1 中文字库渲染与“豆腐块乱码”根治范式

1. **官方字库真相**：
   官方工程中已经生成并集成了 `main/lv_font_cn_16.c`，内置 3755 个常用汉字。它使用 `LV_FONT_DECLARE(lv_font_cn_16)` 即可引入，数据存储在 Flash 只读区，不消耗运行时 SRAM。
2. **正确的使用范式**：
   ```c
   #include "lvgl.h"
   LV_FONT_DECLARE(lv_font_cn_16);

   lv_obj_t *label = lv_label_create(parent);
   lv_obj_set_style_text_font(label, &lv_font_cn_16, 0);
   lv_label_set_text(label, "随身安全哨兵");
   ```
3. **安全文字过滤器 (`safe_text_filter`)**：
   如果展示动态扫描到的网络名称（SSID）或蓝牙广播名称，其中可能包含 Emoji 或超出 3755 汉字范围的生僻字。若直接丢给 LVGL，未收录字符会渲染为方块豆腐块 `□`。
   **标准实践**：在 UI 渲染前，对未知字符串进行 ASCII / 常用字过滤，非法字符替换为 `.` 或跳过。

### 4.2 按键交互解耦与“长按顶层拦截”原则

根据官方 `docs/coding-conventions.zh_CN.md` 规范：
* **短按 (`BUTTON_CLICK`)**：交由各子功能页面（如雷达切换目标、白名单添加、偷拍重新扫描）。
* **长按确定键 (`BUTTON_LONG_PRESS`)**：**统一由 `main.c` 顶层统一拦截**！
  * 当用户长按确定键时，顶层状态机强制退出当前运行的 App，清理其所有定时器与扫描任务，返回系统主菜单；
  * **禁止**在子页面中私自截断长按确定键而不做全局退出处理。

### 4.3 列表与页面滚动视图规范
* **问题点**：当探测到的信标或 Wi-Fi 热点超过屏幕高度时，初版代码常因未设置滚动而导致内容被截断。
* **正确做法**：
  ```c
  lv_obj_set_scroll_dir(list_container, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list_container, LV_SCROLLBAR_MODE_AUTO);
  // 在物理按键 UP/DOWN 时驱动滚动聚焦：
  lv_obj_scroll_to_view(focused_item, LV_ANIM_ON);
  ```

### 4.4 蓝牙 (NimBLE) 与 Wi-Fi 内存平衡
* ESP32-C3 SRAM 极其宝贵。官方推荐使用轻量级 **NimBLE**（而非臃肿的 Bluedroid）。
* **共存注意事项**：当需要执行 Wi-Fi 扫描（如偷拍排查）时，应暂停后台 BLE 扫描或限制 BLE 吞吐，避免 Wi-Fi 与 BLE 射频资源争抢导致内核 OOM 崩溃。

---

## 五、 四大核心功能实现与权威参考标准

### 5.1 防追踪雷达与 IETF DULT / Bluetooth SIG 规范

```
┌─────────────────────────────────────────────────────────────┐
│                    BLE 广播包解析与鉴权                       │
└──────────────────────────────┬──────────────────────────────┘
                               │
            ┌──────────────────┴──────────────────┐
            ▼                                     ▼
   【Manufacturer Data (0xFF)】          【Service UUID (0x03/0x16)】
            │                                     │
   ├─ 0x004C (Apple, Inc.)               └─ 0xFEED (Tile Inc. UUID)
   │   ├─ 0x12 (Find My 离线寻找, 25B payload)  => 判定为: Tile 寻物瓷贴
   │   ├─ 0x10 (Nearby Continuity 邻近协同)
   │   └─ 0x07 (AirPods Proximity 弹窗配对)
   ├─ 0x0075 (Samsung Electronics)
   │   └─ SmartTag / SmartTag2 专有载荷
   └─ 0x027D (Huawei Device Co., Ltd.)
       └─ Huawei Tag 寻物信标
```

#### 伴随判定状态机（时间窗口算法）：
* 蓝牙信标可能来自地铁上擦肩而过的路人，绝不能一出现就报警。
* **模型参数**：
  * **SAFE（安全）**：未发现信标，或出现时长 $< 3$ 分钟；
  * **NOTICE（提示）**：近场（$\text{RSSI} \ge -75\text{dBm}$）首次捕获；
  * **ALERT（高危警报）**：陌生信标在持续 **5 分钟** 的滑动时间窗口内被连续捕获（次数 $> N$ 且平滑 RSSI 稳定），判定为恶意跟踪，点亮屏幕并声光告警。

### 5.2 偷拍排查物理边界与四维立体判决模型

单机离线状态下，通过 2.4GHz 全频段 14 信道的主动扫描，构建四维检测启发式算法：

| 判定维度 | 触发条件 | 威胁等级 | 工程与现实依据 |
| :--- | :--- | :--- | :--- |
| **隐藏 SSID** | `ssid_len == 0` 或首字节为 `\0`，且 $\text{RSSI} \ge -70\text{dBm}$ | 极高可疑 | 针孔摄像头多关闭广播躲避普通手机扫描；强信号表明发射源就在室内 |
| **摄像机命名前缀** | 匹配 `LOOKCAM`, `V380`, `IWFCAM`, `HDWIFICAM`, `MINICAM`, `SPY` 等 21 种前缀 | 高可疑 | 绝大多数黑产改装摄像头均采用公模方案，出厂 AP 热点具备固定特征 |
| **安防芯片 OUI** | MAC 前 3 字节匹配涂鸦、雄迈、海康、大华、乐鑫 IoT 等指纹库 | 中/高可疑 | 提取 IEEE 官方厂商注册号，直接甄别发射模组物理硬件身份 |
| **近场超强信号** | $\text{RSSI} \ge -45\text{dBm}$ | 提示排查 | 距离探测器仅 1~2 米范围内的强辐射源，提示物理近距离翻找排查 |

### 5.3 智能熄屏电源管理与口袋防误触状态机

```
[系统启动/任意交互] ──> 【亮屏 100% (SCREEN_ON)】
                             │
                      无操作 35 秒
                             ▼
                        【微暗 20% (SCREEN_DIM)】
                             │
                      无操作 45 秒
                             ▼
                        【熄屏 0% (SCREEN_OFF)】
                             │
       ┌─────────────────────┴─────────────────────┐
       ▼                                           ▼
【口袋首按唤醒】                              【后台 ALERT 报警】
仅拦截唤醒，绝不透传业务                        瞬间唤醒点亮并声光报警
```

* **常亮抑制矩阵**：
  * 冷热寻物翻找期间：强制全程常亮；
  * Wi-Fi 全信道主动扫描期间：强制常亮；
  * 高危报警激活期间：强制常亮。

### 5.4 盖革冷热寻物音频变频发生器
* **设计意图**：当怀疑身上有 AirTag 等追踪器时，用户无法一边翻找背包一边死盯屏幕。
* **实现原理**：
  * 将实时 RSSI（$-95\text{dBm} \sim -35\text{dBm}$）线性映射为 $0\% \sim 100\%$ 接近度；
  * 根据接近度驱动蜂鸣器以变频盖革脉冲（$1000\text{Hz} \sim 3000\text{Hz}$）发声：距离越近，滴答声越急促、音调越高，实现全盲手持探物。

---

## 六、 标准化研发流程与质量把控

新 Agent 接手任何新任务时，必须严格贯彻以下 **六步研发闭环流程**：

1. **需求与源码研读**：阅读官方已有代码与规范，确认硬件边界与接口；
2. **方案沟通与确认**：向用户提交设计矩阵与权威依据，确认后再动手；
3. **驱动与算法引擎开发**：编写纯 C 逻辑模块，实现解耦与状态机设计；
4. **本地 Host 单元测试**：在 PC/Mac 上用 gcc 构建并秒级回归验证；
5. **代码与规范审计**：逐行检查按键长按退出、字库、内存泄漏与边界；
6. **安全烧录与实机验证**：绝不擦除全盘，保留 cardid，验证无误后交付。

---

## 七、 本地仿真、Host 单元测试与安全烧录规范

### 1. 本地 Host 单元测试规范（免硬件极速验证）
```bash
# 1. 验证追踪引擎与 Wi-Fi 偷排查判定
gcc -Wall -Wextra -I. -Imain tests/test_tracker_engine.c main/tracker_engine.c main/tracker_wifi_spy.c -o test_engine && ./test_engine && rm -f test_engine

# 2. 验证智能熄屏、口袋防误触与常亮抑制状态机
gcc -Wall -Wextra -I. -Imain tests/test_tracker_power.c main/tracker_power.c -o test_power && ./test_power && rm -f test_power
```

### 2. 固件编译规范 (ESP-IDF v5.5.3)
```bash
. ~/esp/esp-idf-v5.5.3/export.sh
idf.py build
```

### 3. 安全物理烧录规范（严守禁令 1）
```bash
# 第一步：查找设备端口
ls /dev/cu.usbmodem* /dev/ttyACM* 2>/dev/null

# 第二步：安全烧录（绝对不含 erase_flash，分别写入指定 offset）
python3 -m esptool --chip esp32c3 --port /dev/cu.usbmodem101 -b 460800 \
  --before default_reset --after hard_reset write_flash \
  --flash-mode dio --flash-size 8MB --flash-freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/FoloToy-AI-Passport.bin
```

---

## 八、 多渠道发布全流程与凭据安全

### 8.1 FoloToy 官方社区发布规范
* **发布脚本路径**：`~/.gemini/config/skills/folotoy-ai-passport-publisher/scripts/publisher.py`
* **关键参数与规范**：
  1. 环境变量：`FOLOTOY_AI_PASSPORT_URL=https://ai-passport.folotoy.cn`；
  2. 必须具备 **3:4 竖版封面**（建议 AI 生图，标注示意图，绝不虚构真机实拍）；
  3. 文案要求：中英文双语，聚焦“它是什么、怎么玩、哪里有趣”，不在面向普通用户的简介中堆砌芯片寄存器参数；
  4. 准确汇报审核状态为 `pending`。

### 8.2 GitHub 开源协同与 Fine-grained PAT 权限避坑
1. **权限必须开启项**：
   - `Contents`: **Read and write**（写入代码、创建分支）；
   - `Workflows`: **Read and write**（**关键避坑**：官方仓库含有 `.github/workflows`，PAT 若无 Workflows 权限将导致 `git push` 被拒！）。
2. **凭据擦除机制**：
   推送完成后，立即将 remote URL 恢复为公共 HTTPS 地址，严防 PAT 滞留本地 `.git/config`。

---

## 九、 新 Agent 接手自查清单 (Checklist)

- [ ] **是否通读了官方相关源码？** 没有凭空假设 API 或篡改官方架构。
- [ ] **出厂证书 `0x356000` 是否安全？** 严禁执行 `erase_flash`，已备份 `backup_cardid.bin` 且已加入 `.gitignore`。
- [ ] **文字是否支持中文？** 是否使用了 `&lv_font_cn_16`？是否存在生僻字导致的方块豆腐块乱码？
- [ ] **长按确定键是否由 `main.c` 顶层退出？** 离开子应用时是否销毁了定时器、释放了任务？
- [ ] **UI 列表是否有垂直滚动？** 超过屏幕展示区域时，物理按键能否驱动视口滚动到底？
- [ ] **无线判定是否有标准出处？** Bluetooth SIG 公司 ID、UUID、IEEE OUI 数据库是否标注准确？
- [ ] **熄屏策略是否符合场景？** 翻找和扫描期间是否常亮？熄屏状态下首按是否成功拦截防误触？
- [ ] **是否通过了本地 Host 单元测试？** `test_runner` 是否 100% Pass？
- [ ] **敏感凭据是否已清理？** Git remote、文档中是否已清除 PAT、兑换码和私钥？

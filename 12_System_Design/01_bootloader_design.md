# 系统设计：引导加载程序架构

## 面试问题

> “为安全关键型嵌入式设备设计引导加载程序。
> 它必须支持OTA更新、加密验证和
> 失败时回滚。请说明你的设计思路。”

这是一道白板系统设计题。没有单一的正确答案——面试官想了解你的思维过程。


---

## 第 1 步：提出澄清问题

在画任何东西之前，先问：

- 使用哪款 MCU？ （STM32、iMX8、裸机 或Linux？）
- Flash 闪存如何布局？（内部 Flash 还是外部 SPI Flash？）
- 使用什么更新通道？ （UART、USB、以太网、蜂窝网络？）
- 安全标准是什么？ （IEC 61508、ISO 26262，没有？）
- 需要哪种校验或签名验证？ （SHA256 哈希？RSA 签名？ECDSA？）
- 固件可以有多大？ （决定分区分配）
- 需要断电安全吗？ （单 Bank 与双 Bank Flash）

---

## 步骤2：Flash闪存布局

```
┌────────────────────────────────────────────────────┐  0x0800_0000 (STM32 example)
│                   BOOTLOADER                        │  64 KB — NEVER updated via OTA
│  (read-protected, physically separate from app)     │
├────────────────────────────────────────────────────┤  0x0801_0000
│              Boot Descriptor Block (BDB)            │  4 KB
│  magic, active_slot (0/1), attempt_count, flags    │
├────────────────────────────────────────────────────┤  0x0801_1000
│                  APPLICATION SLOT A                 │  192 KB
│  [Header: version, size, CRC, signature, date]     │
│  [Firmware binary]                                  │
├────────────────────────────────────────────────────┤  0x0804_1000
│                  APPLICATION SLOT B                 │  192 KB
│  (same structure as Slot A)                         │
├────────────────────────────────────────────────────┤  0x0807_1000
│              Persistent Data / NVM                  │  Remaining flash
│  (device config, calibration — never touched by BL)│
└────────────────────────────────────────────────────┘
```

**为什么有两个插槽？**
- 插槽 A = 当前运行的固件
- 插槽 B = 正在下载新固件
- 下载完成→验证B→设置active_slot=B→重置
- 如果 B 启动失败 N 次：恢复到 A
- 这称为 A/B（双库）或乒乓更新

**替代方案：带暂存区的单库**
- 保存Flash但不安全：如果覆盖期间断电，就会变砖
- 仅适用于非关键设备

---

## 第 3 步：引导加载程序引导流程

```
RESET
  │
  ▼
[1] Hardware Init
    - Clock, watchdog, minimal GPIO
    - Do NOT init peripherals not needed for boot

  │
  ▼
[2] Read Boot Descriptor Block (BDB)
    - Validate magic number
    - If BDB corrupt → use factory defaults (Slot A)

  │
  ▼
[3] Integrity Check on Active Slot
    - Compute SHA-256 over [header + firmware]
    - Compare against header.sha256
    - If mismatch → mark slot bad → try other slot

  │
  ▼
[4] Signature Verification (if required)
    - Verify RSA-2048 / ECDSA-P256 signature over firmware hash
    - Public key stored in bootloader flash (write-protected)
    - If signature invalid → reject firmware, do NOT boot

  │
  ▼
[5] Version Downgrade Check
    - Check firmware version >= minimum_allowed_version
    - Prevents rollback attack (installing old vulnerable firmware)
    - minimum_allowed_version stored in OTP (one-time programmable) fuses

  │
  ▼
[6] Jump to Application
    - Set stack pointer: __set_MSP(app_header->stack_top)
    - Jump to reset handler: app_reset_handler()
    - On Cortex-M: must disable interrupts, set VTOR, jump

  │
  ▼
[7] Watchdog started in bootloader — app must pet it
    - If app never starts petting: watchdog resets
    - Bootloader increments attempt_count
    - After N failures: roll back to previous slot
```

---

## 步骤4：OTA更新流程

```
App receives update command (MQTT, UART, etc.)
  │
  ▼
[1] Open update channel, receive firmware binary + metadata
    (version, size, SHA-256, signature)

  │
  ▼
[2] Write to INACTIVE slot in chunks
    - Erase sector before writing (flash write rules)
    - Write 256-byte pages
    - Compute running CRC/hash as you go

  │
  ▼
[3] After complete download:
    - Verify running hash == expected hash
    - Write header to inactive slot

  │
  ▼
[4] Signal bootloader: "install this slot"
    - Update BDB: pending_slot = B, attempt_count = 0
    - Set UPDATE_PENDING flag in BDB

  │
  ▼
[5] Reset → Bootloader picks up UPDATE_PENDING
    - Verifies Slot B integrity + signature
    - If OK: active_slot = B
    - If not OK: clear flag, keep active_slot = A

  │
  ▼
[6] App boots from new slot
    - On first successful boot: set BDB.confirmed = 1
    - If not confirmed within N boots: rollback
```

---

## 第 5 步：安全考虑

| 威胁 | 缓解措施 |
|--------|-----------|
| 未签名固件 | 加密签名验证 (ECDSA-P256) |
| 重放攻击（旧固件） | 版本单调性+OTP保险丝 |
| 物理Flash已读 | STM32 上的 RDP（读出保护）2 级 |
| 引导加载程序修改 | 带写保护的独立Flash组 |
| 降级攻击 | OTP 中的最低版本、防回滚计数器 |
| 供应链攻击 | 安全启动链、认证证书 |

---

## 第 6 步：看门狗策略

```c
/* Bootloader starts watchdog with 5-second window */
IWDG->KR  = 0xCCCC;  /* start */
IWDG->KR  = 0x5555;  /* unlock */
IWDG->PR  = 0x04;    /* prescaler /64 */
IWDG->RLR = 1953;    /* reload: 1953 * 64 / 32000Hz ≈ 3.9 s */
IWDG->KR  = 0xAAAA;  /* reload */

/* Bootloader kicks WDT during hash computation (can take >1s for 1MB) */

/* On jump to app: watchdog is STILL RUNNING */
/* App must call: IWDG->KR = 0xAAAA; within 3.9 seconds */
/* If app hangs: watchdog fires, bootloader increments fail count */
```

---

## 常见面试追问s

**问：如果Flash擦除在更新过程中失败怎么办？**
> 仅在验证完整映像后才会写入非活动插槽报头。
> 部分擦除/写入会留下无效的插槽报头→引导加载程序会拒绝它。

**问：如何在 128 KB RAM 的设备上处理 1 MB 固件？**
> 增量式流式传输和计算 SHA-256（滑动窗口哈希）。
> 切勿在 RAM 中缓冲整个图像。

**问：为什么 ECDSA-P256 比 RSA-2048 更适合嵌入式？**
> 更小的密钥大小（256 位与 2048 位）、更快的验证、更小的代码。
> RSA 签名验证需要约 2 KB RAM 的密钥； ECDSA < 500 字节。

**问：如果有人在插槽交换期间断电会发生什么？**
> 采用A/B设计：没有任何损失。 BDB 更新 (active_slot = B) 是
> 单 4 字节写入 — 大多数 Flash 控制器上的 atomic。
> 在确认 B 工作正常之前，插槽 A 永远不会被擦除。

---

## 面试清单

在说"I'm done designing"之前：

- [ ] Flash 闪存布局 已定义（引导加载程序、BDB、插槽 A、插槽 B、NVM）
- [ ] 完整性检查（哈希）+身份验证（签名）
- [ ] 回滚机制（尝试计数器、确认）
- [ ] Watchdog integration
- [ ] 防回滚（版本检查+OTP）
- [ ] 读保护/安全启动
- [ ] 断电安全（A/B或atomic更新）
- [ ] 更新定义的通道和协议

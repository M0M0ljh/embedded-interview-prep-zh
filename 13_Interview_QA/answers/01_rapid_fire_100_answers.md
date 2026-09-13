# 答案：100 道嵌入式快速问答

---

## A 节——C 语言

**1. `volatile`有什么作用？什么时候必须使用它？** 告诉编译器：“每次读/写都必须真正访问目标——寄存器中没有缓存，没有重新排序。”将其用于：(a) 与 ISR 共享的变量，(b) 内存映射 ​​硬件寄存器，(c) 被另一个 CPU 内核或 DMA 更改的变量。


**2.一个变量可以同时是`const`和`volatile`吗？** 可以。 `const volatile uint32_t *STATUS_REG = (uint32_t*)0x40020000;` `const` = 软件不得写入。 `volatile` = 硬件自动更改它。经典示例：只读状态寄存器。



**3. `static` 文件作用域与函数作用域。** 文件作用域：限制此翻译单元的可见性 — 嵌入的“私有”等价物。函数作用域：变量在函数调用之间持续存在（存储在`.data`/`.bss`中，而不是栈）。用于去抖计数器、状态机。


**4.为什么`int`危险？使用什么？** `int` 的大小由具体实现决定（16 或 32 位取决于 平台/编译器）。使用 `<stdint.h>` 类型：`uint8_t`、`uint16_t`、`uint32_t`、`int32_t`。切勿在协议代码或硬件寄存器访问中使用`int`。


**5. 3 个未定义行为的示例 (UB)。** (a) 有符号整数溢出：`INT_MAX + 1`。 (b) 移位量等于或超过类型位宽：`uint32_t x; x << 32`。 (c) 解引用 NULL 或释放指针。另外：读取未初始化的变量、越界数组访问。


**6. 结构填充与紧凑结构体。**编译器插入填充字节以对齐成员（例如，`uint32_t`对齐到4个字节）。 `__attribute__((packed))` 删除填充 — 对于协议帧很有用，但会导致未对齐的访问，从而在严格对齐的 CPU 上崩溃或导致 ARM 上的读取速度缓慢。


**7. `sizeof` 代表 `struct { char c; int x; }`？** 不是 5 — 它是 8（在 32 位上）。 `char` 在偏移量 0 处，3 个字节填充，`int` 在偏移量 4 处。使用 `__attribute__((packed))` 表示 5 个字节，接受未对齐的性能损失。


**8.嵌入式中的联合用例。**类型双关：以不同类型访问相同的字节。

```c
union { uint32_t raw; float f; } u;
u.raw = 0x3F800000;
// u.f == 1.0f
```
另外：覆盖寄存器，将协议字节反序列化为struct。

**9. for 循环中的 `++i` 与 `i++`。** 在 `for (int i = 0; i < N; i++)` 和 `for (int i = 0; i < N; ++i)` — 生成的代码相同。仅当使用表达式的值时，差异才重要：`a = i++`（a 获取旧值）与 `a = ++i`（a 获取新值）。


**10.带联合的类型双关 — 在 C 中安全？** 是的，在 C11 (TC3) 中，定义了读取与上次写入不同的 union 成员。在 C++ 中，它是 UB — 对于 C++，请使用 `memcpy`。


**11. `restrict`关键字。**告诉编译器：“此指针是此范围内此内存的唯一别名。”实现更好的优化（无需别名分析）。常见于`memcpy`类函数。 `void my_memcpy(uint8_t * restrict dst, const uint8_t * restrict src, size_t n)`。


**12.为什么在嵌入式生产中避免`malloc`？** 时间不确定，长期运行会产生内存碎片，无法从故障中恢复，编译时堆大小未知，被MISRA/IEC 61508禁止。使用static分配：固定大小的缓冲区，内存池。


**13. 检测裸机上的内存泄漏。** 跟踪池分配计数 (alloc_count - free_count)。栈填充法（0xA5 填充值 + 水位检查）。 FreeRTOS (`xPortGetFreeHeapSize`) 中的堆检查函数。对于 Linux 嵌入：Valgrind、Valgrind massif、`/proc/self/status` VmRSS。


**14. 栈溢出检测。** FreeRTOS：栈填充法+`uxTaskGetStackHighWaterMark()`。裸机：在栈边界设置金丝雀值（0xA5A5A5A5），在运行时检查。 MPU：配置栈保护区（溢出时生成故障）。 Cortex-M：MSPLIM 寄存器（M33/M4 与 TrustZone）。


**15. 循环（环形）缓冲区。** Head = 写入索引（生产者），tail = 读取索引（消费者）。对于掩码技巧，大小必须是 2 的幂。满：`(head+1) & MASK == tail`。空：`head == tail`。 SPSC（单生产者、单消费者）场景下可实现无锁。


**16. 字节序。 ARM Cortex-M 默认。** 小端序 默认（在某些变体中可以配置为 BE8，但实际上所有现实世界的 ARM 嵌入式都是 LE）。网络字节序是大端序。使用`htons()`/`ntohl()`或手动`(buf[0]<<8)|buf[1]`进行协议解析。


**17. 位带。** 将 SRAM/外设 中的每个位映射到 32MB 别名区域中其自己的 32 位字。启用 原子置位/清零，无需 读-改-写（避免与 ISR 之间的竞态条件）。适用于 Cortex-M3/M4. 不适用于 M0/M0+。


**18. `uint8_t *p` 与 `const uint8_t *p`.** `uint8_t *p`：指向可变uint8_t 的指针。 `const uint8_t *p`：指向只读uint8_t的指针（无法写入`*p`）。对于const指针：`uint8_t * const p`。对于const：`const uint8_t * const p`。


**19. `__attribute__((packed))` 风险。** 删除 struct 填充。风险：未对齐的访问 — 在 Cortex-M0/M0+ 上，这会导致 HardFault。在 M3/M4 上它可以工作，但速度较慢（逐字节加载）。通过未对齐的指针解引用指向打包成员的指针是不安全的。


**20. `memcpy` 与 `memmove`.** `memcpy`：源和目标不得重叠（UB 如果重叠）。 `memmove`：正确处理重叠区域（使用中间缓冲区或反向复制）。当 移位 数据位于缓冲区内时，使用`memmove`。


---

## B 部分 — 中断和 ISR

**21. ISR.** 中永远不要做的 5 件事 (1) `printf`（使用堆、锁、阻塞）。 (2)`malloc`/`free`。 (3) `delay_ms()` 或任何阻塞等待。 (4) 锁定互斥锁（xSemaphoreTake — 可以阻塞）。 (5) 不保存FPU上下文的浮点。


**22. 虚假中断。** 在没有明显源的情况下触发的中断（清除标志，无待处理源）。原因：电气噪声，清除标志时竞态条件。防御性处理：检查ISR中的标志，如果没有找到源则忽略，不要无限循环。


**23. `volatile` 在 ISR 到主通信中修复了什么。** 如果没有 volatile，编译器可能会在循环迭代中将标志缓存在寄存器中，并且永远不会从内存中重新读取。主循环永远看到陈旧的值。 `volatile` ​​强制每次读取都进入内存。


**24. 在 Cortex-M 中断入口处自动保存寄存器。** 硬件将 8 个寄存器压入栈：R0、R1、R2、R3、R12、LR、PC、xPSR。如果ISR使用它们，则必须手动保存/恢复 R4–R11（被调用者保存）（编译器通过EXC_RETURN机制自动为ISR函数执行此操作）。


**25. 尾链。** 当 CPU 正在处理另一个较低优先级的 IRQ 时发生中断时，它会直接链接，而无需 出栈/重新入栈. 节省约 12 个周期。当多个 IRQ 待处理时有效。


**26. 中断中的抢占。** 较高优先级IRQ 可以中断（抢占）当前正在执行的较低优先级ISR。在 Cortex-M 上：通过 NVIC 优先级进行配置。如果待处理的 IRQ 的优先级组 > 当前活动组，则会抢占。


**27. NMI（不可屏蔽中断）。** 无法通过软件禁用。始终执行。使用案例：时钟故障监视器、看门狗 NMI 模式、电源故障检测。关于STM32：BXCAN 错误可以生成 NMI，可以配置看门狗来触发 NMI。


**28. NVIC。设置中断优先级。** NVIC = 嵌套向量中断控制器（Cortex-M 内核外设）。设置优先级：`NVIC_SetPriority(IRQn, priority)`。启用：`NVIC_EnableIRQ(IRQn)`。较低的数字=较高的优先级。实现的位数各不相同（通常 4 位 = STM32 上的 16 个级别）。


**29. 中断优先级与抢占优先级。** STM32 使用组优先级和子优先级。组优先级决定抢占。子优先级打破相同组优先级 IRQ 之间的联系（没有抢占，只是排序）。通过`NVIC_SetPriorityGrouping()`设置。


**30. 清除ISR中的中断标志。**如果不清除该标志，则IRQ返回后立即再次触发→无限ISR循环→系统挂起。一些外设在读取数据寄存器时清零（UART RXNE 通过读取 DR 清零）。其他需要显式清除（TIM SR UIF 由`SR &= ~TIF_UIF` 清除）。


**31. 惰性 FPU 上下文保存 (Cortex-M4F)。** 默认情况下，FPU 寄存器（S0-S15、FPSCR）不会保存在中断入口时以节省周期。保存被推迟到ISR实际使用FP指令。如果你的 ISR 使用 float：设置 FPCCR.LSPEN=0（立即保存），或者不在 ISR 中使用 float。


**32. 优先级反转。原因。** 高优先级任务 H 阻塞等待低优先级任务 L 持有的资源。中优先级任务 M 抢占 L (M > L)。即使 H > M > L，H 也会挨饿。 解决方案：互斥锁中的优先级继承 — L 暂时提升为 H 的优先级。


**33. ISR 信令的二进制信号量与互斥锁。** 二进制信号量：无所有权跟踪，可以从 ISR 给出。互斥锁：跟踪哪个任务拥有它（对于优先级继承）。你不能从 ISR 提供互斥锁（破坏所有权状态）。将 `xSemaphoreGiveFromISR()` 与二进制信号量一起使用。


**34. 临界区 在裸机上。** 禁用中断：`__disable_irq()`，访问共享数据，重新启用：`__enable_irq()`。在具有优先级的 Cortex-M 上：使用 BASEPRI 寄存器屏蔽低于阈值的中断而不阻塞 NMI/HardFault.


**35. `__disable_irq()` 与 BASEPRI。** `__disable_irq()` 设置 PRIMASK — 禁用所有可屏蔽中断。 BASEPRI = N 仅屏蔽优先级 < N 的中断。在 FreeRTOS (`taskENTER_CRITICAL()`) 中首选 BASEPRI，因此 NMI 和 HardFault 仍会响应。


---

## C 部分——内存

**36. ARM Cortex-M 内存布局。**
```
0x00000000: Code (flash) — .text, .rodata
0x20000000: SRAM — .data (initialized), .bss (zeroed),
                   heap (grows up), stack (grows down)
0x40000000: Peripherals (memory-mapped registers)
0xE0000000: System (SysTick, NVIC, SCB, DWT, ITM)
```

**37. .data 和 .bss 初始化。** `.data`：初始化的全局变量（例如，`int x = 5`）。启动时值存储在闪存中，并通过启动代码 (Reset_Handler) 复制到 SRAM。 `.bss`：零初始化的全局变量（例如，`int y;`）。启动代码向该区域写入零。 `.bss` 不存储在闪存中（节省闪存空间 - 只需要起始地址和大小）。


**38. 堆 vs 栈。** 堆：动态内存（malloc/free），从freeRAM底部向上增长。栈：函数调用帧、局部变量、返回地址，从RAM顶部向下增长。他们相互成长——碰撞=无声的腐败。


**39. 栈溢出 在裸机上。** 无声损坏。栈会增长为堆、.bss 或.data。变量被垃圾覆盖。系统可能会正常运行一段时间，然后随机崩溃。除非MPU配置了栈保护区，否则不会自动保护。


**40. 悬空指针。** 引用已释放或超出范围的内存的指针。 `int *p = malloc(4); free(p); *p = 5;` — UB。在裸机上：函数返回局部变量的地址 - 栈帧 消失了，指针指向垃圾。


**41. Use-after-free.** 在`free()`之后访问内存。常见错误：`free(p); if (p->flag) {...}`。该内存可能已被其他分配重用。修复：`free(p); p = NULL;` — 然后NULL解引用是可检测到的。


**42. NULL 与野指针。** NULL：指针值为 0，明确无效。解除引用会导致故障（如果MPU/MMU启用）。野指针：具有随机值的未初始​​化指针——可以指向任何地方，解引用会破坏随机内存。始终初始化指针。


**43. 内存碎片。** 经过多次 alloc/free 循环后，free 内存存在于许多不连续的小洞中。即使总 free 字节 > 请求的大小，大的分配也会失败。嵌入式缓解措施：固定大小的内存池，仅在启动时分配，从不free。


**44. 内存池。** N 个固定大小块的预分配数组。 `alloc()`：O(1)，返回下一个free块。 `free()`：将块标记为可用。无碎片（所有块大小相同）。用于FreeRTOS队列、CAN消息缓冲区、传感器数据缓冲区。


**45. `__attribute__((section(".ccm")))`.** 将变量放置在名为 链接器段 中。链接器脚本将 `.ccm` 映射到 STM32F4 上的内核耦合内存 (CCM SRAM)（0x10000000 处为 64 KB）。 CCM 直接连接到CPU D 总线——最快的数据访问。 DMA 无法访问 CCM。


**46. CCMRAM。你不能做什么。** DMA 无法访问 CCMRAM。只有CPU可以。用于：快速查找表、栈、RTOS 数据。不要放置：DMA 缓冲区、共享外设缓冲区。


**47. 启动代码初始化startup_stm32xxxx.s中的.data和.bss.** Reset_Handler：

1. 将`.data`从闪存（`_sidata`至`_edata`）复制到SRAM（`_sdata`）。
2. 将 `.bss` 从 `_sbss` 归零到 `_ebss`。
3. 致电`SystemInit()`。
4. 致电`main()`。

**48. 链接描述文件角色。** 定义启动代码使用的内存区域（FLASH、RAM 大小和地址）、节（.text、.data、.bss、.stack、.heap）和符号（`_estack`、`_sidata`）。还将代码放置在特定部分（`.ramfunc`、`.ccm`）。


**49. SRAM1 与 SRAM2 的STM32。** SRAM1 (112 KB)：通用，DMA 可访问，速度较慢。 SRAM2 (16 KB)：在某些型号上受奇偶校验保护。两者都位于 0x20000000 区域，但物理上分开 — 对于 MPU 区域配置很重要。


**50. DMA.** 直接内存访问控制器在外设和内存之间移动数据，无需CPU 参与。 CPU 设置源、目标、count，然后启动。 DMA 完成后触发中断。在后台传输数据时释放 CPU 进行计算。


---

## D 部分 — 外设和协议

**51. UART vs SPI vs I2C.** UART：异步，2 线 (TX/RX)，点对点，最多 ~10 Mbit/s. SPI：同步，4+ 线 (SCLK/MOSI/MISO/CS)，全双工，多从机，每个从机有 1 个 CS，最多 100+ Mbit/s. I2C：同步，2 线 (SDA/SCL)，半双工，多主多从机7 位地址，开漏，最高 3.4 Mbit/s.


**52. 波特率。 84 MHz 上 115200 的 BRR。** `BRR = FCLK / (16 × BAUD) = 84,000,000 / (16 × 115200) = 45.57 → round to 45`（误差 ~1%）。


**53. TXE 与 TC 标志。** TXE（TX 空）：DR 为空，准备写入下一个字节。 TC（传输完成）：所有位 移位 都被输出到线路上（移位 寄存器为空）。使用 TC 进行 RS-485 方向控制（在 TC 之前不得置低 DE）。


**54. 4 SPI 模式。** 模式 0 (CPOL=0,CPHA=0)：空闲低电平，采样上升沿。模式1（CPOL=0，CPHA=1）：空闲低电平，采样下降。模式 2 (CPOL=1,CPHA=0)：空闲高电平，采样下降。模式 3 (CPOL=1,CPHA=1)：空闲高电平，采样上升沿。模式 0 最常见。


**55. I2C 中的时钟延长。** 从设备将 SCL 保持为低电平以暂停事务 — 为从设备提供准备数据的时间。主设备必须支持时钟拉伸（SCL 上不支持poll/timeout）。当从设备处理速度较慢时需要（例如，在响应之前读取闪存）。


**56. I2C 总线锁定和恢复。** 从设备开始发送，但在字节中被重置 — 将 SDA 保持为低电平。主设备发送9个时钟脉冲：大多数从设备在看到时钟后会释放SDA。如果不是：assert STOP 条件，则硬件复位。内核：`i2c_recover_bus()`。


**57. I2C ACK/NACK.** 每个字节后，接收器在第 9 个时钟 = ACK 期间将 SDA 拉低。如果接收方释放SDA = NACK。主设备在最后一个读取字节的 STOP 之前发送 NACK，以表示“不再需要数据”。


**58. PWM。占空比公式。** `Duty% = CCRx / (ARR + 1) × 100`。频率 = `FCLK / ((PSC+1) × (ARR+1))`。


**59. 输入捕获模式。** 当输入pin 出现指定边沿时，定时器将 CNT 寄存器的值捕获到 CCRx 中。用于测量脉冲宽度、周期或频率。


**60. ADC ​​过采样。** 平均 N 个连续 ADC 读数，以减少噪声并提高有效分辨率。 4× 过采样 → +1 位分辨率。 16×→+2 位。 STM32 上的硬件过采样：设置 CFGR2 中的 OVSR 和 OSR 位。


**61. DMA 双缓冲.** DMA 在两个缓冲区之间交替：填充缓冲区 B 时，CPU 处理缓冲区 A。当 DMA 完成 A 时，ISR 向 CPU 发出信号， DMA 切换填充 A，而 CPU 处理 B。连续数据，无间隙。


**62. RS-485 与 RS-232。** RS-232：单端、±12V、点对点、最大 ~15m。 RS-485：差分对 (A/B)，±5V，多点最多 32 个节点，100 kbps 时为 1200m。 RS-485 需要方向控制 (DE pin)。


**63. Modbus RTU。 FC 用于保持寄存器。** FC=03 读取保持寄存器。框架：`[ADDR][0x03][START_HI][START_LO][COUNT_HI][COUNT_LO][CRC_LO][CRC_HI]`。


**64. CRC-16/IBM.** 多项式：0x8005（正常）= 0xA001（反射）。初始化：0xFFFF。 Input/output 反映。由Modbus RTU、USB 使用。


**65. CAN 帧格式。 DLC.** DLC = 数据长度代码（4 位），0–8。指定有效负载字节数。经典CAN：最大 8 字节。 CAN FD：最大 64 字节。


**66. CAN 仲裁。** 逐位无损仲裁。所有节点同时传输。显性位 (0) 覆盖隐性位 (1)。当节点尝试发送 1 但在总线上看到 0 时，它会失去仲裁 — 停止传输并重试。较低的 ID 获胜（ID field 中的显性位较多）。


**67. CAN 总线关闭。** 传输错误计数器 (TEC) > 255。节点停止传输（总线关闭）。恢复：等待128×11隐性位，然后重新进入错误活动状态。或应用程序重置。


**68. CAN FD.** 灵活的数据速率CAN。两相帧：经典速度仲裁（最多 1 Mbit/s），数据相位最多 8 Mbit/s. 有效负载最多 64 字节。需要 CAN FD 控制器和收发器。


**69. SOME/IP.** 基于 IP 的可扩展的面向服务的中间件。汽车中间件协议（AUTOSAR）。运行于 UDP/TCP. 用于汽车以太网中的服务发现和 RPC。在高带宽应用中取代CAN。


**70. GPIO ​​推挽与漏极开路。** 推挽：驱动器主动驱动高电平和低电平 — 快速，无需上拉。漏极开路：驱动器只能拉低；高态需要外部上拉电阻。 I2C SDA/SCL 必须是开漏极（允许多个主机/从机 共享总线，而无需短路）。


---

## E 部分 — RTOS

**71. 抢占式与协作式RTOS.** 抢占式：当更高优先级的任务准备就绪时，调度器可以随时中断任何任务。协作式：任务运行到主动让出 CPU。抢占式提供有界延迟；协作式调度更简单，但一个挂起的任务就会停止一切。


**72. 打钩。 `configTICK_RATE_HZ`.** 驱动RTOS 调度器的周期性中断 (SysTick)。 `configTICK_RATE_HZ=1000` → 每个 Tick 1 毫秒。所有 `vTaskDelay()` 和超时值均以刻度为单位。更高的速率=更好的分辨率，但更多的CPU开销。


**73. `vTaskDelay` 与 `vTaskDelayUntil`.** `vTaskDelay(100)`：从现在开始延迟 100 个刻度。 `vTaskDelayUntil(&lastWake, 100)`：延迟到距离上次唤醒 100 个时钟周期。 `vTaskDelayUntil` 适用于精确的周期性任务 - 它补偿处理时间，因此周期是准确的。


**74. 任务栈大小。** 每个任务都有自己的栈。大小以字为单位（每个 4 字节）。包括：局部变量、函数调用深度、中断上下文（64字节）、字符串缓冲区。用`uxTaskGetStackHighWaterMark()`测量。绝不小于`configMINIMAL_STACK_SIZE`。添加 20% m 精量。


**75. `uxTaskGetStackHighWaterMark()` 返回 值。** 返回自任务启动以来剩余的最小字数。较低 = 接近溢出。 返回 为 0 = 发生溢出。在调试版本中定期检查。


**76. FreeRTOS.** 中有 3 种信号量类型 (1) 二进制信号量：0 或 1，用于事件信号。 (2) 计数信号量：0到N，跟踪多个资源。 (3) 互斥锁：二进制+优先级继承，用于资源保护。


**77. 为什么不能提供来自 ISR 的互斥锁？** 互斥锁跟踪 优先级继承 的所有权。 `xSemaphoreGive()` 检查并更新所有者任务的优先级。在ISR中，没有有效的任务上下文——从ISR访问任务控制块会破坏RTOS内部。使用二进制信号量+`xSemaphoreGiveFromISR()`。


**78. 死锁 — 两个任务、两个互斥锁示例。** 任务 A：获取 M1，然后等待 M2。任务B：获取M2，然后等待M1。两者都无法继续。两者都会永远阻塞。预防：始终以相同的全局顺序获取锁。


**79. 活锁与死锁。**死锁：任务被阻塞，永远等待（没有进展）。活锁：任务不会被阻塞——它们继续运行并改变状态，但仍然没有取得总体进展（例如，两个任务在循环中不断互相让步）。


**80. RTOS.** FIFO 缓冲区中的消息队列，具有内置同步功能。 `xQueueSend()` 如果已满则阻塞。 `xQueueReceive()` 如果为空则阻塞。提供数据传输和任务同步。可从 ISR 与 `xQueueSendFromISR()` 一起使用。


**81. `portMAX_DELAY` 风险。** 永远等待。风险：如果事件永远不会到来（bug、死锁），任务将永远陷入困境。在生产中，使用有限超时并处理超时情况（日志错误、重置外设等）。


**82. heap_4.c 与 heap_1.c.** heap_1.c：最简单 — 只分配，从不释放。没有碎片。 heap_4.c：首次适应块合并——支持`free()`，合并相邻的free块。 heap_5.c：heap_4，但跨越多个不连续的内存区域。


---

## F 部分 — Linux 嵌入

**83. sysfs。通过它控制 3 件事。** `/sys` 的虚拟文件系统将内核对象公开为文件。控制：（1）GPIO 方向/电平值（`/sys/class/gpio`），（2）LED 亮度/触发器（`/sys/class/leds`），（3）CPU频率调节器（`/sys/devices/system/cpu`）。


**84. 从 Linux 用户空间切换 GPIO。** 选项 A (sysfs)：`echo 17 > /sys/class/gpio/export; echo out > /sys/class/gpio/gpio17/direction; echo 1 > /sys/class/gpio/gpio17/value`。选项 B (libgpiod)：`gpioset gpiochip0 17=1`。 Libgpiod 是现代的首选方法。


**85. 内核模块 — insert/remove.** `insmod mydriver.ko`：加载到内核中。 `rmmod mydriver`：删除。 `modprobe`：处理依赖关系。 `lsmod`：列出加载的模块。 `dmesg`：检查内核日志中的模块消息。


**86. 设备树。** 硬件描述文件 (DTS/DTB)，告诉Linux内核存在哪些硬件及其连接方式（地址、中断、时钟源、pin配置）。替换特定于板的代码。由引导加载程序加载，在启动时由内核解析。


**87. systemd 服务单元文件。**
```ini
[Unit]
Description=My Sensor App
After=network.target

[Service]
ExecStart=/usr/bin/sensor-app
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```
`systemctl enable myapp.service` — 启动时启动。

**88. 调试内核 Oops.** `dmesg | tail -50` — 找到 oops。查找：PC（程序计数器）、LR（链接寄存器）、调用栈。使用`addr2line -e vmlinux <address>`查找源代码行。或者带有核心转储的`gdb vmlinux`。


**89. strace.** 跟踪正在运行的进程的系统调用。 `strace -p PID` — 附加到正在运行的进程。 `strace -e open,read ./app` — 跟踪特定的系统调用。 `strace -o log.txt ./app` — 保存到文件。有用：查找哪个文件无法打开，哪个套接字调用被阻止。


**90. 字符设备与块设备。** 字符设备（`/dev/ttyS0`、`/dev/i2c-0`）：字节流，无缓冲，按顺序访问。块设备（`/dev/sda`、`/dev/mmcblk0`）：固定大小的块、缓冲、随机访问、支持文件系统。


**91. TCP 在 Linux 中保持活动状态。**
```c
int enable = 1;
setsockopt(fd, SOL_SOCKET,  SO_KEEPALIVE,  &enable, sizeof(enable));
int idle = 60;    /* start after 60s idle */
setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE,  &idle,   sizeof(idle));
int interval = 10;
setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
int count = 3;
setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT,   &count,  sizeof(count));
```

---

## G 部分 — 测试和调试

**92. 嵌入式单元测试。** 单独测试各个功能。工具：Unity（C）、CppUTest、cmocka。使用存根函数模拟硬件依赖项（GPIO、SPI）。在主机 PC 上运行以提高速度。将 HAL 与业务逻辑分开以启用模拟。


**93. MC/DC 覆盖范围。** 修正条件/判定覆盖（MC/DC）。决策中的每个布尔条件必须独立影响结果。 DO-178C（航空电子设备 A 级）要求。示例： `if (A && B)` — 测试 A=T,B=T； A=T，B=F； A=F，B=T。不只是分支真/假覆盖.


**94. HIL（硬件在环）测试设置。** 真实 ECU/firmware 运行，其 I/O 连接到模拟物理设备（发动机、电机、传感器）的仿真模型。测试 PC 通过 CAN/模拟信号注入刺激。用于在没有物理硬件的情况下进行测试、验证时序、注入故障。


**95. 运行时查找栈溢出。** FreeRTOS：`uxTaskGetStackHighWaterMark()`<安全裕度→报警。栈绘制：启动时填充0xA5，向后扫描第一个非0xA5。 MPU：配置栈下方的保护区→溢出时的硬故障→记录PC。


**96. 嵌入式目标上的 Valgrind。** Valgrind 仅在 x86 Linux 上运行。对于嵌入式：在 Valgrind 下的主机上运行纯逻辑代码（需要 HAL 抽象）。对于 ARM Linux 嵌入式（Raspberry Pi、iMX8）：Valgrind 可用并可在本机运行。不适用于裸机 MCU。


**97. ASAN（地址清理程序）。** 编译器工具 (`-fsanitize=address`) 检测：释放后使用、缓冲区溢出、返回后使用、重复释放。增加约 2× 内存开销和约 2× 运行时开销。在 ARM Linux 上受支持。如果没有运行时支持，则不适用于裸机。


**98. 调试 ARM Cortex-M 上的硬故障。**
1. 在 HardFault 处理程序中：从 SCB 读取 CFSR、HFSR、MMFAR、BFAR。
2. 栈帧中的 PC（MSP 或 PSP + 第 6 个字）= 错误指令。
3. 常见原因：CFSR.BFARVALID + BFAR = 错误内存地址、CFSR.INVPC = 无效EXC_RETURN、CFSR.UNDEFINSTR = 未定义指令。
4. 使用`addr2line -e firmware.elf <PC_value>`查找源代码行。

**99. JTAG 与 SWD。** JTAG：4 线（TCK、TMS、TDI、TDO）+ TRST 可选。行业标准，支持设备链，可以测试板连接（边界扫描）。 SWD（串行线调试）：2 线（SWDCLK、SWDIO）。 ARM 专用，节省引脚，与单器件 JTAG 具有相同的调试能力。大多数嵌入式板使用 SWD。


**100. 实验室正常、现场崩溃 — 5 个可能的原因。** (1) **Timing/interrupt**：电缆较长、传感器响应较慢、错过中断。使用示波器。 (2) **电源**：负载下的电压暂降、电机的 EMI。检查 VCC 上的示波器。 (3) **温度**：组件不符合规格、晶体漂移、MOSFET 阈值漂移。在极端温度下进行测试。 (4) **栈溢出**：现场运行时数据量更大（完整的日志缓冲区等）。检查水印。 (5) **竞态条件**：不同的流量模式会暴露罕见的并发错误。添加日志记录，提高调试版本的 Tick 频率。


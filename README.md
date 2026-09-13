# 嵌入式面试准备（中文版）

> **从 GPIO 寄存器到 Linux BSP。**
> 源自嵌入式系统面试的真实编程练习、找错题和系统设计题，覆盖从入门到进阶的多个难度。

> **中文版说明：** 本目录是原英文仓库的中文镜像。C 代码、函数名、宏、寄存器名、协议字段和代码字面量保持不变；翻译不代表对原仓库的技术结论进行权威校正，学习时请对照芯片手册和官方文档。

---

## 这个仓库背后的故事

我今年申请了意大利的 **12 家嵌入式公司**。

对于每家公司，我都研究了其技术栈，并据此设计真实的练习题——不是理论，也不是 LeetCode。实际面试练习：

- 从零实现 CRC-16
- 找出某个 ISR 中的 3 个错误
- 使用 OTA 和回滚设计引导加载程序
- 实现 Modbus RTU FC=03 请求生成器

这个仓库就是我整理的所有内容。内容从基础 C 位运算一直延伸到 Linux BSP、AUTOSAR 和完整的系统设计。


---

## 里面有什么

| 文件夹 | 主题 | 文件 |
|--------|-------|-------|
| `01_C_Fundamentals/` | 位运算、指针、结构体、`volatile`/`const`/`static`、字节序 | 5 道题 + 答案 |
| `02_Memory/` | 栈/堆、内存池、链接器段、MMIO | 4 道题 + 答案 |
| `03_Interrupts_and_ISR/` | ISR 设计，环形缓冲区，临界区，DMA 双缓冲 | 4 道题 + 答案 |
| `04_Bare_Metal_Peripherals/` | GPIO 寄存器、UART、SPI/I2C、定时器/PWM | 4 道题 + 答案 |
| `05_RTOS/` | 任务、信号量/互斥锁/队列、优先级反转、死锁 | 2 道题 + 答案 |
| `06_Communication_Protocols/` | UART 帧格式 + CRC、Modbus RTU/TCP、CAN 总线 | 3 道题 + 答案 |
| `07_Linux_Embedded/` | sysfs/GPIO、hwmon、/proc 解析、LED 触发器、网络统计 | 1 道题 + 答案 |
| `08_IoT_Protocols/` | MQTT 数据包生成器、CoAP、JSON、TLS 概念 | 1 道题 + 答案 |
| `12_System_Design/` | 引导加载程序架构、OTA、A/B 闪存、看门狗 | 1 篇深入讲解 |
| `13_Interview_QA/` | 100 道快速问答、20 道找错题和白板题 | 3 个文件 + 答案 |

---

## 如何使用这个仓库

### 方法一：先练习
1. 打开问题文件（例如，`01_C_Fundamentals/01_bit_manipulation.c`）
2. 阅读顶部的理论部分
3. 实现所有 TODO 部分
4. 尝试最后的找错题
5. 检查`answers/`文件夹进行比较

### 方法二：阅读并理解
1. 对照阅读题目和答案
2. 在注释中重点关注“为什么”
3. 关闭文件，凭记忆重新写一遍

### 方法三：面试模拟
1. 设置 30 分钟计时器
2. 打开一个文件，在不查看答案的情况下完成练习
3. 30 分钟后，与答案进行比较，记下差距
4. 每天这样做，持续两周

---

## 文件命名约定

```
XX_Topic/
├── NN_problem_name.c       ← TODO stubs + theory + bug hunt
└── answers/
    └── NN_problem_name_answers.c  ← complete implementations + explanations
```

---

## 构建并运行（主机）

每个 C 文件都在 Linux/Mac/Windows (GCC/Clang) 上编译：

```bash
gcc -Wall -Wextra -o test 01_C_Fundamentals/01_bit_manipulation.c && ./test
```

Python 文件（Linux 嵌入部分）：
```bash
python3 07_Linux_Embedded/some_file.py
```

---

## 涵盖的主题

**C 编程**
- 位运算（置位/清零/翻转、field提取、位计数、反转位）
- 指针算术、函数指针、内存映射寄存器
- 结构填充、打包结构、位域、union 类型双关
- `volatile`、`const`、`static` — 带有 ISR 和硬件寄存器用例
- 字节序检测、字节交换函数、协议解析

**内存**
- 栈与堆、.data 与 .bss、闪存区域
- 固定大小的内存池（O(1) alloc/free，无碎片）
- 栈水位 / 填充法（FreeRTOS 风格）
- 链接器段 属性 (`__attribute__((section(...)))`)

**中断**
- ISR 黄金法则（你不能做的事）
- SPSC 环形缓冲区 (无锁)
- 临界区 (关闭/开启 IRQ)
- DMA双缓冲图案
- 优先级反转，嵌套中断

**外设 (STM32 寄存器级)**
- GPIO：MODER、OTYPER、OSPEEDR、PUPDR、IDR、ODR、BSRR、AFRL/AFRH
- UART：BRR计算，轮询TX，ISR驱动的环形缓冲区TX
- SPI：4种模式（CPOL/CPHA）、全双工传输、CS管理
- I2C：初始化，写事务，组合读事务，ACK/NACK
- 定时器：PSC/ARR计算、PWM模式1、输入捕捉、SysTick

**RTOS (FreeRTOS)**
- 任务创建、优先级、`vTaskDelayUntil`
- 二进制信号量（ISR→任务）、互斥锁（优先级继承）
- 消息队列 (生产者/消费者)
- 死锁，活锁，优先级反转

**通信协议**
- UART 成帧：起始/结束 字节，长度前缀，字节填充
- CRC-16/IBM (Modbus)：逐位和表驱动
- Modbus RTU: FC=03 请求/响应, FC=10 写入多个
- CAN 总线：仲裁、错误计数器、TEC/REC、Bus-Off
- CAN信号提取（Intel 字节序）

**Linux 嵌入式**
- sysfs GPIO (export/direction/value/unexport)
- LED触发控制（定时器触发，delay_on/delay_off）
- /proc/meminfo解析
- hwmon 温度读数
- 网络接口统计（/sys/class/net）

**物联网协议**
- MQTT：QoS 级别、主题通配符、PUBLISH 数据包二进制格式
- CoAP 与 MQTT 比较
- JSON 有效负载构造和简单解析
- TLS 概念（mbedTLS、mTLS、PSK）

**系统设计**
- 引导加载程序：闪存布局、A/B插槽、完整性+签名验证
- OTA 回滚更新流程
- 安全性：读保护、防回滚、OTP 保险丝

**面试练习**
- 100 个快速问题，完整answers
- 20 个真实的嵌入式错误（缺少 volatile、竞态条件、CRC 字节序等）
- 白板题：memcpy，环形缓冲区，CRC，状态机，字节顺序

---

## 这是给谁的

- 嵌入式工程师准备技术面试
- 学生从大学项目过渡到行业
- 高级工程师在担任新角色之前温习基础知识
- 任何想要比教程更深入的人

---

## 许可证

MIT — 使用它、分叉它、分享它。

---

## 作者

由一位嵌入式工程师构建，他花了几个月的时间准备意大利嵌入式公司（汽车、工业物联网、医疗设备）的面试。


这里的每个练习都来自真实的面试问题或真实的生产错误。

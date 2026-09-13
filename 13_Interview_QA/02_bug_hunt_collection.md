# 找错题 集合 — 20 个真实的嵌入错误

> 下面的每个错误都是嵌入式面试中出现的类型
> 真实的生产事故。有些是微妙的，有些是明显的。
> 这项技能是解释什么是错的以及为什么它很重要。

---

## Bug 1——反复无常的小姐

```c
uint8_t g_flag = 0;

void UART_IRQHandler(void) {
    g_flag = 1;
}

int main(void) {
    while (!g_flag) {
        // wait for ISR
    }
    process_data();
}
```

**出了什么问题？** `g_flag` 缺失 `volatile`。编译器认为`g_flag`在`main()`中永远不会改变（从它的角度来看），将循环优化为`while (1)`，并且`main()`永远不会退出。


**修复：** `volatile uint8_t g_flag = 0;`

---

## Bug 2——超时时整数溢出

```c
uint8_t start = get_tick_ms();   // returns uint8_t
// ... some processing ...
if ((get_tick_ms() - start) > 1000) {
    timeout_handler();
}
```

**出了什么问题？** `get_tick_ms()` 返回 `uint8_t`（最多 255）。对于 ≤ 255 的值，差异可以正确换行，但永远不会达到 1000 毫秒的超时，因为减法结果 (uint8_t) 永远不会超过 255。另外：1000 > 255，因此在编译时条件始终为 false（取决于类型）。


**修复：** 使用 `uint32_t` 作为刻度计数器。 `uint32_t` 1kHz 每 49 天环绕一次。

---

## Bug 3 — 在 malloc 之后缺少 NULL 检查

```c
void process_message(uint8_t len) {
    uint8_t *buf = malloc(len);
    memcpy(buf, g_uart_buf, len);
    parse_message(buf);
    free(buf);
}
```

**出了什么问题？** 如果 `malloc` 失败（堆已满），则 `buf` 为 NULL。 `memcpy(NULL, ...)` 是 UB — 裸机上的 HardFault。


**修复：** `if (!buf) { log_error(ERR_OOM); return; }`

---

## Bug 4 — 竞态条件 多字节读取

```c
// Written by ISR:
volatile uint16_t g_timestamp_high;
volatile uint16_t g_timestamp_low;

// Read by main:
uint32_t get_timestamp(void) {
    return ((uint32_t)g_timestamp_high << 16) | g_timestamp_low;
}
```

**出了什么问题？** 在读取 `g_timestamp_high` 和 `g_timestamp_low` 之间，ISR 可能会触发并更新两者。主要读取旧高+新低→垃圾时间戳。


**修复：** 禁用两次读取周围的中断，或者使用 32 位 atomic 类型（如果 MCU 支持）。

---

## Bug 5 — I2C 电源循环后总线卡住

```c
void i2c_init(void) {
    // Configure GPIO for SDA/SCL
    gpio_set_af(I2C_SDA_PIN, AF4);
    gpio_set_mode(I2C_SDA_PIN, GPIO_MODE_AF);
    // Initialize I2C peripheral
    i2c_configure(400000);
}
```

**出了什么问题？** 在电源周期上，如果从机正在发送数据，SDA 可能会被从机保持为低电平。 I2C 外设初始化不处理这道题。总线被卡住（SDA = 0 = 仲裁永远丢失）。


**修复：** 在`i2c_configure()`之前，使用GPIO切换SCL 9次以对卡住的从机进行时钟输出，然后发送STOP条件。

---

## 错误 6 — CRC 字节序

```c
uint16_t crc = compute_crc16(payload, len);
frame[len]   = crc >> 8;       // high byte first
frame[len+1] = crc & 0xFF;     // low byte second
```

**出了什么问题？** Modbus RTU 将 CRC 指定为 小端序（低字节优先）。此代码发送大端序。接收方的CRC检查将失败。


**修复：** `frame[len] = crc & 0xFF; frame[len+1] = crc >> 8;`

---

## 错误 7 — ISR 中的 printf

```c
void TIM2_IRQHandler(void) {
    TIM2->SR &= ~TIM_SR_UIF;
    g_tick++;
    if (g_tick % 1000 == 0) {
        printf("1 second elapsed\n");  // BUG
    }
}
```

**出了什么问题？** `printf`在内部使用`malloc`，访问锁（互斥锁），并且可能会阻塞等待UARTTX。从 ISR 调用它可能会导致系统死锁（如果 main 持有 printf 锁）、损坏堆，或者只是花费几毫秒的时间来阻塞 ISR。


**修复：** 在ISR中设置一个标志。在主循环中打印。

---

## Bug 8 — 返回栈分配的缓冲区

```c
const char *get_version_string(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "v%d.%d.%d", MAJOR, MINOR, PATCH);
    return buf;  // BUG: returning pointer to stack
}
```

**出了什么问题？** `buf` 是一个局部变量。当函数返回时，栈帧消失了。返回的指针是悬空的。该字符串可能会短暂地显示为有效（栈尚未被覆盖），然后默默地损坏。


**修复：** 使用`static char buf[32]`（但不是线程安全的）或调用者提供的缓冲区。

---

## Bug 9 — 通过 Cast 进行未对齐访问

```c
uint8_t frame[16];
// ...receive frame via UART...
uint16_t length = *(uint16_t*)(frame + 1);  // BUG: offset 1 is not 2-byte aligned
```

**出了什么问题？** 在 Cortex-M0/M0+ 上：HardFault（强制严格对齐）。在 M3/M4 上：可以工作，但编译器可能会生成 2 个字节读取而不是 1 个半字加载（速度较慢，非atomic 访问时可能会出现撕裂）。


**修复：** `uint16_t length = (uint16_t)((uint16_t)frame[1] << 8) | frame[2];`（无对齐假设）。

---

## Bug 10——看门狗在慢速分支中没有被喂食

```c
void main_loop(void) {
    while (1) {
        watchdog_feed();
        
        if (sensor_available()) {
            process_sensor();   // this takes 500ms
        }
    }
}
```

**出了什么问题？** 看门狗在循环顶部馈送。如果`sensor_available()`为true且`process_sensor()`需要500毫秒，而看门狗超时为400毫秒→系统在处理中重置。


**修复：** 在long操作中喂入看门狗，或者增加看门狗超时，或者将`process_sensor()`分成更小的部分。

---

## 错误 11 — Modbus 注册 字节序

```c
uint16_t register_val = 1500;  // RPM

response[3] = register_val & 0xFF;    // BUG: LSB first
response[4] = register_val >> 8;      // MSB second
```

**出了什么问题？** Modbus 寄存器是大端序（首先是MSB）。这会发送小端序。主站读取`(0x05 << 8) | 0xDC = 0xDC05 = 56325`而不是`0x05DC = 1500`。


**修复：** `response[3] = register_val >> 8; response[4] = register_val & 0xFF;`

---

## Bug 12——双重免费

```c
void cleanup(uint8_t *buf1, uint8_t *buf2) {
    free(buf1);
    free(buf2);
    if (buf1) free(buf1);   // BUG: freed twice
}
```

**出了什么问题？** `free(buf1)` 两次：在第一个 free 之后，该块的堆元数据可能会被重用。再次释放它会损坏堆内部结构——稍后崩溃或无声损坏。


**修复：** 在`free(p)`之后，始终设置`p = NULL`。第二个`if (p) free(p)`将被跳过。

---

## Bug 13——有符号/无符号比较 Bug

```c
void process_buffer(uint8_t *buf, int8_t len) {
    if (len < 0) return;
    for (uint8_t i = 0; i < len; i++) {  // BUG: mixing signed/unsigned
        process(buf[i]);
    }
}
```

**出了什么问题？** `i` 是`uint8_t`，`len` 是`int8_t`。比较 `i < len` — 如果 `len` 为负数，则它会被符号扩展为一个大的 unsigned ​​值（由于整数提升），并且循环无限期地运行。


**修复：** 使用一致的类型。 `uint8_t len` 或显式转换：`i < (uint8_t)len`。

---

## Bug 14 — ISR 清除错误标志

```c
void USART1_IRQHandler(void) {
    if (USART1->SR & USART_SR_RXNE) {
        uint8_t data = USART1->DR;
        ring_push(&g_rx_buf, data);
        USART1->SR &= ~USART_SR_RXNE;  // BUG
    }
}
```

**出了什么问题？** USART1_SR 中的 RXNE 是通过读取 DR 来清除的，而不是通过写入 SR 来清除。 `SR &= ~RXNE` 写入没有任何作用（在某些 MCU 上，向其他位写入 0 会意外清除错误标志）。而且DR的读已经清除了。


**修复：** 删除 `SR &= ~RXNE` 行。读取`DR`足以清除`RXNE`。

---

## Bug 15——减法中的定时器溢出

```c
uint32_t start = TIM2->CNT;
do_something();  // takes ~50ms
uint32_t elapsed = TIM2->CNT - start;  // BUG when counter wraps
if (elapsed > TIMEOUT_TICKS) timeout();
```

**出了什么问题？** 如果定时器回绕（CNT从0xFFFFFFFF溢出到0），`TIM2->CNT - start`给出一个巨大的数字，错误地触发超时。


**修复：** **实际上这很好**，如果两者都是`uint32_t`。无符号减法正确换行：`(0x00000005 - 0xFFFFFFF0) = 0x00000015`（正确的经过时间）。关键：两者必须是相同的unsigned类型。如果开始是`int32_t`，它就会中断。

---

## Bug 16 — 互斥锁已被占用，但从未在错误路径上释放

```c
int read_sensor(float *out) {
    xSemaphoreTake(g_spi_mutex, portMAX_DELAY);
    
    if (spi_exchange(0x80) != 0x55) {
        return -1;  // BUG: mutex never released!
    }
    
    *out = decode_reading(spi_exchange(0x00));
    xSemaphoreGive(g_spi_mutex);
    return 0;
}
```

**出了什么问题？** 在错误路径上，函数返回而不给出互斥锁。下一个任务尝试永远获取互斥锁 → 死锁。


**修复：** 每次返回之前始终提供互斥锁，或使用 goto 清理模式。

---

## Bug 17 — 位运算：设置前忘记清除

```c
void set_gpio_mode(GPIO_TypeDef *gpio, uint8_t pin, uint8_t mode) {
    gpio->MODER |= (mode << (pin * 2));  // BUG: ORs without clearing first
}
```

**出了什么问题？** 如果 pin 之前处于 `AF` 模式 (0b10) 并且你尝试设置 `OUTPUT` (0b01)，则OR 0b01 会进入0b10 = 0b11（模拟模式）。始终先清除field。


**修复：** `gpio->MODER = (gpio->MODER & ~(0b11u << (pin*2))) | ((uint32_t)mode << (pin*2));`

---

## Bug 18 — sprintf 缓冲区溢出

```c
char msg[16];
sprintf(msg, "Error code: %d, module: %s", error_code, module_name);
uart_send(msg);
```

**出了什么问题？** 如果 `error_code` 有很多数字和/或 `module_name` 是 long，则 `sprintf` 会写入超过 `msg[16]` 的末尾。无提示栈损坏（覆盖返回地址、其他本地地址等）。


**修复：** `snprintf(msg, sizeof(msg), ...)`。或者使用更大的缓冲区。检查最大可能的字符串长度。

---

## Bug 19 — 交换机中缺少 `break`

```c
void handle_command(uint8_t cmd) {
    switch (cmd) {
        case CMD_START:
            start_motor();
            // BUG: missing break — falls through to stop!
        case CMD_STOP:
            stop_motor();
            break;
        case CMD_RESET:
            system_reset();
            break;
    }
}
```

**出了什么问题？** 在 `CMD_START` 之后缺少 `break`。代码失败并在启动后立即调用`stop_motor()`。


**修复：** 在`start_motor();`之后添加`break;`。在 C17 中，在需要故意失败的地方添加`[[fallthrough]];`。

---

## Bug 20 — printf 阻止系统

```c
volatile uint8_t g_overrun = 0;

void UART_IRQHandler(void) {
    if (USART1->SR & USART_SR_ORE) {  // overrun error
        g_overrun = 1;
        printf("UART overrun!\n");  // BUG: printf IN ISR
    }
    // ...
}
```

**出了什么问题？** 与 Bug 7 相同 — ISR 中的 `printf`。这种情况特别危险：溢出错误意味着UART已经不堪重负，调用`printf`（内部使用UART）会使情况变得更糟。经典的反馈循环。


**修复：** 仅设置`g_overrun = 1`。登录主循环。增加一个计数器。切勿从 ISR 打印。

/*
 * 答案：03_Interrupts_and_ISR/01_isr_design.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * 任务 1 — 使用环形缓冲区更正 ISR
 * ============================================================ */

static volatile uint32_t UART_SR   = 0x20;  /* RXNE设置*/
static volatile uint8_t  UART_DR   = 0;
#define UART_SR_RXNE  (1u << 0)

#define RX_BUF_SIZE  128u

volatile uint8_t  g_rx_buf[RX_BUF_SIZE];
volatile uint8_t  g_rx_head    = 0;
volatile uint8_t  g_rx_tail    = 0;
volatile uint8_t  g_rx_overrun = 0;
volatile uint8_t  g_line_ready  = 0;

void UART_IRQHandler_CORRECT(void)
{
    if (!(UART_SR & UART_SR_RXNE)) return;   /* 规则 5：检查标志*/
    uint8_t c = UART_DR;                      /* 读取 DR 会清除真实硬件上的 RXNE*/

    uint8_t next_head = (uint8_t)((g_rx_head + 1) % RX_BUF_SIZE);
    if (next_head == g_rx_tail) {
        g_rx_overrun = 1;                     /* 缓冲区已满——丢弃字节*/
        return;
    }
    g_rx_buf[g_rx_head] = c;
    g_rx_head = next_head;

    if (c == '\n') g_line_ready = 1;
}

int process_received_line(char *out_buf, uint8_t out_max)
{
    if (!g_line_ready) return -1;

    uint8_t len = 0;
    while (g_rx_tail != g_rx_head && len < out_max - 1u) {
        char c = (char)g_rx_buf[g_rx_tail];
        g_rx_tail = (uint8_t)((g_rx_tail + 1) % RX_BUF_SIZE);
        out_buf[len++] = c;
        if (c == '\n') break;
    }
    out_buf[len] = '\0';
    g_line_ready = 0;
    return (int)len;
}

/* ============================================================
 * 任务 2 — SPSC 环形缓冲区
 * ============================================================ */

#define RING_BUF_MASK  0x3Fu
#define RING_BUF_SIZE  (RING_BUF_MASK + 1)

typedef struct {
    volatile uint8_t  buf[RING_BUF_SIZE];
    volatile uint8_t  head;
    volatile uint8_t  tail;
} RingBuf;

void ring_init(RingBuf *rb)
{
    memset((void*)rb->buf, 0, RING_BUF_SIZE);
    rb->head = 0;
    rb->tail = 0;
}

int ring_push(RingBuf *rb, uint8_t byte)
{
    uint8_t next_head = (rb->head + 1u) & RING_BUF_MASK;
    if (next_head == rb->tail) return -1;   /* 满*/
    rb->buf[rb->head] = byte;
    rb->head = next_head;
    return 0;
}

int ring_pop(RingBuf *rb, uint8_t *out)
{
    if (rb->head == rb->tail) return -1;   /* 空的*/
    *out = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1u) & RING_BUF_MASK;
    return 0;
}

uint8_t ring_available(const RingBuf *rb)
{
    return (rb->head - rb->tail) & RING_BUF_MASK;
}

/* ============================================================
 * 任务 3 — SensorSnapshot 的临界区
 * ============================================================ */

static uint8_t g_irq_enabled = 1;
static void __disable_irq(void) { g_irq_enabled = 0; }
static void __enable_irq(void)  { g_irq_enabled = 1; }

typedef struct {
    uint32_t timestamp;
    float    temperature;
    float    pressure;
    uint8_t  valid;
} SensorSnapshot;

volatile SensorSnapshot g_sensor;

void sensor_isr(uint32_t ts, float temp, float pressure)
{
    __disable_irq();        /* 防止更高优先级ISR读取部分写入*/
    g_sensor.timestamp   = ts;
    g_sensor.temperature = temp;
    g_sensor.pressure    = pressure;
    g_sensor.valid       = 1;
    __enable_irq();
}

SensorSnapshot sensor_get_snapshot(void)
{
    SensorSnapshot local;
    __disable_irq();
    local = g_sensor;       /* atomic struct 复制到临界区*/
    __enable_irq();
    return local;
}

/* ============================================================
 * 任务 4 — DMA 双缓冲
 * ============================================================ */

#define DMA_BUF_SIZE  256u

uint8_t g_dma_buf_a[DMA_BUF_SIZE];
uint8_t g_dma_buf_b[DMA_BUF_SIZE];

volatile uint8_t g_dma_buf_ready  = 0;   /* 1=buf_a 就绪，2=buf_b 就绪*/
volatile uint8_t g_dma_active_buf = 0;   /* 0=正在填充 buf_a, 1=正在填充 buf_b*/

void DMA_IRQHandler(void)
{
    /* 指示哪个缓冲区刚刚完成*/
    g_dma_buf_ready = (g_dma_active_buf == 0) ? 1u : 2u;
    /* 开关活动缓冲器*/
    g_dma_active_buf = (g_dma_active_buf == 0) ? 1u : 0u;
    /* 在实际代码中：在此处重新启动DMA在新的活动缓冲区上传输*/
}

uint8_t *dma_get_ready_buffer(uint16_t *len)
{
    if (g_dma_buf_ready == 0) { *len = 0; return NULL; }
    uint8_t *buf = (g_dma_buf_ready == 1) ? g_dma_buf_a : g_dma_buf_b;
    *len = DMA_BUF_SIZE;
    g_dma_buf_ready = 0;
    return buf;
}

/* ============================================================
 * 任务 5 — 找错题 已修复
 *
 * 错误 1 和 2：g_pulse_width_ticks 不是 volatile。
 *   编译器可能会将写入缓存在寄存器中，并且永远不会刷新到RAM。
 *   Main 读取陈旧值。修复：volatileuint32_tg_pulse_width_ticks；
 *
 * Bug 3：在 get_pulse_width() 中忙等待（while (!g_pulse_ready){}）。
 *   这会阻塞主循环，浪费CPU周期，阻止其他任务
 *   运行，并且可能会导致低优先级的处理不足。
 *   修复：使用RTOS事件标志或二进制信号量；或 返回 -1（如果未准备好）
 *   并让调用者重试（非阻塞轮询模式）。
 * ============================================================ */

volatile uint32_t g_rise_tick = 0;
volatile uint32_t g_fall_tick = 0;
volatile uint8_t  g_pulse_ready = 0;
volatile uint32_t g_pulse_width_ticks;   /* 已修复：现在volatile*/

void rising_edge_isr(uint32_t current_tick)  { g_rise_tick = current_tick; g_pulse_ready = 0; }
void falling_edge_isr(uint32_t current_tick) {
    g_fall_tick = current_tick;
    g_pulse_width_ticks = g_fall_tick - g_rise_tick;
    g_pulse_ready = 1;
}

int get_pulse_width_nonblocking(uint32_t *width_out)
{
    /* 修复：非阻塞 - 调用者决定在未准备好时做什么*/
    if (!g_pulse_ready) return -1;
    *width_out = g_pulse_width_ticks;
    g_pulse_ready = 0;
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1：在 ISR 内绝不能做的 5 件事，以及原因。

A: 1. printf() / sprintf() — 内部使用 malloc 作为缓冲区，调用锁
      （互斥锁）在I/O流上。如果 main 持有锁，可能会死锁。
   2. malloc() / free() — 堆函数采用互斥锁；堆状态可能是
      ISR 触发时不一致；不确定的运行时间。
   3. delay_ms() / vTaskDelay() — 阻塞 ISR 停止所有其他 IRQ
      相同或较低的优先级。系统出现挂起。
   4. xSemaphoreTake()（阻塞）— 可以尝试阻塞内部ISR，其中
      是 未定义行为 在 FreeRTOS 中；请使用 xSemaphoreGiveFromISR() 代替。
   5. 浮点运算（无 FPU 上下文保存）— 在 Cortex-M4F 上，
      FPU 寄存器（S0-S15、FPSCR）不会保存在中断入口处
      默认（延迟堆叠）。使用 FP 的ISR 会损坏 main 的 FP 寄存器。

Q2：Cortex-M4 中断入口自动保存哪些寄存器？

A：硬件自动将8个寄存器压入当前栈（MSP或PSP）：
   R0、R1、R2、R3 — 参数/返回 寄存器（调用者保存）
   R12——暂存寄存器
   LR (R14) — 链接寄存器（返回 地址）
   PC (R15) — 中断指令的程序计数器
   xPSR — 处理器状态寄存器（标志、ISR 编号、Thumb 状态）
   这 8 个寄存器 × 4 字节 = 最少 32 字节栈帧。
   不自动保存：R4-R11（被调用者保存 - 编译器将它们保存在序言中
   如果 ISR 使用它们）。 FPU 寄存器 S0-S15 延迟保存 (FPCCR.LSPEN)。

Q3：什么是杂散中断？你如何在防守上处理它？

答：触发了一个没有可识别源的虚假中断 — ISR 向量
   已输入，但状态寄存器未显示待处理标志。
   原因：IRQ线上的电噪声，竞态条件清除标志，
   软件在标志被清除之前重新启用中断。
   防守处理：始终检查每个ISR顶部的标志：
     if (!(PERIPH->SR & EXPECTED_FLAG)) 返回；  // 虚假的——忽略
   切勿假设 ISR 因预期原因而被触发。

Q4：在 32 位 MCU 上，在 ISR 和 main 之间共享 uint64_t。原子？

答：不需要。64 位读取需要两个 32 位总线事务（LDRD 或两个 LDR）。
   在第一次和第二次读取之间，ISR 可以触发并更新两半。
   结果：主读取 old_high + new_low — 损坏的值。
   解决方案：
   (a) 临界区：禁止中断，读取，使能中断。
   (b) 双复制模式：读取直到两个连续读取一致。
   (c) 顺序计数器：ISR 在写入之前/之后递增计数器；
       main 读取直到计数器没有改变（seqlock 模式）。

Q5：Cortex-M 上可屏蔽中断和不可屏蔽中断的区别。

A：可屏蔽（IRQ）：可以使用 PRIMASK 全局禁用（CPSID I / __disable_irq()）
   或选择性地使用 BASEPRI。所有外设中断（UART、TIM、DMA 等）
   可屏蔽。 FreeRTOS 使用 BASEPRI 来屏蔽低于阈值的 IRQ。
   不可屏蔽中断 (NMI)：无法通过软件禁用。总是有回应。
   用途：时钟故障监视器、NMI 模式下的看门狗、灾难性硬件故障。
   同样不可屏蔽：HardFault、Reset。
   HardFault：由内存访问违规、无效指令触发。
   在 Cortex-M33 (TrustZone) 上：SecureFault 也无法被非安全代码屏蔽。

Q6：ARM Cortex-M 上的尾链是什么？为什么它会减少延迟？

答：当CPU完成一个ISR并且另一个IRQ正在等待（同等或更低优先级）时，
   而不是完全拆栈（恢复 8 个寄存器）并为下一个ISR重新堆叠
   （又省了8个寄存器），直接从ISR出口到下一个ISR入口。
   节省大约 12 个周期（相比之下，unstack+stack 节省约 30 个周期）。
   栈帧 保持不变（SP 不变）。 EXC_RETURN机制
   直接识别待处理的IRQ和"chains"。
   实际影响：在 168 MHz 时，每个链接中断可节省 12 个周期 = ~71 ns。
   对于具有多个同时 IRQ 的系统（CAN 邮箱、ADC 过采样）至关重要。
*/

int main(void)
{
    /* 环形缓冲区测试*/
    RingBuf rb;
    ring_init(&rb);
    assert(ring_available(&rb) == 0);

    for (uint8_t i = 0; i < 10; i++) ring_push(&rb, i);
    assert(ring_available(&rb) == 10);

    uint8_t out;
    ring_pop(&rb, &out);
    assert(out == 0);
    assert(ring_available(&rb) == 9);

    /* 临界区测试*/
    sensor_isr(1000, 25.5f, 1013.25f);
    SensorSnapshot snap = sensor_get_snapshot();
    assert(snap.valid == 1 && snap.timestamp == 1000);

    /* DMA double 缓冲测试*/
    g_dma_active_buf = 0;
    DMA_IRQHandler();
    assert(g_dma_buf_ready == 1);
    assert(g_dma_active_buf == 1);
    uint16_t len;
    uint8_t *buf = dma_get_ready_buffer(&len);
    assert(buf == g_dma_buf_a && len == DMA_BUF_SIZE);
    assert(g_dma_buf_ready == 0);

    /* 脉宽测试*/
    rising_edge_isr(1000);
    falling_edge_isr(1500);
    uint32_t width;
    assert(get_pulse_width_nonblocking(&width) == 0);
    assert(width == 500);

    printf("All ISR design answers verified.\n");
    return 0;
}

/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * 主题：volatile、const、static — 三个关键词
 *         每场嵌入式面试 询问
 * 文件：01_C_Fundamentals/04_volatile_const_static.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/* ============================================================
 * THEORY
 * ============================================================
 *
 * volatile：
 *   告诉编译器：“不要优化这个读/写。
 *   该变量可以在正常程序流程之外更改
 *   （硬件，ISR，其他CPU核心）。”
 *   如果没有volatile，编译器可能会缓存在寄存器中
 *   并且永远不会重新阅读 - 你的 ISR 标志永远不会被看到。
 *
 * const：
 *   "This value will not change." 编译器可以放在ROM。
 *   在 MCU 上：const 全局变量位于 Flash (free SRAM) 中的 .rodata。
 *   const + volatile 一起：硬件状态寄存器
 *   它会自己改变，但你永远不会写入它。
 *
 * static：
 *   在函数作用域：变量在调用中仍然存在（存储
 *   在.data 或.bss 中，不在栈中）。
 *   在文件范围内：限制对此翻译单元的可见性
 *   （"private" 的嵌入等效项）。
 *
 * const volatile uint32_t *:
 *   自行更改的只读硬件寄存器。
 *   示例：RX 数据寄存器 — 你读取它，硬件写入它。
 * ============================================================ */


/* ============================================================
 * 任务 1 — volatile：ISR 至主通信
 *
 * UART ISR 填充环形缓冲区并设置标志。
 * 主循环必须看到标志的变化。如果没有volatile，会出现什么问题？
 * ============================================================ */

/* 标志 — ISR 设置它，main 读取它*/
/* TODO：将正确的限定符添加到此声明中*/
uint8_t g_data_ready = 0;

/* 环形缓冲区 — 由ISR写入，由main读取*/
/* TODO：添加正确的限定符*/
uint8_t g_rx_buf[64];
/* TODO：添加正确的限定符*/
uint8_t g_rx_head = 0;
uint8_t g_rx_tail = 0;

/* 模拟 ISR — 在真实 MCU 上将是 USART1_IRQHandler*/
void simulated_uart_isr(uint8_t received_byte)
{
    g_rx_buf[g_rx_head % 64] = received_byte;
    g_rx_head++;
    g_data_ready = 1;
}

int main_loop_iteration(void)
{
    /* TODO：检查g_data_ready
     * 如果设置：从g_rx_buf[g_rx_tail % 64]读取一个字节，增量g_rx_tail
     *         如果g_rx_tail==g_rx_head：清除g_data_ready
     *         return the byte read
     * 如果未设置：返回 -1*/
    return -1;
}

/* ============================================================
 * 任务 2 — const 用于查找表和配置
 *
 * 在微控制器上，const 全局数组位于Flash。
 * 这可以释放宝贵的 SRAM。
 *
 * 实现PWM占空比到DAC输出查找表。
 * 该表应位于 ROM (Flash) 中，而不是 SRAM 中。
 * ============================================================ */

/* TODO：使用正确的限定符声明此表，以便它
 * 住在 ROM 上的 MCU — 256 个条目，0 到 255*/
uint8_t pwm_to_dac_table[256]; /* TODO：修复限定符，TODO：填写值0..255*/

uint8_t pwm_to_dac(uint8_t pwm_percent)
{
    /* TODO：将pwm_percent钳位至0-99，对工作台进行索引
     * （100% PWM = 255DAC，线性）*/
    (void)pwm_percent;
    return 0;
}

/* ============================================================
 * 任务 3 — static 用于状态保留和封装
 * ============================================================ */

/* 一个简单的去抖过滤器——必须保留调用之间的状态*/
uint8_t debounce_button(uint8_t raw_pin_state)
{
    /* TODO：声明一个 static uint8_t 计数器 = 0
     *       声明 static uint8_t stable_state = 0
     * 逻辑：
     *   if raw_pin_state == stable_state：重置计数器，返回 stable_state
     *   else: 递增计数器
     *         如果计数器 >= 5：stable_state = raw_pin_state，计数器 = 0
     *   return stable_state */
    (void)raw_pin_state;
    return 0;
}

/* 毫秒刻度计数器 — 按 SysTick ISR 递增*/
static volatile uint32_t g_tick_ms = 0;

void systick_isr(void) /* 模拟的*/
{
    g_tick_ms++;
}

uint32_t get_tick_ms(void)
{
    /* TODO：返回g_tick_ms
     * 问：该读取是否应该受到保护？为什么/为什么不在 32 位 ARM 上？*/
    return 0;
}

uint8_t has_elapsed_ms(uint32_t start_tick, uint32_t duration_ms)
{
    /* TODO: 返回 1 如果 (get_tick_ms() - start_tick) >= duration_ms
     * 注意：即使计数器环绕，这也能正常工作！
     * （unsigned 减法在 C 中正确换行）
     * 这是在嵌入式中测量经过时间的正确方法。*/
    (void)start_tick; (void)duration_ms;
    return 0;
}

/* ============================================================
 * 任务 4 — const volatile 一起：只读硬件寄存器
 *
 * 硬件定时器计数器寄存器：
 * - 不断变化（硬件递增 → volatile）
 * - 你不应该从软件写入它（const）
 * ============================================================ */

/* 模拟定时器计数器寄存器*/
static volatile uint32_t _fake_timer_cnt = 0;

/* TODO：将timer_count_reg声明为指向
 * a const volatile uint32_t，指向_fake_timer_cnt*/
/* const volatile uint32_t *timer_count_reg = ...*/

uint32_t read_timer_count(void)
{
    /* TODO：解引用 timer_count_reg 和 返回 值*/
    return 0;
}

/* 尝试编写 - 如果声明正确则不应编译*/
/* void BAD_write_timer(uint32_t v) { *timer_count_reg = v; }*/

/* ============================================================
 * 任务 5 — static 在文件范围：模块私有状态
 *
 * 实现软件PWM模块。内部状态应该
 * 无法从此文件外部访问。
 * ============================================================ */

/* TODO：将它们声明为文件私有（无法从其他 .c 文件访问）*/
uint8_t  pwm_duty_pct = 0;
uint32_t pwm_period_ticks = 0;
uint32_t pwm_on_ticks     = 0;

void pwm_set_duty(uint8_t duty_pct, uint32_t period_ticks)
{
    /* TODO：保存period_ticks和duty_pct
     * TODO：计算pwm_on_ticks = (duty_pct * period_ticks) / 100*/
    (void)duty_pct; (void)period_ticks;
}

/* 从计时器 ISR 每个 Tick 调用*/
uint8_t pwm_get_output(uint32_t current_tick)
{
    /* TODO: 返回 1 如果 (current_tick % pwm_period_个 勾选) < pwm_on_ticks
     * return 0 otherwise
     * 句柄 pwm_period_ticks == 0（除以零保护）*/
    (void)current_tick;
    return 0;
}

/* ============================================================
 * 任务 6 — 找错题
 *
 * 下面的函数应该使用非阻塞定时器使 LED 闪烁。
 * 它有 3 个与 volatile/static/const 滥用相关的错误。
 * 找到并标记每一个。
 * ============================================================ */

uint32_t tick = 0;   /* 错误1：？？？ — 这是在轮询循环中读取的*/

void led_blink_BUGGY(void)
{
    uint32_t last_toggle = 0;
    uint8_t  led_state   = 0;
    const uint32_t BLINK_INTERVAL = 500;   /* 女士*/

    /* 错误2：？？？ — 'last_toggle' 是局部变量。
     * 每次调用这个函数时会发生什么？*/
    while (1) {
        if ((tick - last_toggle) >= BLINK_INTERVAL) {
            led_state   ^= 1;
            last_toggle  = tick;
            printf("LED: %s\n", led_state ? "ON" : "OFF");
        }
        /* 错误3：？？？ — 'tick' 这里永远不会递增。在真实系统中
         * 它将增加 SysTick ISR。但限定符是什么
         * 缺少让编译器优化的声明
         * 这个循环进入无限紧密循环读取缓存寄存器？*/
    }
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * 问题 1：如果从 ISR 标志变量中删除 volatile 会发生什么？
 *     给出一个具体的编译器优化示例。
 *     答案：TODO
 *
 * Q2：一个变量可以同时是const和volatile吗？举个真实的例子。
 *     答案：TODO
 *
 * Q3：static全局和非static全局有什么区别？
 *     为什么嵌入式样式指南更喜欢static文件范围变量？
 *     答案：TODO
 *
 * Q4：uint32_t计数器在SysTickISR中递增并在main中读取。
 *     在 32 位 ARM Cortex-M4 上，读取的是atomic吗？在 Cortex-M0 上？
 *     答案：TODO
 *
 * Q5：你在 STM32 项目中有一个 const uint8_t lookup_table[512]。
 *     它实际生活在哪里？你如何验证这一点？
 *     答案：TODO
 */

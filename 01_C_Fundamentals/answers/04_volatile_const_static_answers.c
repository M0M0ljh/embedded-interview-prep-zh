/*
 * 答案：04_volatile_const_static.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/* ============================================================
 * 任务 1 — 正确的 volatile 限定词
 * ============================================================ */

volatile uint8_t  g_data_ready = 0;   /* volatile：ISR集，主要阅读*/
volatile uint8_t  g_rx_buf[64];       /* volatile：ISR写入，主要读取*/
volatile uint8_t  g_rx_head = 0;      /* volatile：由ISR撰写*/
volatile uint8_t  g_rx_tail = 0;      /* volatile：由 main 编写*/

void simulated_uart_isr(uint8_t received_byte)
{
    g_rx_buf[g_rx_head % 64] = received_byte;
    g_rx_head++;
    g_data_ready = 1;
}

int main_loop_iteration(void)
{
    if (!g_data_ready) return -1;
    uint8_t byte = g_rx_buf[g_rx_tail % 64];
    g_rx_tail++;
    if (g_rx_tail == g_rx_head) g_data_ready = 0;
    return (int)byte;
}

/* ============================================================
 * 任务 2 — const ROM 中的查找表
 * ============================================================ */

/* const → 放置在 .rodata (Flash) — 未复制到 SRAM*/
static const uint8_t pwm_to_dac_table[256] = {
    /* 线性映射：索引 i → 值 i (0..255)*/
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
    16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
    32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
    48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
    64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
    80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
    96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
    112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
    128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
    160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
    176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
    192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
    208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
    224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
    240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};

uint8_t pwm_to_dac(uint8_t pwm_percent)
{
    if (pwm_percent > 99) pwm_percent = 99;
    /* 线性：0%→0、99%→252（大约）*/
    return pwm_to_dac_table[(pwm_percent * 255) / 100];
}

/* ============================================================
 * 任务 3 — static 用于状态保留
 * ============================================================ */

uint8_t debounce_button(uint8_t raw_pin_state)
{
    static uint8_t counter     = 0;
    static uint8_t stable_state = 0;

    if (raw_pin_state == stable_state) {
        counter = 0;
    } else {
        counter++;
        if (counter >= 5) {
            stable_state = raw_pin_state;
            counter = 0;
        }
    }
    return stable_state;
}

static volatile uint32_t g_tick_ms = 0;

void systick_isr(void) { g_tick_ms++; }

uint32_t get_tick_ms(void)
{
    /* 在 32 位 ARM 上：单个 uint32_t 读取为 atomic — 不需要 临界区。
     * 在 8 位 MCU 上：需要 临界区（4 字节读取不是 atomic）。*/
    return g_tick_ms;
}

uint8_t has_elapsed_ms(uint32_t start_tick, uint32_t duration_ms)
{
    /* 无符号减法可以正确处理环绕。
     * 示例：开始=0xFFFFFF00，现在=0x00000100，持续时间=512
     * 现在开始 = 0x00000200 = 512 ≥ 512 → 已过去。正确的！*/
    return (get_tick_ms() - start_tick) >= duration_ms ? 1u : 0u;
}

/* ============================================================
 * 任务 4 — const volatile 指向硬件寄存器的指针
 * ============================================================ */

static volatile uint32_t _fake_timer_cnt = 100;

/* const：软件不得写入
 * volatile：硬件改变了它（编译器每次都必须重新读取）*/
const volatile uint32_t *timer_count_reg = &_fake_timer_cnt;

uint32_t read_timer_count(void) { return *timer_count_reg; }

/* 如果限定符正确，则不会编译：
 * void BAD_write_timer(uint32_t v) { *timer_count_reg = v; }*/

/* ============================================================
 * 任务 5 — static 文件范围 PWM 模块
 * ============================================================ */

static uint8_t  pwm_duty_pct    = 0;
static uint32_t pwm_period_ticks = 0;
static uint32_t pwm_on_ticks     = 0;

void pwm_set_duty(uint8_t duty_pct, uint32_t period_ticks)
{
    pwm_duty_pct     = duty_pct;
    pwm_period_ticks = period_ticks;
    pwm_on_ticks     = (uint32_t)((duty_pct * (uint64_t)period_ticks) / 100u);
}

uint8_t pwm_get_output(uint32_t current_tick)
{
    if (pwm_period_ticks == 0) return 0;
    return ((current_tick % pwm_period_ticks) < pwm_on_ticks) ? 1u : 0u;
}

/* ============================================================
 * 任务 6 — 找错题 已修复
 *
 * 错误 1：`uint32_t tick` 应该是 `volatile uint32_t tick`
 *        如果没有volatile，编译器会发现 Tick 在
 *        while(1) 循环（从它的角度来看，ISR是不可见的）和
 *        优化整个条件→无限紧密循环。
 *
 * Bug 2：`uint32_t last_toggle = 0`是led_blink_BUGGY()内部的LOCAL变量。
 *        每次调用该函数时，它都会重置为 0。
 *        它必须是`static uint32_t last_toggle = 0`才能在调用之间保持不变。
 *        （或者该函数应该只调用一次并在内部循环。）
 *
 * Bug 3：`tick`在循环中永远不会递增。
 *        在真实的MCU上，这是由SysTickISR完成的。失踪的`volatile`
 *        Bug 1 甚至阻止编译器检查 ISR 更改。
 *        修复：volatile + ISR 增加 Tick。
 * ============================================================ */

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

问题 1：如果从 ISR 标志变量中删除 volatile 会发生什么？

答：如果没有volatile，编译器会像变量一样分析代码
   永远不会在正常程序流程之外进行更改。在main()中：
     while (!g_flag) {} // 编译器：g_flag 始终为 0 → while(1)
   编译器将 g_flag 缓存在循环顶部的寄存器中，并且
   永远不会重新读取内存。 ISR 写入RAM，但main() 读取其
   永久缓存寄存器。系统挂起。
   GCC -O2 的示例：循环成为返回自身的单个 CMP + 分支。

Q2：一个变量可以同时是const和volatile吗？真实的例子。

答：是的。 const volatile uint32_t *STATUS_REG = (uint32_t*)0x40013800;
   `const`：软件不得对其进行写入（只读寄存器）。
   `volatile`：值自主变化（硬件写入），所以
               编译器必须在每次访问时重新读取，而不是缓存。
   没有volatile：编译器读取一次并优化以后的读取。
   如果没有 const：编码错误可能会写入 HW 寄存器。

Q3：static 全球与非static 全球。为什么更喜欢static？

答：非static全局：通过`extern`对所有翻译单元可见。任意.c
   文件可能会被意外读取或修改。
   静态全局（文件范围）：仅在声明 .c 文件中可见。
   嵌入式的好处：
   - 封装：防止其他模块绕过你的API。
   - 链接器优化：链接器可以消除未使用的static全局变量。
   - 防止名称冲突：两个文件可以有`static int counter`而不会发生冲突。
   嵌入式风格指南（MISRA C:2012 规则 8.7）要求变量使用 static
   不被其他翻译单位使用。

Q4：SysTickISR中的uint32_t计数器，在main中读取。 M4上的原子？ M0？

答：Cortex-M4（32 位）：是的，单个 32 位对齐读取（LDR 指令）是
   atomic — 总线不可分割地完成整个 32 位事务。
   Cortex-M0/M0+（32 位总线，但指令集较窄）：也用于 atomic
   对齐的 32 位访问。
   但是：如果计数器不是volatile，编译器可能不会重新读取它
   每次。无论原子性如何，始终使用`volatile`。
   非atomic情况：uint64_t（两次 32 位读取）、结构、未对齐访问。

Q5：constuint8_tlookup_table[512]在STM32。它住在哪里？

答：在 .rodata 节中，链接器将其放置在Flash（只读存储器）中。
   它不消耗 SRAM。这是关键优势：SRAM 中的 512 字节表
   会白白吃掉 512/8192 = 6.25% of 8KB SRAM 预算。
   验证：arm-none-eabi-nmfirmware.elf | grep lookup_table
   你会看到该地址位于 Flash 范围内（例如，0x08002xxx）。
   另外：arm-none-eabi-sizefirmware.elf → 检查.text+.rodata 与.data+.bss。
*/

int main(void)
{
    /* ISR→主要通讯测试*/
    simulated_uart_isr(0x42);
    int b = main_loop_iteration();
    assert(b == 0x42);
    assert(main_loop_iteration() == -1);  /* 没有更多数据*/

    /* Debounce：需要连续5次不同的读取才能改变状态*/
    assert(debounce_button(1) == 0);
    assert(debounce_button(1) == 0);
    assert(debounce_button(1) == 0);
    assert(debounce_button(1) == 0);
    assert(debounce_button(1) == 1);  /* 第五次通话 → 稳定*/

    /* 刻度已过*/
    g_tick_ms = 1000;
    assert(has_elapsed_ms(900, 100) == 1);
    assert(has_elapsed_ms(900, 200) == 0);

    /* 定时器count寄存器*/
    assert(read_timer_count() == 100);

    /* PWM */
    pwm_set_duty(50, 1000);
    assert(pwm_on_ticks == 500);
    assert(pwm_get_output(0)   == 1);
    assert(pwm_get_output(499) == 1);
    assert(pwm_get_output(500) == 0);
    assert(pwm_get_output(999) == 0);

    printf("All volatile/const/static answers verified.\n");
    return 0;
}

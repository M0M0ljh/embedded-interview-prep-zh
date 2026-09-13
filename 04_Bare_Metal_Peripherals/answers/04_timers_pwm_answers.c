/*
 * 答案：04_Bare_Metal_Peripherals/04_timers_pwm.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1：定时器输出 84 MHz 起 1 Hz，PSC=8399，ARR=9999。核实。

答：输出频率 = FCLK / ((PSC+1) * (ARR+1))
   = 84,000,000 / (8400 * 10000)
   = 84,000,000 / 84,000,000
   = 1 赫兹 ✓
   周期 = 1 秒。定时器在 10 kHz 内部时钟时计数 0..9999（10000 个 系统节拍（Tick））。

Q2：PWM 占空比停留在 0%。调试清单。

A: 1. 检查CCER：CC1E 位（通道1 输出使能）必须置位（=1）。
      如果 CC1E=0，pin 始终由GPIO 默认驱动，而不是PWM。
   2. 检查CCR1值：如果PWM模式1下CCR1=0（CNT<CCR→HIGH），
      计数 0 时输出为高电平 → 有效为 0% duty。
   3. 检查 CCMR1：OC1M[2:0] 必须为 110（PWM 模式 1）或 111（模式 2）。
      如果 OC1M=000（冻结），输出不会改变。
   4. 检查GPIO备用功能：pin必须处于AF模式且AF#正确。
      如果pin处于GPIO输出模式，PWM信号不会出现在pin上。
   5. 检查 ARR 预载：如果更改 CCR，则应设置 OC1PE（CCMR1 位 3）
      操作期间——防止出现故障。
   6. 检查 BDTR（高级定时器 TIM1/TIM8）：MOE（主输出使能）位
      必须设置。高级定时器需要 MOE=1 才能在引脚上显示输出。

Q3：PWM 频率和占空比测量的输入捕获。

答：在同一个pin（CC1S/CC2S）上使用两个通道：
   CC1：上升沿触发（IC1F=00，CC1P=0=上升沿）
   CC2：在同一pin下降沿触发（CC2S=10=IC2映射到TI1，
        CC2P=1=falling)
   周期测量：上升沿捕获CC1→等待→再次捕获CC1。
   频率 = TIMER_CLOCK / (CC1[2] - CC1[1]) -- 或 CC1 的使用周期
   占空比：捕获 CC2（下降沿）和 CC1（下一个上升沿）。
   high_time = CC2 - CC1（前一个上升沿）
   周期 = CC1_new - CC1_old
   duty_cycle = (high_time * 100) / 周期
   重置 CC1 捕获上的计数器（TI1FP1 上的 SMS=100 重置模式）以进行干净的测量。

Q4: SysTick FreeRTOS 系统节拍（Tick） vs sysTick 裸机 延迟。区别？

答：FreeRTOS 系统节拍（勾选）：SysTick 每 1 毫秒（或 1/configTICK_RATE_HZ 秒）触发一次。
   SysTick_Handler 调用 xPortSysTickHandler()，其中：递增 Tick count，
   检查是否有任何被阻塞的任务应该唤醒，如果有上下文则触发 PendSV
   需要切换。 PendSV 处理程序执行实际的上下文切换。
   Tick ISR 不得禁用 FreeRTOS 中断（必须等于或低于
   configMAX_SYSCALL_INTERRUPT_PRIORITY）。

   裸机 延迟：SysTick 从重载值开始倒计时，触发 COUNTFLAG。
   循环检查 COUNTFLAG 或减少 SysTick_Handler 中的计数器。
   没有RTOS参与。阻塞延迟。

Q5：PSC=0 时的定时器分辨率，168 MHz。最小可测量脉冲？

答：PSC=0：定时器每 1/168MHz 递增 = ~5.95 ns。
   这是定时器分辨率。
   最小可测量脉冲 = 1 个定时器 Tick = 5.95 ns。
   实际限制：输入捕捉抖动增加了 1-2 个 Tick 的不确定性。
   因此，最小可靠测量脉冲 ≈ 2-3 个 Tick ≈ 12-18 ns。
   ARR=0xFFFF (65535)：最大可测量周期 = 65535 × 5.95 ns ≈ 390 µs。
   对于较长的脉冲，请使用较高的 PSC。

Q6：输入捕获中定时器溢出/翻转——如何处理？

A：如果测量的脉冲长于一个定时器周期，则捕获的 CC2
   值小于CC1（边缘之间发生溢出）。
   处理： if (CC2 >= CC1) width = CC2 - CC1;
                否则宽度 = (ARR + 1 - CC1) + CC2;
   更好：使用 uint32_t 减法，如果 ARR=0xFFFF 则可以正确换行
   值为 16 位：宽度 = (uint16_t)(CC2 - CC1)；
   启用定时器溢出中断：递增 32 位溢出计数器。
   真正的 32 位时间戳 = (overflow_count << 16) | CCR_value。
   然后：宽度=时间戳2 - 时间戳1（自然uint32_t减法处理换行）。
*/

typedef struct {
    volatile uint32_t CR1, CR2, SMCR, DIER, SR, EGR;
    volatile uint32_t CCMR1, CCMR2, CCER, CNT, PSC, ARR;
    volatile uint32_t RCR, CCR1, CCR2, CCR3, CCR4;
    volatile uint32_t BDTR, DCR, DMAR;
} TIM_TypeDef;

#define TIM_CR1_CEN       (1u << 0)
#define TIM_CR1_ARPE      (1u << 7)
#define TIM_CCMR1_OC1M_PWM1  (0b110u << 4)
#define TIM_CCMR1_OC1PE   (1u << 3)
#define TIM_CCER_CC1E     (1u << 0)

static TIM_TypeDef _TIM2 = {0}, _TIM3 = {0};
TIM_TypeDef *TIM2 = &_TIM2, *TIM3 = &_TIM3;

/* ============================================================
 * 任务 1 — 定时器频率计算
 * ============================================================ */

void timer_calc_psc_arr(uint32_t timer_clk_hz, uint32_t target_freq_hz,
                        uint16_t *psc_out, uint32_t *arr_out)
{
    /* 尝试找到 ARR < 65536 的 PSC。
       ARR = timer_clk / ((PSC+1) * 频率) - 1*/
    for (uint32_t psc = 0; psc < 65536; psc++) {
        uint32_t arr = (timer_clk_hz / ((psc + 1) * target_freq_hz));
        if (arr >= 1 && arr <= 65536) {
            *psc_out = (uint16_t)psc;
            *arr_out = arr - 1;
            return;
        }
    }
    /* 回退——尽力而为*/
    *psc_out = 0xFFFFu;
    *arr_out = 0xFFFFu;
}

/* ============================================================
 * 任务 2 — 定时器时基初始化
 * ============================================================ */

void timer_init_timebase(TIM_TypeDef *tim, uint16_t psc, uint32_t arr)
{
    tim->CR1 &= ~TIM_CR1_CEN;     /* 禁用*/
    tim->PSC  = psc;
    tim->ARR  = arr;
    tim->CR1 |= TIM_CR1_ARPE;     /* ARR预载*/
    tim->EGR  = 1u;               /* UG：强制寄存器更新*/
    tim->SR   = 0;                /* 清除UIF*/
    tim->CR1 |= TIM_CR1_CEN;      /* 启用*/
}

/* ============================================================
 * 任务 3 — PWM 输出初始化
 * ============================================================ */

void timer_pwm_init(TIM_TypeDef *tim, uint16_t psc, uint32_t arr)
{
    tim->CR1 &= ~TIM_CR1_CEN;
    tim->PSC  = psc;
    tim->ARR  = arr;
    tim->CR1 |= TIM_CR1_ARPE;

    /* PWM 通道 1 上的模式 1，启用预载*/
    tim->CCMR1 = (tim->CCMR1 & ~(0b111u << 4)) | TIM_CCMR1_OC1M_PWM1 | TIM_CCMR1_OC1PE;

    /* 启用通道 1 输出*/
    tim->CCER |= TIM_CCER_CC1E;

    tim->CCR1  = 0;               /* 从 0% duty 开始*/
    tim->EGR   = 1u;              /* 强制更新*/
    tim->SR    = 0;
    tim->CR1  |= TIM_CR1_CEN;
}

void timer_pwm_set_duty(TIM_TypeDef *tim, uint8_t duty_percent)
{
    if (duty_percent > 100) duty_percent = 100;
    tim->CCR1 = (duty_percent * (tim->ARR + 1)) / 100u;
}

/* ============================================================
 * 任务 4 — 输入捕获 PWM 测量
 * ============================================================ */

void timer_input_capture_init(TIM_TypeDef *tim, uint16_t psc)
{
    tim->CR1 &= ~TIM_CR1_CEN;
    tim->PSC  = psc;
    tim->ARR  = 0xFFFFu;

    /* CC1S=01：IC1映射到TI1（周期的上升沿）
       CC2S=10：IC2 映射到 TI1（高电平时间为下降沿）*/
    tim->CCMR1 = (0b01u << 0) | (0b10u << 8);  /* CC1S | CC2S*/

    /* CC1P=0（上升），CC2P=1（下降）*/
    tim->CCER  = (1u << 0) | (1u << 4) | (1u << 5);

    tim->CR1  |= TIM_CR1_CEN;
}

/* ============================================================
 * 任务 5 — SysTick
 * ============================================================ */

typedef struct { volatile uint32_t CTRL, LOAD, VAL, CALIB; } SysTick_TypeDef;
static SysTick_TypeDef _SysTick = {0};
SysTick_TypeDef *SysTick = &_SysTick;
#define SYSTICK_CTRL_ENABLE    (1u << 0)
#define SYSTICK_CTRL_TICKINT   (1u << 1)
#define SYSTICK_CTRL_CLKSOURCE (1u << 2)

volatile uint32_t g_tick_ms = 0;

void systick_init(uint32_t cpu_hz, uint32_t tick_hz)
{
    SysTick->CTRL  = 0;
    SysTick->LOAD  = (cpu_hz / tick_hz) - 1u;
    SysTick->VAL   = 0;
    SysTick->CTRL  = SYSTICK_CTRL_ENABLE | SYSTICK_CTRL_TICKINT | SYSTICK_CTRL_CLKSOURCE;
}

void SysTick_Handler(void) { g_tick_ms++; }

uint32_t get_tick_ms(void) { return g_tick_ms; }

void delay_ms(uint32_t ms)
{
    uint32_t start = get_tick_ms();
    while ((get_tick_ms() - start) < ms) {}
}

/* ============================================================
 * 任务 6 — 找错题 已修复
 *
 * 错误 1：PSC = 84000000 / 1000 = 84000 — 但 PSC 是 16 位（最大 65535）。
 *        产生错误的预分频器（循环），定时器以错误的频率运行。
 *        修复：使用 PSC=839、ARR=99999（如果 ARR 是 32 位，如 TIM2/TIM5）
 *             或 PSC=8399，ARR=9999。
 *
 * 缺陷 2：CCER CC1E 位未设置 — 通道输出被禁用。
 *        PWM 信号是内部生成的，但从未到达pin。
 *        修复：蒂姆->CCER |= TIM_CCER_CC1E；
 *
 * 错误 3：CCR1 = arr（始终为 100% duty）。
 *        PWM 模式 1：当 CNT < CCR1 时输出高电平。
 *        如果 CCR1 = ARR，则几乎所有 个 Tick = ~100% duty 的输出均为高电平。
 *        修复：对于 50% duty，CCR1 = arr / 2。或者使用timer_pwm_set_duty()。
 * ============================================================ */

int main(void)
{
    uint16_t psc; uint32_t arr;

    /* 84 MHz 定时器 → 1 Hz*/
    timer_calc_psc_arr(84000000, 1, &psc, &arr);
    assert((uint32_t)(psc + 1) * (arr + 1) == 84000000u);

    /* 84 MHz 定时器 → 1000 Hz*/
    timer_calc_psc_arr(84000000, 1000, &psc, &arr);
    assert((uint32_t)(psc + 1) * (arr + 1) == 84000u);

    /* PWM 初始化和任务*/
    timer_pwm_init(TIM2, 839, 99999);
    assert(TIM2->CR1 & TIM_CR1_CEN);
    assert(TIM2->CCER & TIM_CCER_CC1E);

    timer_pwm_set_duty(TIM2, 50);
    assert(TIM2->CCR1 == 50000u);

    timer_pwm_set_duty(TIM2, 0);
    assert(TIM2->CCR1 == 0u);

    timer_pwm_set_duty(TIM2, 100);
    assert(TIM2->CCR1 == 100000u);

    printf("All timer/PWM answers verified.\n");
    return 0;
}

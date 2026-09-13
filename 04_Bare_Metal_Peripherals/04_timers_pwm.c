/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * 主题：定时器和 PWM — 硬件定时器配置
 * 文件：04_Bare_Metal_Peripherals/04_timers_pwm.c
 * ============================================================
 *
 * 定时器是嵌入式系统的支柱：
 * PWM，输入捕捉、输出比较、时基、看门狗。
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/* ============================================================
 * 理论 — STM32 通用定时器 (TIM2–TIM5)
 * ============================================================
 *
 * 定时器每 N 个时钟周期递增其计数器 CNT，
 * 其中 N 由预分频器 (PSC) 设置。
 *
 * 定时器时钟频率：FCLK（经过APB预分频器）
 * 计数器时钟：FCLK/(PSC+1)
 * 更新事件 (UEV)：当 CNT 达到 ARR 时发生（自动重新加载）
 *
 * 公式：
 *   更新频率 = FCLK / ((PSC+1) * (ARR+1))
 *   周期（秒）= 1 / Update_frequency
 *
 * 示例：84 MHz 定时器时钟的 1 ms Tick
 *   84,000,000 / ((839+1) * (99+1)) = 1000 Hz → 1 ms 周期
 *   PSC = 839，ARR = 99
 *
 * 关键寄存器：
 *   CR1 : CEN (bit0) — 计数器使能，DIR (bit4) — 向上/向下
 *   PSC ：预分频器值（PSC+1 对输入时钟进行分频）
 *   ARR ：自动重载寄存器（计数器在 ARR 处回绕）
 *   CNT：当前计数器值
 *   CCR1-4：捕捉/比较寄存器（用于PWM或输入捕捉）
 *   CCMR1：通道 1/2 模式（OC1M 位 [6:4] 用于PWM 模式）
 *   CCER：捕捉/比较使能
 *   DIER ：中断/DMA 使能（UIE bit0 = 更新中断）
 *   SR : 状态 (UIF bit0 = 更新中断标志)
 *   EGR：事件生成（UG bit0 = 强制更新）
 *
 * PWM 模式 1：CNT < CCRx → 输出高电平
 *             CNT >= CCRx → 输出低电平
 *   占空比 = CCRx / (ARR+1) × 100%
 * ============================================================ */

typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SMCR;
    volatile uint32_t DIER;
    volatile uint32_t SR;
    volatile uint32_t EGR;
    volatile uint32_t CCMR1;
    volatile uint32_t CCMR2;
    volatile uint32_t CCER;
    volatile uint32_t CNT;
    volatile uint32_t PSC;
    volatile uint32_t ARR;
    volatile uint32_t RCR;
    volatile uint32_t CCR1;
    volatile uint32_t CCR2;
    volatile uint32_t CCR3;
    volatile uint32_t CCR4;
    volatile uint32_t BDTR;
} TIM_TypeDef;

/* CR1位*/
#define TIM_CR1_CEN   (1u << 0)
#define TIM_CR1_UDIS  (1u << 1)
#define TIM_CR1_URS   (1u << 2)
#define TIM_CR1_ARPE  (1u << 7)   /* 自动重新加载预加载启用*/

/* 迪尔钻头*/
#define TIM_DIER_UIE   (1u << 0)
#define TIM_DIER_CC1IE (1u << 1)
#define TIM_DIER_CC2IE (1u << 2)

/* SR位*/
#define TIM_SR_UIF   (1u << 0)
#define TIM_SR_CC1IF (1u << 1)

/* CCER 位*/
#define TIM_CCER_CC1E  (1u << 0)
#define TIM_CCER_CC1P  (1u << 1)
#define TIM_CCER_CC2E  (1u << 4)

/* EGR位*/
#define TIM_EGR_UG   (1u << 0)

/* CCMR OC 模式位（在 OC 模式下，每个半字节的位 [6:4]）*/
#define TIM_CCMR1_OC1M_PWM1  (0b110u << 4)
#define TIM_CCMR1_OC1M_PWM2  (0b111u << 4)
#define TIM_CCMR1_OC1PE      (1u << 3)   /* OC1 预载使能*/
#define TIM_CCMR1_CC1S_OUT   (0b00u << 0)

static TIM_TypeDef _TIM2 = {0};
static TIM_TypeDef _TIM3 = {0};
TIM_TypeDef *TIM2 = &_TIM2;
TIM_TypeDef *TIM3 = &_TIM3;

/* ============================================================
 * 任务 1 — PSC 和 ARR 计算
 * ============================================================ */

void timer_calc_psc_arr(uint32_t fclk_hz, uint32_t desired_freq_hz,
                        uint16_t *psc_out, uint16_t *arr_out)
{
    /* TODO：找到 PSC 和 ARR，使得：
     *   fclk_hz / ((PSC+1) * (ARR+1)) = desired_freq_hz
     *
     * 方法：尽量保持ARR接近999（分辨率的小数点后3位）
     * PSC = fclk_hz / (desired_freq_hz * (ARR+1)) - 1
     *
     * 提示：从 ARR=999 开始，计算 PSC。
     * 如果 PSC > 65535，则增加 ARR。*/
    (void)fclk_hz; (void)desired_freq_hz; (void)psc_out; (void)arr_out;
}

/* ============================================================
 * 任务 2 — 基本定时器（时基）初始化
 * ============================================================ */

void timer_init_timebase(TIM_TypeDef *tim, uint16_t psc, uint32_t arr,
                         uint8_t irq_enable)
{
    /* TODO：首先禁用定时器*/
    /* TODO：设置 PSC = psc*/
    /* TODO：设置 ARR = arr*/
    /* TODO：如果irq_enable：DIER |= UIE*/
    /* TODO：设置 ARPE（预加载 ARR 更新）*/
    /* TODO：生成更新事件（EGR |= UG）以立即加载 PSC 和 ARR*/
    /* TODO：使能定时器（CR1 |= CEN）*/
    (void)tim; (void)psc; (void)arr; (void)irq_enable;
}

/* ============================================================
 * 任务 3 — PWM 输出配置（通道 1）
 *
 * 将 TIM 通道 1 配置为PWM 模式 1 输出。
 * 此后，改变 CCR1 会改变占空比。
 * ============================================================ */

void timer_pwm_init(TIM_TypeDef *tim, uint16_t psc, uint32_t arr,
                    uint32_t ccr1_initial)
{
    /* TODO：设置所需频率的 PSC 和 ARR*/
    /* TODO：配置CCMR1：
     *   CC1S = 00（输出比较模式）
     *   OC1M = 110（PWM模式1）
     *   OC1PE = 1（预加载启用 - 占空比在下次更新时生效）*/
    /* TODO：配置CCER：CC1E = 1（使能通道1输出）*/
    /* TODO：设置 CCR1 = ccr1_initial*/
    /* TODO：启用ARPE，生成UG，启用CEN*/
    (void)tim; (void)psc; (void)arr; (void)ccr1_initial;
}

void timer_pwm_set_duty(TIM_TypeDef *tim, uint8_t duty_percent)
{
    /* TODO：CCR1 = (duty_percent * (时间->ARR + 1)) / 100
     * 注意：设置 OC1PE 后，这将在下一次更新事件时生效*/
    (void)tim; (void)duty_percent;
}

/* ============================================================
 * 任务 4 — 输入捕获（测量脉冲宽度或频率）
 *
 * 配置通道 1 在上升沿捕获，
 * 通道 2 在下降沿捕获。
 * 脉冲宽度 = CCR2 - CCR1（在定时器个 Tick 中）。
 * ============================================================ */

void timer_input_capture_init(TIM_TypeDef *tim, uint16_t psc)
{
    /* TODO：设置 PSC 以获得所需的定时器分辨率
     * TODO：ARR = 0xFFFF（最大值 — 脉冲期间不回绕）
     * TODO：CCMR1：
     *   CC1S = 01（通道1输入，映射到TI1）
     *   CC2S = 10（通道 2 输入，映射到反相 TI1）
     * TODO：CCER：
     *   CC1P = 0（上升沿），CC1E = 1
     *   CC2P = 1（下降沿），CC2E = 1
     * TODO：SMCR = 0（无从模式 — free 运行）
     * TODO：DIER：CC1IE = 1 在上升沿捕获时触发ISR
     * TODO：启用定时器*/
    (void)tim; (void)psc;
}

uint32_t timer_get_pulse_width_us(TIM_TypeDef *tim, uint32_t fclk_hz)
{
    /* TODO: 个 Tick = tim->CCR2 - tim->CCR1（如果 CCR2 < CCR1 则处理换行）
     * TODO: 返回 个 刻度 * 1_000_000 / (fclk_hz / (tim->PSC + 1))*/
    (void)tim; (void)fclk_hz;
    return 0;
}

/* ============================================================
 * 任务 5 — SysTick 定时器（Cortex-M 内核定时器）
 *
 * SysTick 始终存在于 Cortex-M 上。
 * 24 位递减计数器，从 LOAD 寄存器重新加载。
 * 用于 OS Tick (FreeRTOS) 或简单延迟。
 * ============================================================ */

typedef struct {
    volatile uint32_t CTRL;   /* bit0=启用，bit1=TickINT，bit2=CLKSOURCE，bit16=COUNTFLAG*/
    volatile uint32_t LOAD;
    volatile uint32_t VAL;    /* 当前count（写入任意值清除）*/
    volatile uint32_t CALIB;
} SysTick_TypeDef;

#define SYSTICK_CTRL_ENABLE    (1u << 0)
#define SYSTICK_CTRL_TICKINT   (1u << 1)
#define SYSTICK_CTRL_CLKSOURCE (1u << 2)
#define SYSTICK_CTRL_COUNTFLAG (1u << 16)

static SysTick_TypeDef _SysTick = {0};
SysTick_TypeDef *SysTick = &_SysTick;

void systick_init_ms(uint32_t fclk_hz, uint8_t irq_enable)
{
    /* TODO：LOAD = (fclk_hz / 1000) - 1 → 每 1 毫秒触发一次*/
    /* TODO: VAL = 0 → 清除电流 count*/
    /* TODO：CTRL：CLKSOURCE=1（核心时钟），TickINT=irq_enable，ENABLE=1*/
    (void)fclk_hz; (void)irq_enable;
}

/* ============================================================
 * 任务 6 — 找错题：PWM 初始化错误
 *
 * 下面的代码将 1 kHz PWM 配置为 50% duty。
 * 定时器时钟 = 84 MHz。它有 3 个错误。
 * ============================================================ */

void pwm_init_BUGGY(TIM_TypeDef *tim)
{
    uint32_t fclk = 84000000;
    uint32_t pwm_freq = 1000;
    uint32_t arr = 999;

    /* Bug 1：PSC计算错误*/
    /* 正确：PSC = fclk / (pwm_freq * (arr+1)) - 1 = 84000000 / 1000000 - 1 = 83*/
    tim->PSC = fclk / pwm_freq;   /* 给出 84000——太大了*/

    tim->ARR = arr;

    /* Bug 2：在启用 OC 模式之前，CC1S 未设置为输出 (00)*/
    tim->CCMR1 = TIM_CCMR1_OC1M_PWM1 | TIM_CCMR1_OC1PE;
    /* 缺少： | TIM_CCMR1_CC1S_OUT — 但 00 已经是 0，所以这实际上没问题。
     * 真正的错误 2：此版本中缺少 OC1PE — 占空比无法完全更新*/

    /* 实际Bug 2：通道输出未启用*/
    /* 缺失：tim->CCER |= TIM_CCER_CC1E；*/

    /* 错误 3：50% duty 应该是 CCR1 = (ARR+1)/2 = 500*/
    tim->CCR1 = arr;   /* 这是 100% duty，而不是 50%*/

    tim->EGR |= TIM_EGR_UG;
    tim->CR1 |= TIM_CR1_CEN;
}

/* ============================================================
 * SELF-TEST
 * ============================================================ */

int main(void)
{
    uint16_t psc = 0, arr = 0;
    timer_calc_psc_arr(84000000, 1000, &psc, &arr);
    /* 当 ARR=999 时：PSC = 84000000/(1000*1000)-1 = 83*/
    assert(psc == 83 || psc == 83);
    assert(arr == 999);

    printf("All Timer tests PASSED.\n");
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1：定时器时钟为 84 MHz。你需要 50 Hz PWM 1000 步
 *     的分辨率。计算 PSC 和 ARR。
 *     答案：TODO
 *
 * Q2：PWM模式1和PWM模式2有什么区别？
 *     答案：TODO
 *
 * Q3：更改时为什么要设置OC1PE（预载使能）
 *     动态占空比？
 *     答案：TODO
 *
 * Q4：声纳测距仪需要一个 1 µs 定时器 Tick。
 *     定时器时钟 = 72 MHz。你使用什么 PSC 值？
 *     答案：TODO
 *
 * Q5：STM32F4上的TIM2和TIM6有什么区别？
 *     你什么时候会选择每一个？
 *     答案：TODO
 */

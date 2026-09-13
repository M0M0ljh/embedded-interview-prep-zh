/*
 * 答案 — 01_bit_manipulation.c
 * 在尝试完所有TODO之前，请勿打开。
 */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/* ── 任务一────────────────────────────────────────────────────*/

void gpio_set_pin(uint16_t *reg, uint8_t pin)   { *reg |=  (uint16_t)(1u << pin); }
void gpio_clear_pin(uint16_t *reg, uint8_t pin) { *reg &= ~(uint16_t)(1u << pin); }
void gpio_toggle_pin(uint16_t *reg, uint8_t pin){ *reg ^=  (uint16_t)(1u << pin); }
uint8_t gpio_read_pin(uint16_t reg, uint8_t pin){ return (reg >> pin) & 1u; }

/* ── 任务二────────────────────────────────────────────────────*/

uint8_t can_get_tx_count(uint32_t r)    { return (uint8_t)((r >>  0) & 0xFF); }
uint8_t can_get_rx_count(uint32_t r)    { return (uint8_t)((r >>  8) & 0xFF); }
uint8_t can_get_error_counter(uint32_t r){ return (uint8_t)((r >> 16) & 0xFF); }

uint32_t can_set_tx_count(uint32_t reg, uint8_t count)
{
    return (reg & ~0x000000FFu) | ((uint32_t)count & 0xFF);
}

/* ── 任务3────────────────────────────────────────────────────*/

int is_little_endian(void)
{
    uint16_t x = 0x0001;
    return *(uint8_t*)&x == 0x01;   /* 1 如果 小端序*/
}

uint16_t swap16(uint16_t x)
{
    return (uint16_t)((x >> 8) | (x << 8));
}

uint32_t swap32(uint32_t x)
{
    return ((x & 0x000000FFu) << 24) |
           ((x & 0x0000FF00u) <<  8) |
           ((x & 0x00FF0000u) >>  8) |
           ((x & 0xFF000000u) >> 24);
}

/* ── 任务4────────────────────────────────────────────────────*/

uint8_t count_set_bits(uint32_t x)
{
    uint8_t count = 0;
    while (x) { x &= x - 1; count++; }   /* 布赖恩·科尼汉*/
    return count;
}

int8_t find_highest_set_bit(uint32_t x)
{
    if (x == 0) return -1;
    int8_t pos = 0;
    while (x >>= 1) pos++;
    return pos;
}

uint32_t reverse_bits(uint32_t x)
{
    uint32_t result = 0;
    for (int i = 0; i < 32; i++) {
        result = (result << 1) | (x & 1u);
        x >>= 1;
    }
    return result;
}

/* ── 任务5────────────────────────────────────────────────────*/

#define USART_CR1_UE    (1u << 13)
#define USART_CR1_M     (1u << 12)
#define USART_CR1_PCE   (1u << 10)
#define USART_CR1_PS    (1u <<  9)

typedef struct {
    volatile uint32_t CR1, CR2, CR3, BRR, SR, DR;
} USART_TypeDef;

void usart_enable(USART_TypeDef *u)         { u->CR1 |= USART_CR1_UE; }
void usart_configure_8n1(USART_TypeDef *u)  { u->CR1 &= ~(USART_CR1_M | USART_CR1_PCE | USART_CR1_PS); }
void usart_configure_8e1(USART_TypeDef *u)  { u->CR1 = (u->CR1 & ~(USART_CR1_M | USART_CR1_PS)) | USART_CR1_PCE; }
void usart_configure_9o1(USART_TypeDef *u)  { u->CR1 |= USART_CR1_M | USART_CR1_PCE | USART_CR1_PS; }

/* ── 任务 6 — 找错题 答案 ────────────────────────────────*/
/*
 * Bug 1: reg = reg & 掩码；
 *        这仅保留 field 位 — 应该保留 CLEAR。
 *        修复：reg = reg & ~掩码；   （面具的补充）
 *
 * 错误 2：uint16_t field = val << 移位；
 *        val 是uint8_t（8 位）。对于 8 位类型，移位 左移 12 为 UB。
 *        修复：uint16_tfield=（uint16_t）（（uint16_t）val<<移位）；
 *             （首先将 val 转换为 uint16_t，然后移位）
 *
 * 错误 3：返回 reg | field；
 *        此时reg仍然有旧的field位，因为Bug 1是错误的。
 *        修复Bug 1后，reg被正确屏蔽，这个OR是正确的。
 *        但是：掩码补码逻辑必须位于OR之前。
 *
 * 更正的功能：
 *   uint16_t set_field(uint16_t reg, uint8_t val) {
 *       uint16_t掩码=0xF000;
 *       uint8_t移位 = 12；
 *       reg &= ~面具；                              //清除field
 *       return reg | ((uint16_t)((uint16_t)val << shift) & mask);
 *   }
 */

/* ── 面试问题解答────────────────────────────────*/
/*
 * Q1：在大多数平台上，(1<<31) 为 signed int 移位 — UB。
 *     (1u<<31) 是unsigned — 定义的行为，给出0x80000000。
 *
 * Q2：在 Cortex-M 上，使用 LDREX/STREX（独占访问）或禁用中断
 *     __disable_irq()/__enable_irq() 围绕读-改-写。
 *     许多STM32外设也有BSRR寄存器：atomic设置/清除
 *     一次写入 — 无需读-改-写。
 *
 * Q3: reg &= ~0x0Fu;   （清除位 3:0，保留所有其他位）
 *
 * Q4：位带将每个位映射到字对齐地址，从而形成 32 位
 *     写 = 单比特切换。消除读-改-写，所以它是
 *     从ISR角度来看，本质上是atomic。在 Cortex-M7+ 上已弃用。
 *
 * Q5：在编译时使用 a 预计算 256 个条目（每个字节值一个）
 *     每个输入字节的constexpr/const表，然后XOR表[字节]。
 *     总计 ROM：256 × 2 字节 = 512 字节（CRC-16）。
 */

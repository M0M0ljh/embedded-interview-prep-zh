/*
 * 答案：04_Bare_Metal_Peripherals/02_uart_bare_metal.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

typedef struct {
    volatile uint32_t SR, DR, BRR, CR1, CR2, CR3;
} USART_TypeDef;

#define USART_SR_TXE    (1u << 7)
#define USART_SR_TC     (1u << 6)
#define USART_SR_RXNE   (1u << 5)
#define USART_SR_ORE    (1u << 3)
#define USART_SR_FE     (1u << 1)
#define USART_CR1_UE    (1u << 13)
#define USART_CR1_M     (1u << 12)
#define USART_CR1_PCE   (1u << 10)
#define USART_CR1_PS    (1u << 9)
#define USART_CR1_TXEIE (1u << 7)
#define USART_CR1_RXNEIE (1u << 5)
#define USART_CR1_TE    (1u << 3)
#define USART_CR1_RE    (1u << 2)

static USART_TypeDef _USART1={0}, _USART2={0};
USART_TypeDef *USART1=&_USART1, *USART2=&_USART2;

/* ============================================================ TASK 1 */

uint32_t uart_calc_brr(uint32_t fclk_hz, uint32_t baud, uint8_t over8)
{
    uint32_t div = over8 ? (8u * baud) : (16u * baud);
    return (fclk_hz + div/2) / div;   /* 四舍五入到最接近的*/
}

/* ============================================================ TASK 2 */

typedef enum { UART_PARITY_NONE=0, UART_PARITY_EVEN, UART_PARITY_ODD } UartParity;
typedef struct {
    uint32_t fclk_hz, baud;
    uint8_t word_len_9bit, stop_bits_2;
    UartParity parity;
    uint8_t rx_irq_enable, tx_irq_enable;
} UartConfig;

void uart_init(USART_TypeDef *usart, const UartConfig *cfg)
{
    usart->CR1 &= ~USART_CR1_UE;           /* 第 1 步：禁用*/

    usart->BRR = uart_calc_brr(cfg->fclk_hz, cfg->baud, 0);  /* 第 2 步：BRR*/

    uint32_t cr1 = USART_CR1_TE | USART_CR1_RE;
    if (cfg->word_len_9bit)     cr1 |= USART_CR1_M;
    if (cfg->parity != UART_PARITY_NONE) {
        cr1 |= USART_CR1_PCE;
        if (cfg->parity == UART_PARITY_ODD) cr1 |= USART_CR1_PS;
    }
    if (cfg->rx_irq_enable)     cr1 |= USART_CR1_RXNEIE;
    if (cfg->tx_irq_enable)     cr1 |= USART_CR1_TXEIE;
    usart->CR1 = cr1;                        /* 步骤3：CR1*/

    /* 步骤4：CR2停止位[13:12]*/
    if (cfg->stop_bits_2)
        usart->CR2 = (usart->CR2 & ~(0b11u << 12)) | (0b10u << 12);

    usart->CR1 |= USART_CR1_UE;             /* 第 5 步：启用 LAST*/
}

/* ============================================================ TASK 3 */

void uart_send_byte_blocking(USART_TypeDef *usart, uint8_t byte)
{
    while (!(usart->SR & USART_SR_TXE)) {}  /* 等待TX寄存器为空*/
    usart->DR = byte;
}

void uart_send_string_blocking(USART_TypeDef *usart, const char *str)
{
    while (*str) uart_send_byte_blocking(usart, (uint8_t)*str++);
}

void uart_send_buffer_blocking(USART_TypeDef *usart, const uint8_t *buf, uint16_t len)
{
    while (len--) uart_send_byte_blocking(usart, *buf++);
}

/* ============================================================ TASK 4 */

#define TX_BUF_SIZE  256u
#define TX_BUF_MASK  (TX_BUF_SIZE - 1)

typedef struct {
    uint8_t  buf[TX_BUF_SIZE];
    volatile uint8_t head, tail;
} TxRingBuf;
static TxRingBuf g_tx_buf = {0};

int uart_send_async(USART_TypeDef *usart, const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        uint8_t next = (g_tx_buf.head + 1u) & TX_BUF_MASK;
        if (next == g_tx_buf.tail) return -1;   /* 缓冲区已满*/
        g_tx_buf.buf[g_tx_buf.head] = data[i];
        g_tx_buf.head = next;
    }
    usart->CR1 |= USART_CR1_TXEIE;   /* 使能TXE中断*/
    return 0;
}

void USART_TXE_IRQHandler(USART_TypeDef *usart)
{
    if (g_tx_buf.head != g_tx_buf.tail) {
        usart->DR = g_tx_buf.buf[g_tx_buf.tail];
        g_tx_buf.tail = (g_tx_buf.tail + 1u) & TX_BUF_MASK;
    } else {
        usart->CR1 &= ~USART_CR1_TXEIE;   /* 没有更多数据——禁用中断*/
    }
}

/* ============================================================ TASK 5 */

typedef enum { UART_RX_OK=0, UART_RX_OVERRUN=1, UART_RX_FRAMING=2 } UartRxStatus;

UartRxStatus uart_receive_byte(USART_TypeDef *usart, uint8_t *out)
{
    if (usart->SR & USART_SR_ORE) {
        (void)usart->SR; (void)usart->DR;   /* 清除 ORE：读取 SR，然后读取 DR*/
        return UART_RX_OVERRUN;
    }
    if (usart->SR & USART_SR_FE) {
        (void)usart->SR; (void)usart->DR;
        return UART_RX_FRAMING;
    }
    if (usart->SR & USART_SR_RXNE) {
        *out = (uint8_t)usart->DR;
        return UART_RX_OK;
    }
    *out = 0;
    return UART_RX_OK;
}

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1：UART 显示乱码 — 首先要检查 3 件事。

A: 1. 波特率不匹配：即使1% deviation也会导致高波特率下的帧错误。
      验证 BRR = FCLK / (16 × BAUD)。检查示波器：测量实际位时间。
   2. 电压电平不匹配：3.3V MCU 与 5V 设备通信（反之亦然）。没有
      电平移位，可能未达到逻辑高阈值。
   3. UART 配置不匹配：8N1 与 8E1，1 个停止位与 2 个。一侧使用
      奇偶校验，而其他奇偶校验则没有→每个字节看起来都已损坏。

问题 2：TXE 与 TC — 何时分别使用。

A：TXE（发送数据寄存器为空，SR 位 7）：当 移位 寄存器
   已加载 DR，并且 DR 再次为空 — 准备好接受下一个字节。使用 TXE 来
   连续填充 DR 以获得最大吞吐量。
   TC（传输完成，SR 位 6）：当 LAST 位已被移位ed 时触发
   在 TX 线上AND TXE 已设置。信号线闲置。
   将 TC 用于 RS-485 DE（方向启用）pin：你不得置低 DE，直到
   TC 会触发——否则你会在传输过程中切断最后的比特。

Q3: 为什么在发送功能中启用 TXEIE，而完成后在ISR 中禁用？

答：当 TXE=1 时，TXEIE（TX 空中断使能）会连续触发long
   （DR 为空）。如果我们永久启用它，ISR将发射数千个
   即使没有什么可发送的，每秒也会发送多次 — 浪费 CPU。
   图案：
   - uart_send()：推送到环形缓冲区，然后|= TXEIE。 ISR 立即点火。
   - ISR：如果环形缓冲区有数据→写入DR。如果为空→清除TXEIE。完毕。
   这是"interrupt-driven TX with auto-disable" 模式。高效的。

Q4：RS-485 与UART。 GPIO pin 和计时。

A: RS-485 需要一个 DE (Driver Enable) pin 来切换发送和接收
   （半双工 巴士）。将 MCU GPIO 连接到 RS-485 收发器的 DE/RE# 引脚。
   发射前：GPIO 高（启用发射器，禁用接收器）。
   发送后：等待TC（传输完成）标志，然后GPIO低
   （禁用发射器，启用接收器）。
   必须使用 TC 标志，而不是 TXE — 当 DR 为空但最后一个字节为 TXE 时触发
   仍在移位寄存器中。过早切换 DE 会切断最后的位。

Q5：115200 波特时的位时间和帧持续时间。 ISR 延迟限制。

答：位时间 = 1 / 115200 = 8.68 µs。
   8N1 帧 = 1 个起始 + 8 个数据 + 1 个停止 = 10 位 = 每字节 86.8 µs。
   在 115200 波特率下，每 86.8 µs 到达一个新字节。
   ISR 延迟限制：ISR 必须先读取 DR 寄存器（清除 RXNE）
   下一个字节开始到达并溢出 1 字节硬件缓冲区。
   最大ISR延迟：86.8 µs（一帧时间）。
   在 168 MHz 时：86.8 µs = ~14,600 CPU 周期 — 非常慷慨。即使是慢速的ISR也可以。
   在 921600 波特率下：帧 = 10.8 µs → ~1,815 个周期。 ISR一定要快。
*/

int main(void)
{
    assert(uart_calc_brr(84000000, 115200, 0) == 45);
    assert(uart_calc_brr(16000000, 9600,   0) == 104);
    assert(uart_calc_brr(72000000, 115200, 0) == 39);

    /* 模拟TXE驱动的发送*/
    USART2->SR = USART_SR_TXE;  /* TX 空*/
    uint8_t msg[] = "Hi";
    uart_send_async(USART2, msg, 2);
    USART_TXE_IRQHandler(USART2);
    assert(USART2->DR == 'H');
    USART_TXE_IRQHandler(USART2);
    assert(USART2->DR == 'i');
    USART_TXE_IRQHandler(USART2);
    assert(!(USART2->CR1 & USART_CR1_TXEIE));  /* 缓冲区空后禁用*/

    printf("All UART answers verified.\n");
    return 0;
}

/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * 主题 : UART — 裸机 实现
 * 文件：04_Bare_Metal_Peripherals/02_uart_bare_metal.c
 * ============================================================
 *
 * 在 每场嵌入式面试 中询问UART。
 * 了解寄存器、帧、波特率公式，
 * 以及如何编写非阻塞驱动程序。
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * 理论 — UART 基础知识
 * ============================================================
 *
 * UART frame (8N1):
 *   [空闲][启动][D0][D1][D2][D3][D4][D5][D6][D7][停止][空闲]
 *   - IDLE = 线路高电平（逻辑 1）
 *   - 起始位 = 逻辑 0（1 位）
 *   - 8 个数据位，LSB 首先
 *   - 停止位 = 逻辑 1（1 或 2 位）
 *   - 无奇偶校验 (8N1)，或偶/奇奇偶校验 (8E1/8O1)
 *
 * 波特率公式（STM32USART）：
 *   BRR = FCLK / (16 * BAUD) — 过采样 16（默认）
 *   BRR = FCLK / (8 * BAUD) — 8 倍过采样（OVER8 位设置）
 *
 * 示例：FCLK = 84 MHz，波特率 = 115200
 *   BRR = 84_000_000 / (16 * 115200) = 45.57 → 0x002D.09，格式为 STM32
 *
 * STM32 USART 关键寄存器：
 *   SR (0x00)：状态 — TXE(bit7)、TC(bit6)、RXNE(bit5)、ORE(bit3)
 *   DR (0x04)：数据 — 读=接收，写=发送
 *   BRR (0x08)：波特率寄存器
 *   CR1 (0x0C)：控制 1 — UE(位 13)、M(位 12)、PCE(位 10)、PS(位 9)、
 *                            TXEIE(位7)、TCIE(位6)、RXNEIE(位5)、
 *                            TE（位3），RE（位2）
 *   CR2 (0x10)：控制 2 — 停止位 [13:12]：00=1 位，10=2 位
 *   CR3 (0x14)：控制 3 — DMAT（位 7）、DMAR（位 6）、HDSEL（位 3）
 * ============================================================ */

/* 模拟USART寄存器*/
typedef struct {
    volatile uint32_t SR;
    volatile uint32_t DR;
    volatile uint32_t BRR;
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t CR3;
} USART_TypeDef;

/* SR位*/
#define USART_SR_TXE    (1u << 7)   /* TX 寄存器为空 — 准备写入*/
#define USART_SR_TC     (1u << 6)   /* 传输完成*/
#define USART_SR_RXNE   (1u << 5)   /* RX 不为空 — 数据可用*/
#define USART_SR_ORE    (1u << 3)   /* 溢出错误*/
#define USART_SR_FE     (1u << 1)   /* 成帧错误*/

/* CR1位*/
#define USART_CR1_UE      (1u << 13)
#define USART_CR1_M       (1u << 12)
#define USART_CR1_PCE     (1u << 10)
#define USART_CR1_PS      (1u << 9)
#define USART_CR1_TXEIE   (1u << 7)
#define USART_CR1_TCIE    (1u << 6)
#define USART_CR1_RXNEIE  (1u << 5)
#define USART_CR1_TE      (1u << 3)
#define USART_CR1_RE      (1u << 2)

static USART_TypeDef _USART1 = {0};
static USART_TypeDef _USART2 = {0};
USART_TypeDef *USART1 = &_USART1;
USART_TypeDef *USART2 = &_USART2;

/* ============================================================
 * 任务 1 — 波特率计算
 * ============================================================ */

uint32_t uart_calc_brr(uint32_t fclk_hz, uint32_t baud, uint8_t over8)
{
    /* TODO：如果 over8 == 0：BRR = fclk_hz / (16 * 波特率)
     *       如果 over8 == 1: BRR = fclk_hz / (8 * 波特率)
     * 四舍五入到最接近的整数。
     * 注：STM32 BRR 存储尾数[15:4]和小数[3:0]
     *       但对于本练习，只需 返回 四舍五入的整数。*/
    (void)fclk_hz; (void)baud; (void)over8;
    return 0;
}

/* ============================================================
 * 任务 2 — USART 外设初始化
 * ============================================================ */

typedef enum {
    UART_PARITY_NONE = 0,
    UART_PARITY_EVEN,
    UART_PARITY_ODD
} UartParity;

typedef struct {
    uint32_t    fclk_hz;
    uint32_t    baud;
    uint8_t     word_len_9bit;   /* 0=8位，1=9位*/
    uint8_t     stop_bits_2;     /* 0=1 个停止位，1=2 个停止位*/
    UartParity  parity;
    uint8_t     rx_irq_enable;
    uint8_t     tx_irq_enable;
} UartConfig;

void uart_init(USART_TypeDef *usart, const UartConfig *cfg)
{
    /* TODO：第 1 步 — 在配置之前禁用 USART（清除 UE）*/
    /* TODO：步骤 2 — 设置 BRR*/
    /* TODO：步骤 3 — 配置 CR1：
     *   字长（M 位）、奇偶校验（PCE、PS）、TX/RX 使能（TE、RE）
     *   RXNEIE 如果 cfg->rx_irq_enable
     *   TXEIE 如果 cfg->tx_irq_enable*/
    /* TODO：步骤 4 — 配置 CR2 停止位 [13:12]*/
    /* TODO：步骤 5 — 启用 USART（设置 UE）*/
    (void)usart; (void)cfg;
}

/* ============================================================
 * 任务 3 — 阻止传输（轮询）
 *
 * 用于调试输出。适合启动消息。
 * 请勿在生产中使用 ISR 驱动的代码。
 * ============================================================ */

void uart_send_byte_blocking(USART_TypeDef *usart, uint8_t byte)
{
    /* TODO：等待 TXE 置位（SR 位 7）
     * 将字节写入 usart->DR*/
    (void)usart; (void)byte;
}

void uart_send_string_blocking(USART_TypeDef *usart, const char *str)
{
    /* TODO：发送每个字符直到“\0”*/
    (void)usart; (void)str;
}

void uart_send_buffer_blocking(USART_TypeDef *usart, const uint8_t *buf, uint16_t len)
{
    /* TODO：发送 len 个字节*/
    (void)usart; (void)buf; (void)len;
}

/* ============================================================
 * 任务 4 — 使用TX-空中断进行非阻塞传输
 *
 * 驱动程序维护一个TX环形缓冲区。
 * uart_send_async() 将数据放入环形缓冲区并启用 TXEIE。
 * USART_TXE_IRQHandler() 每个中断发送一个字节；当缓冲
 * 为空，则禁用 TXEIE。
 * ============================================================ */

#define TX_BUF_SIZE  256u
#define TX_BUF_MASK  (TX_BUF_SIZE - 1)

typedef struct {
    uint8_t  buf[TX_BUF_SIZE];
    volatile uint8_t head;   /* 作者：uart_send_async*/
    volatile uint8_t tail;   /* 作者：ISR*/
} TxRingBuf;

static TxRingBuf g_tx_buf = {0};

int uart_send_async(USART_TypeDef *usart, const uint8_t *data, uint16_t len)
{
    /* TODO：将每个字节推入g_tx_buf环形缓冲区
     * 如果缓冲区已满：返回 -1（丢弃数据）
     * 推送后：启用 TXEIE，以便 ISR 触发
     * 成功返回 0 */
    (void)usart; (void)data; (void)len;
    return -1;
}

void USART_TXE_IRQHandler(USART_TypeDef *usart)
{
    /* 当USART_SR_TXE设置时调用（DR为空）
     * TODO：如果g_tx_buf有数据（头！=尾）：
     *         将g_tx_buf.buf[tail]写入usart->DR
     *         提前尾部
     *       其他：
     *         禁用 TXEIE（清除USART_CR1_TXEIE）
     *         没有更多数据要发送*/
    (void)usart;
}

/* ============================================================
 * 任务 5 — 接收并进行错误处理
 * ============================================================ */

typedef enum {
    UART_RX_OK      = 0,
    UART_RX_OVERRUN = 1,
    UART_RX_FRAMING = 2
} UartRxStatus;

UartRxStatus uart_receive_byte(USART_TypeDef *usart, uint8_t *out)
{
    /* TODO：如果设置了ORE：清除它（读SR然后读DR），返回UART_RX_OVERRUN
     * 如果 FE 设置：清除它（读 SR 然后读 DR），返回 UART_RX_FRAMING
     * 如果 RXNE 设置：*out = (uint8_t)usart->DR, 返回 UART_RX_OK
     * else: *out = 0, 返回 UART_RX_OK（无数据 — 调用者应首先检查 RXNE）*/
    (void)usart; (void)out;
    return UART_RX_OK;
}

/* ============================================================
 * 任务 6 — 找错题：UART 初始化错误
 *
 * 下面的代码将 USART1 初始化为 9600 波特率。
 * 它有 4 个错误。找到并标记每一个。
 * ============================================================ */

void uart_init_BUGGY(USART_TypeDef *usart, uint32_t fclk_hz, uint32_t baud)
{
    /* Bug 1：配置前未禁用USART*/
    /* 缺少：usart->CR1 &= ~USART_CR1_UE；*/

    /* Bug 2：BRR公式错误（无缘无故使用8x而不是16x）*/
    usart->BRR = fclk_hz / (8 * baud);   /* 应为 16 * 波特（默认过采样）*/

    /* Bug 3：TE 和 RE 位未设置 — 发送器和接收器未启用*/
    usart->CR1 = USART_CR1_UE;   /* 缺少 USART_CR1_TE | USART_CR1_RE*/

    /* Bug 4：顺序错误 — UE 在配置 BRR 之前设置。
     * 所有寄存器配置完成后，UE 应设置为最后。*/
}

/* ============================================================
 * SELF-TEST
 * ============================================================ */

static void test_uart(void)
{
    assert(uart_calc_brr(84000000, 115200, 0) == 45);   /* 84M / (16*115200) ≈ 45 */
    assert(uart_calc_brr(16000000, 9600,   0) == 104);  /* 16M / (16*9600) = 104.17 ≈ 104 */

    printf("All UART tests PASSED.\n");
}

int main(void)
{
    test_uart();
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1：UART显示乱码。首先要检查 3 件事？
 *     答案：TODO
 *
 * Q2：TXE 和 TC 标志有什么区别？
 *     你应该什么时候使用每一个？
 *     答案：TODO
 *
 * Q3：在ISR驱动的UART驱动中，为什么要启用TXEIE
 *     在发送功能中并在完成后在ISR中禁用它？
 *     答案：TODO
 *
 * Q4：如何使用 UART 外设实现 RS-485？
 *     需要什么GPIOpin以及如何/何时切换？
 *     答案：TODO
 *
 * Q5：计算 115200 波特率下的位时间和帧持续时间 (8N1)。
 *     这如何限制你的 ISR 延迟要求？
 *     答案：TODO
 */

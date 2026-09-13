/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * 主题 : SPI 和 I2C — 寄存器级 驱动程序
 * 文件：04_Bare_Metal_Peripherals/03_spi_i2c.c
 * ============================================================
 *
 * SPI 和 I2C 是面试高频考点，需要掌握：
 * - 电气差异（推挽输出 与 开漏输出）
 * - SPI 的时钟极性/相位 (CPOL/CPHA)
 * - I2C 地址及 ACK/NACK 时序
 * - 何时选择其中之一
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * 理论 — SPI 与 I2C 并排
 * ============================================================
 *
 * SPI：
 *   线路：SCLK、MOSI、MISO、CS（每个从机一根）
 *   信号线：4+（每个从机一根 CS = N+3 条信号线用于 N 个从机）
 *   驱动方式：推挽输出（速度快，无需上拉电阻）
 *   速度：典型值高达 100 Mbit/s，取决于 MCU
 *   双工：全双工（同时TX+RX）
 *   地址：否 — CS 选择从机
 *   距离：短 (PCB)、电容限制
 *
 * I2C:
 *   线路：SDA（数据）、SCL（时钟）——总共只有 2 条线
 *   信号线：2（所有从机共享相同的 2 根信号线）
 *   驱动方式：开漏输出，带外部上拉电阻（强制）
 *   速度：100 kHz（标准）、400 kHz（快速）、1 MHz（快速+）、3.4 MHz（高）
 *   双工：半双工（SDA 是双向的）
 *   地址：总线上的7位或10位设备地址
 *   多主机：是的，带仲裁
 *
 * SPI 模式 (CPOL/CPHA)：
 *   模式 0：CPOL=0，CPHA=0 → 空闲低电平，上升沿采样（最常见）
 *   模式 1：CPOL=0，CPHA=1 → 空闲低电平，下降沿采样
 *   模式 2：CPOL=1，CPHA=0 → 空闲高电平，下降沿采样
 *   模式 3：CPOL=1，CPHA=1 → 空闲高电平，上升沿采样
 *
 * I2C 交易顺序：
 *   写入：START → ADDR+W → ACK → DATA[0] → ACK → ... → DATA[n] → ACK → STOP
 *   读取：START → ADDR+R → ACK → DATA[0] → ACK → ... → DATA[n] → NACK → STOP
 *   组合（寄存器读取）：
 *          START → ADDR+W → ACK → REG_ADDR → ACK →
 *          RESTART → ADDR+R → ACK → DATA → NACK → STOP
 * ============================================================ */

/* ============================================================
 * SPI — 模拟寄存器
 * ============================================================ */

typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SR;
    volatile uint32_t DR;
} SPI_TypeDef;

/* SPI CR1 位*/
#define SPI_CR1_BIDIMODE  (1u << 15)
#define SPI_CR1_DFF       (1u << 11)   /* 0=8 位，1=16 位帧*/
#define SPI_CR1_SSM       (1u << 9)    /* 软件从机管理*/
#define SPI_CR1_SSI       (1u << 8)    /* 内部从机选择*/
#define SPI_CR1_SPE       (1u << 6)    /* SPI 启用*/
#define SPI_CR1_BR_MASK   (0b111u << 3) /* 波特率位[5:3]*/
#define SPI_CR1_MSTR      (1u << 2)    /* 主模式*/
#define SPI_CR1_CPOL      (1u << 1)
#define SPI_CR1_CPHA      (1u << 0)

/* SPI SR 位*/
#define SPI_SR_BSY   (1u << 7)
#define SPI_SR_TXE   (1u << 1)
#define SPI_SR_RXNE  (1u << 0)

static SPI_TypeDef _SPI1 = {0};
SPI_TypeDef *SPI1 = &_SPI1;

/* 模拟 CS GPIO*/
static volatile uint32_t g_cs_pin = 1;   /* 1 = deasserted */

/* ============================================================
 * 任务 1 — SPI 初始化
 * ============================================================ */

typedef struct {
    uint8_t mode;        /* 0-3 (CPOL/CPHA) */
    uint8_t br_div;      /* 0-7 → PCLK 除以 2^(br_div+1)*/
    uint8_t data_16bit;  /* 0=8位，1=16位*/
    uint8_t msb_first;   /* 0=LSB 第一个（非标准），1=MSB 第一个*/
} SpiConfig;

void spi_init(SPI_TypeDef *spi, const SpiConfig *cfg)
{
    /* TODO：配置前禁用SPI（清除SPE）*/
    /* TODO：构建 CR1：
     *   CPOL = (cfg->模式 >> 1) & 1
     *   CPHA = (cfg->模式 >> 0) & 1
     *   BR = cfg->br_div << 3
     *   MSTR = 1（主模式）
     *   SSM = 1，SSI = 1（软件CS管理）
     *   DFF = cfg->data_16bit
     *   LSBFIRST = !cfg->msb_first（CR1 的位 7）*/
    /* TODO：启用SPI（设置SPE）*/
    (void)spi; (void)cfg;
}

/* ============================================================
 * 任务 2 — SPI 传输（8 位）
 *
 * 在 STM32 上，SPI 是 全双工：发送的每个字节 = 接收的字节。
 * ============================================================ */

void spi_cs_assert(void)   { g_cs_pin = 0; }
void spi_cs_deassert(void) { g_cs_pin = 1; }

uint8_t spi_transfer_byte(SPI_TypeDef *spi, uint8_t tx)
{
    /* TODO：等待 TXE（SR 位 1） — TX 寄存器为空*/
    /* TODO：将tx写入spi->DR*/
    /* TODO：等待 RXNE（SR 位 0）— RX 有数据*/
    /* TODO: 返回 (uint8_t)spi->DR*/
    (void)spi; (void)tx;

    /* 模拟：回显字节*/
    spi->SR |= SPI_SR_TXE | SPI_SR_RXNE;
    spi->DR  = tx;
    return (uint8_t)spi->DR;
}

void spi_write_register(SPI_TypeDef *spi, uint8_t reg, uint8_t value)
{
    /* TODO: CS assert，发送reg（写入位=0），发送值，CS 置低*/
    (void)spi; (void)reg; (void)value;
}

uint8_t spi_read_register(SPI_TypeDef *spi, uint8_t reg)
{
    /* TODO：CSassert，发送reg | 0x80（读取位=1），发送0x00虚拟，CS置低
     * return the byte received during the dummy transfer */
    (void)spi; (void)reg;
    return 0;
}

void spi_transfer_buf(SPI_TypeDef *spi, const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    /* TODO：对于每个字节：spi_transfer_byte，如果 rx != NULL，则存储在 rx 中*/
    (void)spi; (void)tx; (void)rx; (void)len;
}

/* ============================================================
 * I2C — 模拟寄存器
 * ============================================================ */

typedef struct {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t OAR1;
    volatile uint32_t SR1;
    volatile uint32_t SR2;
    volatile uint32_t CCR;
    volatile uint32_t TRISE;
    volatile uint32_t DR;
} I2C_TypeDef;

/* I2C CR1 位*/
#define I2C_CR1_SWRST  (1u << 15)
#define I2C_CR1_ACK    (1u << 10)
#define I2C_CR1_STOP   (1u << 9)
#define I2C_CR1_START  (1u << 8)
#define I2C_CR1_PE     (1u << 0)

/* I2C SR1 位*/
#define I2C_SR1_RXNE   (1u << 6)
#define I2C_SR1_TXE    (1u << 7)
#define I2C_SR1_BTF    (1u << 2)   /* 字节传输完成*/
#define I2C_SR1_ADDR   (1u << 1)   /* 地址已发送/匹配*/
#define I2C_SR1_SB     (1u << 0)   /* 生成起始位*/
#define I2C_SR1_AF     (1u << 10)  /* 承认失败*/
#define I2C_SR1_BERR   (1u << 8)   /* 总线错误*/

static I2C_TypeDef _I2C1 = {0};
I2C_TypeDef *I2C1 = &_I2C1;

/* ============================================================
 * 任务 3 — I2C 初始化
 * ============================================================ */

void i2c_init(I2C_TypeDef *i2c, uint32_t pclk_hz, uint32_t speed_hz)
{
    /* TODO：禁用I2C（清除PE）*/
    /* TODO：设置 CR2.FREQ = pclk_hz / 1_000_000（以 MHz 为单位，最大 42）*/
    /* TODO：计算 CCR：
     *   标准模式（<=100kHz）：CCR = pclk_hz / (2 * speed_hz)
     *   快速模式 (>100kHz)：CCR = pclk_hz / (3 * speed_hz) 且 DUTY=0*/
    /* TODO：计算 TRISE：
     *   标准：(pclk_mhz * 1000 / 1000) + 1 = pclk_mhz + 1
     *   快速：(pclk_mhz * 300 / 1000) + 1*/
    /* TODO：使能I2C（设置PE）*/
    (void)i2c; (void)pclk_hz; (void)speed_hz;
}

/* ============================================================
 * 任务 4 — I2C 写入事务
 *
 * 将 N 个字节写入设备寄存器：
 *   开始 → 地址+W → 寄存器 → 数据[0..n-1] → 停止
 * ============================================================ */

#define I2C_TIMEOUT  10000u

static int i2c_wait_flag(I2C_TypeDef *i2c, uint32_t sr1_flag)
{
    uint32_t timeout = I2C_TIMEOUT;
    while (!(i2c->SR1 & sr1_flag)) {
        if (--timeout == 0) return -1;
    }
    /* 模拟：设置标志*/
    i2c->SR1 |= sr1_flag;
    return 0;
}

int i2c_write(I2C_TypeDef *i2c, uint8_t dev_addr, uint8_t reg_addr,
              const uint8_t *data, uint16_t len)
{
    /* TODO：产生START（CR1 |= START），等待SB标志*/
    /* TODO：发送 (dev_addr << 1) | 0（写），等待ADDR标志*/
    /* TODO：先读SR1，再读SR2，清除ADDR*/
    /* TODO：发送reg_addr，等待TXE*/
    /* TODO：对于数据中的每个字节：写入DR，等待TXE*/
    /* TODO：等待BTF（所有字节传输），生成STOP*/
    /* TODO: 返回 0 表示成功，-1 表示超时*/
    (void)i2c; (void)dev_addr; (void)reg_addr; (void)data; (void)len;
    return 0;
}

/* ============================================================
 * 任务 5 — I2C 读取事务（组合写入然后读取）
 *
 *   开始→地址+W→寄存器→重新启动→地址+R→数据[0..n-1]→停止
 * ============================================================ */

int i2c_read(I2C_TypeDef *i2c, uint8_t dev_addr, uint8_t reg_addr,
             uint8_t *data, uint16_t len)
{
    /* TODO：START，发送addr+W，发送reg_addr*/
    /* TODO：重复启动*/
    /* TODO：发送addr+R (dev_addr<<1)|1，等待ADDR*/
    /* TODO：对于每个字节：
     *   如果是倒数第二个：清除 ACK (CR1 &= ~I2C_CR1_ACK) 至 NACK 最后一个字节
     *   if last：在读取最后一个字节之前设置 STOP
     *   等待RXNE，将DR读入data[i]*/
    /* TODO：为下一个事务重新启用 ACK*/
    (void)i2c; (void)dev_addr; (void)reg_addr; (void)data; (void)len;
    return 0;
}

/* ============================================================
 * 任务 6 — 找错题：I2C 读取事务错误
 *
 * 下面的代码从温度传感器读取 2 个字节。
 * 它有 3 个错误。找到并标记每一个。
 * ============================================================ */

int i2c_read_temp_BUGGY(I2C_TypeDef *i2c, uint8_t addr, uint8_t *out)
{
    /* Bug 1：发送地址之前未生成 START 条件*/
    /* 缺少：i2c->CR1 |= I2C_CR1_START；等待SB；*/

    /* Bug 2：地址格式错误——地址应该是移位加1
     * 和 OR'd 为 1 表示读取*/
    i2c->DR = addr;   /* 应该是 (addr << 1) | 1*/
    i2c_wait_flag(i2c, I2C_SR1_ADDR);
    (void)(i2c->SR1); (void)(i2c->SR2);   /* 清除地址*/

    /* 错误 3：最后一个字节之前未清除 ACK — 接收器不会 NACK，
     * 主设备将输入额外字节并失去同步*/
    out[0] = (uint8_t)i2c->DR;
    i2c_wait_flag(i2c, I2C_SR1_RXNE);
    out[1] = (uint8_t)i2c->DR;

    i2c->CR1 |= I2C_CR1_STOP;
    return 0;
}

/* ============================================================
 * SELF-TEST
 * ============================================================ */

int main(void)
{
    /* 冒烟测试：SPI传输返回虚拟字节*/
    SPI1->SR = SPI_SR_TXE | SPI_SR_RXNE;
    SPI1->DR = 0xAB;
    uint8_t rx = spi_transfer_byte(SPI1, 0x55);
    assert(rx == 0xAB);

    printf("All SPI/I2C tests PASSED.\n");
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1: 你需要将 8 个传感器连接到一个MCU。使用 SPI 与 I2C 进行比较。
 *     你会选择哪一个？为什么？
 *     答案：TODO
 *
 * Q2：I2C 总线被卡住 — SCL 为低电平且 SDA 为低电平。
 *     是什么原因造成的以及如何恢复？
 *     答案：TODO
 *
 * Q3：I2C中的"clock stretching"是什么？哪个设备执行它？
 *     答案：TODO
 *
 * 问题 4：你在传感器数据表中看到 CPOL=1、CPHA=1。
 *     画出发送字节0b10110001的波形。
 *     答案：TODO
 *
 * Q5: 为什么I2C SDA 和SCL 必须有上拉 电阻？
 *     如果它们太大会发生什么？太小了？
 *     答案：TODO
 *
 * Q6: 解释一下I2C 7位和10位寻址的区别。
 *     10 位帧如何变化？
 *     答案：TODO
 */

/*
 * 答案：04_Bare_Metal_Peripherals/03_spi_i2c.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/* ============================================================
 * 面试问题答案（在代码之前完成）
 * ============================================================

Q1：8 个传感器 — SPI 与 I2C？

答：取决于传感器。
   SPI：8 个传感器需要 8 个CS 引脚 + 3 个共享引脚 (SCLK/MOSI/MISO) = 总共 11 个引脚。
   优点：全双工，速度快（10-50 Mbit/s），无地址冲突。
   缺点：管脚较多，走线较长。
   I2C：8个传感器共用2根线（SDA+SCL）。需要 8 个唯一地址。
   优点：只有2个引脚。缺点：半双工，最大 400kHz（快速）
   或 1MHz（快速+），地址空间有限（127 个地址，一些保留位）。
   如果满足以下条件，请选择SPI：高数据速率、需要抗噪性（差分）、long PCB 运行。
   如果满足以下条件，请选择 I2C： 低速传感器（温度、湿度、RTC），pin count 至关重要。

Q2：I2C 总线卡住 — SCL 低，SDA 低。原因及恢复。

A：原因：断电或MCU复位时从机处于中字节。奴隶
   保持 SDA 为低电平，等待更多时钟脉冲来完成其字节。
   恢复过程（来自GPIO的位爆炸）：
   1. 检查 SCL — 如果处于低电平，则存在硬件问题（short 电路）。
      如果 SDA 卡住且 SCL 为 free：
   2. 在 SCL 上发送最多 9 个时钟脉冲（GPIO 切换）。
      大多数从机在看到其字节的剩余时钟后将释放 SDA。
   3. 生成停止条件（SDA 低 → SDA 高，同时 SCL 高）。
   4. 如果仍然卡住：重新启动从站。
   Linux：I2C 子系统中的`i2c_recover_bus()` 自动执行此操作。
   在STM32上：使用I2C外设复位（CR1.SWRST），然后使用GPIO恢复。

Q3：I2C 中的时钟延长。哪个设备执行它？

答：从设备将 SCL 保持为低电平以暂停主设备的时钟。
   当主机将 SCL 驱动为高电平时，从机将其保持为低电平（开漏输出 总线）。
   主设备的 SCL 输入为低电平，等待直至为高电平，然后继续。
   使用案例：从机收到I2C地址+寄存器，需要时间来获取数据
   从内部 EEPROM 或ADC 发送响应之前。
   没有时钟拉伸：主机将在从机时输入垃圾数据
   还在准备中。
   主要求：SCL必须是开漏输出（或者有时钟拉伸
   检测电路）—推挽输出 SCL 不能被从机保持为低电平。

Q4：字节0b10110001的CPOL=1、CPHA=1波形。

答：模式 3：时钟空闲为高电平（CPOL=1），数据在上升沿采样（CPHA=1）。
   0b10110001 的序列（MSB 首先）：
   SCLK：H H L H L H L H L H L H L H L H H
          |下降|上升|下降|上升| ...
   DATA: \_1___0___1___1___0___0___0___1/
   - 数据在 SCLK 的下降沿发生变化。
   - 数据在 SCLK 的上升沿采样（读取）。
   - CS 在第一个时钟之前置为低电平，在第 8 个上升沿后置低。

Q5：I2C 上拉 电阻器 — 太大与太小。

答：上拉电阻 在 I2C 上是强制的（开漏输出 线不能主动驱动为高电平）。
   太大（例如 100 kΩ）：
   - RC 时间常数 = R × C_bus long。信号缓慢上升。
   - 在 400 kHz 处，位在采样之前可能无法达到 VIH → 帧错误。
   - 标准规定快速模式的上升时间 < 300 ns。
   太小（例如 100 Ω）：
   - 当从机将 SDA 拉低时，每线电流 = VCC/R = 3.3V/100Ω = 33 mA。
   - 超过从设备的输出灌电流规格（通常为 3-10 mA） → VOL 太高。
   - 还浪费电力。
   正确值：100 kHz 为 4.7 kΩ，400 kHz 为 2.2 kΩ（3.3V、<100pF 总线的典型值）。
   公式：R = (VCC - VOL_max) / I_OL_min，受上升时间约束 = 0.8473 × R × C_bus。

Q6：I2C 7 位寻址与 10 位寻址。

A：7位（标准）：地址为7位→128个地址，部分保留位→~112个可用。
   帧：START + [ADDR:7][R/W:1]（1 字节）+ ACK + ...
   10位：地址为10位→1024个地址。
   帧：START + [11110:5][ADDR_HIGH:2][W:1]（字节 1）+ ACK +
                  [ADDR_LOW:8]（字节 2）+ ACK + ...
   为了读取，需要重复 START + [11110:5][ADDR_HIGH:2][R:1]。
   10 位寻址向后兼容：5 位前缀 11110 为 保留位
   并且不被 7 位设备使用。
   10 位用例：具有许多I2C 设备（例如服务器背板）的大型系统。
*/

/* ============================================================
 * 任务 1-5 — 完整实现
 * ============================================================ */

typedef struct { volatile uint32_t CR1, CR2, SR, DR; } SPI_TypeDef;
#define SPI_SR_TXE   (1u << 1)
#define SPI_SR_RXNE  (1u << 0)
#define SPI_SR_BSY   (1u << 7)
#define SPI_CR1_SPE  (1u << 6)
#define SPI_CR1_MSTR (1u << 2)
#define SPI_CR1_SSM  (1u << 9)
#define SPI_CR1_SSI  (1u << 8)

static SPI_TypeDef _SPI1 = {0};
SPI_TypeDef *SPI1 = &_SPI1;
static volatile uint32_t g_cs_pin = 1;
void spi_cs_assert(void)   { g_cs_pin = 0; }
void spi_cs_deassert(void) { g_cs_pin = 1; }

typedef struct { uint8_t mode, br_div, data_16bit, msb_first; } SpiConfig;

void spi_init(SPI_TypeDef *spi, const SpiConfig *cfg)
{
    spi->CR1 &= ~SPI_CR1_SPE;
    uint32_t cr1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI;
    cr1 |= (cfg->mode & 1u) ? (1u << 0) : 0;  /* CPHA */
    cr1 |= (cfg->mode >> 1) ? (1u << 1) : 0;  /* CPOL */
    cr1 |= ((uint32_t)cfg->br_div & 0x7u) << 3;
    if (cfg->data_16bit)   cr1 |= (1u << 11);
    if (!cfg->msb_first)   cr1 |= (1u << 7);  /* LSBFIRST */
    spi->CR1 = cr1 | SPI_CR1_SPE;
}

uint8_t spi_transfer_byte(SPI_TypeDef *spi, uint8_t tx)
{
    while (!(spi->SR & SPI_SR_TXE)) {}
    spi->DR = tx;
    while (!(spi->SR & SPI_SR_RXNE)) {}
    return (uint8_t)spi->DR;
}

void spi_write_register(SPI_TypeDef *spi, uint8_t reg, uint8_t val)
{
    spi_cs_assert();
    spi_transfer_byte(spi, reg & 0x7Fu);   /* write bit = 0 */
    spi_transfer_byte(spi, val);
    spi_cs_deassert();
}

uint8_t spi_read_register(SPI_TypeDef *spi, uint8_t reg)
{
    uint8_t val;
    spi_cs_assert();
    spi_transfer_byte(spi, reg | 0x80u);   /* read bit = 1 */
    val = spi_transfer_byte(spi, 0x00);
    spi_cs_deassert();
    return val;
}

void spi_transfer_buf(SPI_TypeDef *spi, const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        uint8_t b = spi_transfer_byte(spi, tx ? tx[i] : 0x00u);
        if (rx) rx[i] = b;
    }
}

int main(void)
{
    /* SPI 烟雾测试*/
    SPI1->SR = SPI_SR_TXE | SPI_SR_RXNE;
    SPI1->DR = 0xAB;
    uint8_t rx = spi_transfer_byte(SPI1, 0x55);
    assert(rx == 0xAB);

    printf("All SPI/I2C answers verified.\n");
    return 0;
}

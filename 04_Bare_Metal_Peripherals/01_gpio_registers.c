/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * 主题：GPIO — 寄存器级 编程（STM32 风格）
 * 文件：04_Bare_Metal_Peripherals/01_gpio_registers.c
 * ============================================================
 *
 * "No HAL, no CubeMX. Show me the registers."
 * 这就是初级嵌入式工程师与高级嵌入式工程师的区别。
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/* ============================================================
 * 理论 — STM32 GPIO 寄存器映射
 * ============================================================
 *
 * 每个GPIO端口有10个寄存器（10×4字节= 40字节）。
 *
 * MODER (0x00)：模式 — 每个 pin 2 位
 *          00 = 输入，01 = 输出，10 = 复用功能，11 = 模拟
 *
 * OTYPER (0x04)：输出类型 — 每个 pin 1 位
 *          0 = 推挽输出, 1 = 开漏输出
 *
 * OSPEEDR (0x08)：输出速度 — 每个 pin 2 位
 *          00=低、01=中、10=高、11=非常高
 *
 * PUPDR (0x0C)：上拉/下拉 — 每个 pin 2 位
 *          00=无，01=上拉，10=下拉
 *
 * IDR (0x10)：输入数据寄存器（只读，每个pin 1 位）
 * ODR (0x14)：输出数据寄存器（每个 pin 1 位）
 *
 * BSRR (0x18)：位设置/复位寄存器 — 原子操作
 *          位[15:0] = SET（写1设置pin，0=无影响）
 *          位 [31:16] = 复位（写入 1 清除pin，0 = 无效）
 *
 * AFRL (0x20)：交替功能低电平（引脚 0-7，每个 4 位）
 * AFRH (0x24)：交替功能高电平（引脚 8-15，每个 4 位）
 *
 * 时钟使能：RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN；
 * 必须在访问任何 GPIO 寄存器之前完成。
 * ============================================================ */


/* 模拟GPIO和RCC寄存器进行主机端练习*/
typedef struct {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFRL;
    volatile uint32_t AFRH;
} GPIO_TypeDef;

typedef struct {
    volatile uint32_t AHB1ENR;
    volatile uint32_t APB1ENR;
    volatile uint32_t APB2ENR;
} RCC_TypeDef;

/* 位定义*/
#define RCC_AHB1ENR_GPIOAEN  (1u << 0)
#define RCC_AHB1ENR_GPIOBEN  (1u << 1)
#define RCC_AHB1ENR_GPIOCEN  (1u << 2)

/* GPIO 模式常数*/
#define GPIO_MODE_INPUT    0b00u
#define GPIO_MODE_OUTPUT   0b01u
#define GPIO_MODE_AF       0b10u
#define GPIO_MODE_ANALOG   0b11u

/* GPIO 速度常数*/
#define GPIO_SPEED_LOW     0b00u
#define GPIO_SPEED_MEDIUM  0b01u
#define GPIO_SPEED_HIGH    0b10u
#define GPIO_SPEED_VHIGH   0b11u

/* GPIO 拉力常数*/
#define GPIO_PULL_NONE     0b00u
#define GPIO_PULL_UP       0b01u
#define GPIO_PULL_DOWN     0b10u

/* 模拟实例*/
static GPIO_TypeDef  _GPIOA = {0};
static GPIO_TypeDef  _GPIOB = {0};
static RCC_TypeDef   _RCC   = {0};
GPIO_TypeDef *GPIOA = &_GPIOA;
GPIO_TypeDef *GPIOB = &_GPIOB;
RCC_TypeDef  *RCC   = &_RCC;


/* ============================================================
 * 任务 1 — GPIO 启用和模式配置
 * ============================================================ */

void gpio_clock_enable(RCC_TypeDef *rcc, uint8_t port_index)
{
    /* TODO：设置rcc->AHB1ENR中port_index的位
     * port_index：0=GPIOA、1=GPIOB、2=GPIOC 等
     * 位位置 = port_index*/
    (void)rcc; (void)port_index;
}

void gpio_set_mode(GPIO_TypeDef *gpio, uint8_t pin, uint8_t mode)
{
    /* TODO：MODER 在位位置每个 pin 有 2 位 (pin * 2)
     * 1. 清零：gpio->MODER &= ~(0b11u << (pin * 2))
     * 2. 设置：gpio->MODER |= (mode << (pin * 2))*/
    (void)gpio; (void)pin; (void)mode;
}

void gpio_set_speed(GPIO_TypeDef *gpio, uint8_t pin, uint8_t speed)
{
    /* TODO：OSPEEDR 每个 pin 有 2 位 — 与 MODER 相同的模式*/
    (void)gpio; (void)pin; (void)speed;
}

void gpio_set_pull(GPIO_TypeDef *gpio, uint8_t pin, uint8_t pull)
{
    /* TODO：PUPDR 每个 pin 有 2 位 — 相同的模式*/
    (void)gpio; (void)pin; (void)pull;
}

void gpio_set_output_type(GPIO_TypeDef *gpio, uint8_t pin, uint8_t otype)
{
    /* TODO：OTYPER 在位置 'pin' 每个 pin 有 1 位
     * o类型: 0=推挽输出, 1=开漏输出*/
    (void)gpio; (void)pin; (void)otype;
}

/* ============================================================
 * 任务 2 — GPIO 读写
 * ============================================================ */

void gpio_write_pin(GPIO_TypeDef *gpio, uint8_t pin, uint8_t value)
{
    /* TODO：使用 BSRR 来设置/清除 atomic — 否 读-改-写
     * 如果值 == 1：gpio->BSRR = (1u << pin)
     * 如果值 == 0：gpio->BSRR = (1u << (pin + 16))*/
    (void)gpio; (void)pin; (void)value;
}

void gpio_toggle_pin(GPIO_TypeDef *gpio, uint8_t pin)
{
    /* TODO：切换 ODR 位 — 读-改-写 在此可接受
     * 因为在这个模拟环境中没有ISR。
     * 在实际硬件上：使用 BSRR 读取 ODR 来切换atomic：
     *   if (ODR & (1<<pin)) BSRR = 1<<(pin+16); else BSRR = 1<<pin;*/
    (void)gpio; (void)pin;
}

uint8_t gpio_read_pin(GPIO_TypeDef *gpio, uint8_t pin)
{
    /* TODO: return bit at position 'pin' of IDR*/
    (void)gpio; (void)pin;
    return 0;
}

void gpio_write_port(GPIO_TypeDef *gpio, uint16_t value)
{
    /* TODO: write all 16 pins at once via ODR*/
    (void)gpio; (void)value;
}

uint16_t gpio_read_port(GPIO_TypeDef *gpio)
{
    /* TODO: read all 16 pins from IDR*/
    (void)gpio;
    return 0;
}

/* ============================================================
 * TASK 3 — Alternate Function configuration
 *
 * Each pin can be routed to a peripheral (UART, SPI, I2C, etc.)
 * by selecting an Alternate Function number (AF0–AF15).
 *
 * AFRL: pins 0-7,  4 bits each, starting at bit (pin * 4)
 * AFRH: pins 8-15, 4 bits each, starting at bit ((pin-8) * 4)
 * ============================================================ */

void gpio_set_af(GPIO_TypeDef *gpio, uint8_t pin, uint8_t af_num)
{
    /* TODO: if pin < 8: modify AFRL
     *       else:       modify AFRH (use pin - 8 as bit offset)
     * Each AF field is 4 bits wide*/
    (void)gpio; (void)pin; (void)af_num;
}

/* ============================================================
 * TASK 4 — Full pin configuration helper
 * Configure a pin in one call — common pattern in real code.
 * ============================================================ */

typedef struct {
    uint8_t mode;
    uint8_t otype;
    uint8_t speed;
    uint8_t pull;
    uint8_t af;
} GPIO_PinConfig;

void gpio_configure_pin(GPIO_TypeDef *gpio, uint8_t pin, const GPIO_PinConfig *cfg)
{
    /* TODO: call all setters in this order:
     * 1. gpio_set_mode  (do first — set to INPUT before changing others)
     * 2. gpio_set_output_type
     * 3. gpio_set_speed
     * 4. gpio_set_pull
     * 5. if mode == GPIO_MODE_AF: gpio_set_af*/
    (void)gpio; (void)pin; (void)cfg;
}

/* ============================================================
 * TASK 5 — Real-world: configure USART2 pins on STM32F4
 *
 * USART2:
 *   PA2 = TX (AF7)
 *   PA3 = RX (AF7)
 *   Both: AF mode, high speed, push-pull, no pull
 * ============================================================ */

void init_usart2_pins(void)
{
    /* TODO: enable GPIOA clock*/
    /* TODO: configure PA2 as AF7, output, high speed, push-pull, no pull*/
    /* TODO: configure PA3 as AF7, output, high speed, push-pull, pull-up (for RX)*/
}

/* ============================================================
 * TASK 6 — BUG HUNT: GPIO configuration mistakes
 *
 * The function below tries to configure an I2C SDA pin (open-drain).
 * 它有 3 个错误。找到并标记每一个。
 * ============================================================ */

void configure_i2c_sda_BUGGY(GPIO_TypeDef *gpio, uint8_t pin)
{
    /* 错误1：？？？*/
    /* RCC clock not enabled before accessing GPIO registers*/

    /* 错误2：？？？*/
    gpio->MODER |= (GPIO_MODE_AF << (pin * 2));   /* should CLEAR first, then set*/

    /* 错误3：？？？*/
    gpio->OTYPER &= ~(1u << pin);   /* sets push-pull — I2C SDA MUST be open-drain*/
                                     /* should be: gpio->OTYPER |= (1u << pin)*/

    gpio_set_af(gpio, pin, 4);   /* STM32F4 上的 AF4 = I2C — 这是正确的*/
    gpio_set_pull(gpio, pin, GPIO_PULL_UP);   /* 正确 — I2C 需要 上拉*/
}

/* ============================================================
 * SELF-TEST
 * ============================================================ */

static void test_gpio(void)
{
    /* 重置*/
    GPIOA->MODER = 0;
    GPIOA->ODR   = 0;
    GPIOA->BSRR  = 0;

    /* 测试模式配置*/
    gpio_set_mode(GPIOA, 5, GPIO_MODE_OUTPUT);
    assert((GPIOA->MODER & (0b11u << 10)) == (GPIO_MODE_OUTPUT << 10));

    /* 通过 BSRR 设置测试atomic*/
    gpio_write_pin(GPIOA, 5, 1);
    /* 应设置 BSRR 位 5*/
    assert(GPIOA->BSRR & (1u << 5));

    /* 测试拉动配置*/
    gpio_set_pull(GPIOA, 3, GPIO_PULL_UP);
    assert((GPIOA->PUPDR & (0b11u << 6)) == (GPIO_PULL_UP << 6));

    printf("All GPIO tests PASSED.\n");
}

int main(void)
{
    test_gpio();
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1：为什么GPIO输出使用BSRR而不是ODR？什么竞态条件
 *     BSRR 消除了吗？
 *     答案：TODO
 *
 * Q2：你将 PA5 配置为 OUTPUT，但读取 IDR.5 始终返回 0。
 *     可能出了什么问题？
 *     答案：TODO
 *
 * Q3：如果忘记启用GPIO外设时钟会发生什么情况
 *     在写入其寄存器之前？
 *     答案：TODO
 *
 * Q4：你需要从 3.3V MCU 到 5V I2C 总线上有一个开漏输出 输出。
 *     你如何配置它以及为什么？
 *     答案：TODO
 *
 * Q5: STM32 可以同时设置多少个GPIO引脚
 *     使用BSRR？这个操作真的是atomic吗？
 *     答案：TODO
 */

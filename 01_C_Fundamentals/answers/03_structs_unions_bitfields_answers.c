/*
 * 答案：03_structs_unions_bitfields.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * 任务 1 — sizeof 测验 answers
 * ============================================================
 *
 * struct S1 { char a; intb； charc； }
 *   a@偏移0（1字节）+3填充→b@偏移4（4字节）+c@偏移8（1字节）+3填充
 *   sizeof = 12
 *
 * struct S2 { int b; char一个； charc; }
 *   b@偏移0（4字节）+a@偏移4（1字节）+c@偏移5（1字节）+2填充
 *   sizeof = 8
 *
 * struct S3 { uint16_t x; uint32_ty； uint8_tz; }
 *   x@偏移0（2字节）+2填充→y@偏移4（4字节）→z@偏移8（1字节）+3填充
 *   sizeof = 12
 *
 * struct S4 { uint8_t a; uint8_tb； uint16_tc; uint32_td； } __attribute__((已包装))
 *   无填充：sizeof = 1+1+2+4 = 8
 */

typedef struct { char a; int b; char c; }                            S1;
typedef struct { int b; char a; char c; }                            S2;
typedef struct { uint16_t x; uint32_t y; uint8_t z; }               S3;
typedef struct __attribute__((packed)) { uint8_t a,b; uint16_t c; uint32_t d; } S4;

/* ============================================================
 * 任务 2 — GPIO 寄存器映射和函数
 * ============================================================ */

typedef struct {
    volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR;
    volatile uint32_t IDR, ODR, BSRR, LCKR, AFRL, AFRH;
} GPIO_TypeDef;

#define GPIO_MODE_INPUT   0b00u
#define GPIO_MODE_OUTPUT  0b01u
#define GPIO_MODE_AF      0b10u
#define GPIO_MODE_ANALOG  0b11u

static GPIO_TypeDef _GPIOA = {0};
GPIO_TypeDef *GPIOA = &_GPIOA;

void gpio_set_mode(GPIO_TypeDef *gpio, uint8_t pin, uint8_t mode)
{
    gpio->MODER = (gpio->MODER & ~(0b11u << (pin*2))) | ((uint32_t)mode << (pin*2));
}

void gpio_write_pin_atomic(GPIO_TypeDef *gpio, uint8_t pin, uint8_t value)
{
    /* BSRR：位[15:0] = 设置，位[31:16] = 复位 — 单次写入，atomic*/
    if (value)
        gpio->BSRR = (1u << pin);
    else
        gpio->BSRR = (1u << (pin + 16));
}

uint8_t gpio_read_input(GPIO_TypeDef *gpio, uint8_t pin)
{
    return (gpio->IDR >> pin) & 1u;
}

/* ============================================================
 * 任务 3 — 控制寄存器 union
 * ============================================================ */

typedef union {
    uint32_t raw;
    struct {
        uint32_t PE     : 1;   /* 位 0*/
        uint32_t TXIE   : 1;   /* 位 1*/
        uint32_t RXIE   : 1;   /* 位 2*/
        uint32_t RSVD   : 5;   /* 位 7:3*/
        uint32_t SPEED  : 2;   /* 位 9:8*/
        uint32_t RSVD2  : 22;
    } bits;
} ControlReg;

/* PE=1，TXIE=0，RXIE=1 → 位 0,2 设置 = 0x05
 * SPEED=0b01 位[9:8] → 0x100
 * raw = 0x00000105 */
#define EXPECTED_CR_RAW 0x00000105u

/* ============================================================
 * 任务 4 — SensorWord union：大端序 int16 + uint16
 * ============================================================ */

typedef union {
    uint8_t  bytes[4];
    struct {
        int16_t  temperature;   /* 大端序 以字节为单位[0:1]*/
        uint16_t humidity;      /* 大端序 以字节为单位[2:3]*/
    } fields;
} SensorWord;

void sensor_parse(const uint8_t *raw, int16_t *temp, uint16_t *hum)
{
    /* 大端序 解析 — 从不直接转换字节（对齐 + 别名 UB）*/
    *temp = (int16_t)(((uint16_t)raw[0] << 8) | raw[1]);
    *hum  = (uint16_t)(((uint16_t)raw[2] << 8) | raw[3]);
}

/* ============================================================
 * 任务 5 — CommandFrame 打包 struct
 * ============================================================ */

typedef struct __attribute__((packed)) {
    uint8_t  sof;       /* 0xA5*/
    uint8_t  cmd;
    uint16_t value;     /* 小端序*/
    uint8_t  checksum;  /* cmd 的XOR ^ value_lo ^ value_hi*/
    uint8_t  eof;       /* 0x5A */
} CommandFrame;

void frame_build(CommandFrame *f, uint8_t cmd, uint16_t value)
{
    f->sof      = 0xA5u;
    f->cmd      = cmd;
    f->value    = value;   /* 在 LE 系统上存储 LE*/
    f->checksum = cmd ^ (uint8_t)(value & 0xFFu) ^ (uint8_t)(value >> 8);
    f->eof      = 0x5Au;
}

int frame_validate(const CommandFrame *f)
{
    if (f->sof != 0xA5u || f->eof != 0x5Au) return 0;
    uint8_t expected = f->cmd ^ (uint8_t)(f->value & 0xFFu) ^ (uint8_t)(f->value >> 8);
    return (f->checksum == expected) ? 1 : 0;
}

/* ============================================================
 * 任务 6 — 找错题 已修复
 *
 * Bug 1：sizeof(PacketHeader*) — POINTER 的大小（4 或 8 字节），而不是 struct。
 *        修复：sizeof(*hdr) 或 sizeof(PacketHeader)
 *
 * 错误 2：长度field 不包括报头大小 — 接收方低估了总大小。
 *        修复：hdr->长度 = (uint16_t)(sizeof(*hdr) + payload_len)
 *
 * Bug 3：序列未进行字节交换 - 协议报头应为大端序（网络顺序）。
 *        修复：hdr->sequence = htonl(seq) — 或手动 BE 写入
 * ============================================================ */

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1: 为什么struct S1 { char a; intb； charc; } 有 sizeof = 12，而不是 6？

答：编译器插入填充以使每个成员与其自然对齐方式对齐。
   - `a`（char，1 个字节）位于偏移量 0 处。
   - 3 字节填充，因此`b`（int，4 字节）从偏移量 4 开始（4 字节对齐）。
   - `b` 占用偏移量 4–7。
   - `c`（char，1 个字节）位于偏移量 8 处。
   - 3 字节尾部填充，因此整个struct 大小是 4 的倍数
     （这是必需的，以便 S1 数组保持所有 `b` 成员对齐）。
   总计：12 字节。
   要获得 6 个字节：重新排序字段 (int、char、char) 或使用 __attribute__((packed))。

Q2：打包结构有什么风险？

答：打包结构会删除填充。任何多字节field都可能未对齐。
   在 Cortex-M0/M0+ 上：未对齐的访问会导致 HardFault。
   在 Cortex-M3/M4 上：可以工作，但速度较慢（LDR 变为 2 字节加载）。
   将指针指向打包的field（例如，&pkt->length）会导致未对齐
   指针——将其传递给需要对齐指针的函数是UB。
   规则：仅使用打包结构来描述线路格式；复制字段
   算术前：uint16_tlen； memcpy(&len, &pkt->长度, 2);

Q3：为什么在嵌入式中使用位域？

A: 位域 简单描述一下硬件寄存器：
   struct { uint32_t PE:1; uint32_t TXIE:1; uint32_t 速度：2； } CR;
   与手册移位/掩码相比，可以节省代码。可读。
   风险： (1) 位顺序为由具体实现决定（依赖于编译器/字节顺序）。
          (2) 多个位域跨越字边界是UB。
          (3) 对于所有编译器上的硬件寄存器都不安全。
   规则：在非安全代码中使用位域以提高可读性；使用明确的
   当位布局必须精确时，移位/掩码宏（例如协议帧）。

Q4：在 C 和 C++ 中，union 类型双关 什么时候会导致 UB？

A：在 C 中（自 C99 TC3/C11 起）：读取与上一个不同的 union 成员
   写入是定义的行为——字节被重新解释。安全的。
   在 C++ 中：从技术上讲，它是UB（严格的别名规则）。使用 memcpy 代替：
     floatf; uint32_tu; memcpy(&u, &f, 4);  // 在 C 和 C++ 中都定义

Q5：如何序列化struct以通过UART进行传输？

答：不要直接将 struct 转换为 uint8_t* — 对齐和填充
   布局依赖于平台。
   正确的做法：将field序列化为field：
     输出[0] = pkt.cmd;
     输出[1] = pkt.len;
     out[2] = pkt.value & 0xFF;   // 首先是LSB
     out[3] = pkt.value >> 8;
   或者使用包装好的 struct + memcpy（带有上述注意事项）。
   始终独立于 C struct 记录线路格式。
*/

int main(void)
{
    /* sizeof 检查*/
    printf("sizeof(S1)=%zu (expected 12)\n", sizeof(S1));
    printf("sizeof(S2)=%zu (expected 8)\n",  sizeof(S2));
    printf("sizeof(S4)=%zu (expected 8)\n",  sizeof(S4));

    /* GPIO测试*/
    gpio_set_mode(GPIOA, 5, GPIO_MODE_OUTPUT);
    assert((GPIOA->MODER & (0b11u << 10)) == (GPIO_MODE_OUTPUT << 10u));
    gpio_write_pin_atomic(GPIOA, 5, 1);
    assert(GPIOA->BSRR & (1u << 5));

    /* 控制注册测试*/
    ControlReg cr = {0};
    cr.bits.PE    = 1;
    cr.bits.RXIE  = 1;
    cr.bits.SPEED = 1;
    assert(cr.raw == EXPECTED_CR_RAW);

    /* 传感器字测试*/
    uint8_t raw[] = {0x01, 0x2C, 0x01, 0xF4}; /* 温度=300（0x012C），嗡嗡声=500（0x01F4）*/
    int16_t t; uint16_t h;
    sensor_parse(raw, &t, &h);
    assert(t == 300 && h == 500);

    /* 命令帧测试*/
    CommandFrame f;
    frame_build(&f, 0x10, 0x1234);
    assert(frame_validate(&f) == 1);
    f.checksum ^= 0xFF;
    assert(frame_validate(&f) == 0);

    printf("All struct/union/bitfield answers verified.\n");
    return 0;
}

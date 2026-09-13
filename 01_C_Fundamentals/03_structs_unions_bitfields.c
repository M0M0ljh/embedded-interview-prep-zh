/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * 主题：结构体、联合、位域、打包结构体
 * 文件：01_C_Fundamentals/03_structs_unions_bitfields.c
 * ============================================================
 *
 * 在嵌入式 C 中，结构直接映射到寄存器布局。
 * 联合启用类型双关。 位域 让你命名寄存器位。
 * 了解对齐和填充至关重要——它决定了是否
 * 你的协议解析器工作或产生垃圾。
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * 理论——结构填充和对齐
 * ============================================================
 *
 * 编译器插入填充以使成员与其大小对齐：
 *   struct { uint8_t a; uint32_tb； } → sizeof = 8（a 之后填充 3 个字节）
 *   struct { uint32_t b; uint8_t一个； } → sizeof = 8（a 之后填充 3 个字节）
 *   struct { uint8_t a; uint8_tb； uint16_tc; uint32_td； } → sizeof = 8（无填充）
 *
 * 强制不填充（对于协议帧/寄存器映射）：
 *   __attribute__((已包装)) ← GCC/Clang
 *   #pragma pack(1) ← MSVC 和大多数编译器
 *
 * 警告：打包结构可能会导致未对齐的访问错误
 *           ARM Cortex-M0/M0+（无硬件未对齐支持）。
 *           使用 memcpy() 从打包结构中读取多字节字段。
 *
 * 联合型双关语：
 *   union { float f; uint32_tu; } x;
 *   x.f = 3.14f;
 *   printf("%08X\n", x.u);   ← 将 float 读取为原始字节 — 在 C 中合法
 * ============================================================ */


/* ============================================================
 * 任务 1 — 预测 sizeof()
 *
 * 运行之前，计算每个struct的大小。
 * 然后用assert进行验证。
 * ============================================================ */

struct S1 { uint8_t a; uint32_t b; uint8_t c; };
struct S2 { uint32_t b; uint8_t a; uint8_t c; };
struct S3 { uint8_t a; uint8_t b; uint16_t c; uint32_t d; };
struct S4 { uint8_t a; uint16_t b; uint8_t c; uint32_t d; } __attribute__((packed));

void task1_sizeof_quiz(void)
{
    /* TODO：运行前填写预期大小*/
    printf("S1: %zu (expected: TODO)\n", sizeof(struct S1));
    printf("S2: %zu (expected: TODO)\n", sizeof(struct S2));
    printf("S3: %zu (expected: TODO)\n", sizeof(struct S3));
    printf("S4: %zu (expected: TODO)\n", sizeof(struct S4));

    /* TODO：取消注释并修复预期值
    assert(sizeof(structS1)==???);
    assert(sizeof(structS2)==???);
    assert(sizeof(structS3)==???);
    assert(sizeof(structS4)==???);
    */
}

/* ============================================================
 * 任务 2 — 将地图注册为 struct（STM32 GPIO 样式）
 *
 * 映射STM32GPIO外设寄存器布局。
 * 每个寄存器都是 32 位，偏移量为 4 字节。
 * 然后实现GPIO方向和输出功能。
 * ============================================================
 *
 * 偏移寄存器目的
 * 0x00 MODER 模式（输入/输出/AF/模拟）每 2 位 pin
 * 0x04 OTYPER 输出类型 (推挽输出/开漏输出) 每个 pin 1 位
 * 0x08 OSPEEDR 速度 2 位/pin
 * 0x0C PUPDR 上拉/下拉 每个 pin 2 位
 * 0x10 IDR 输入数据寄存器（只读）
 * 0x14 ODR 输出数据寄存器
 * 0x18 BSRR 位设置/复位寄存器 (atomic)
 * 0x1C LCKR 锁定寄存器
 * 0x20 AFRL 复用功能低
 * 0x24 AFRH 交替功能高
 */

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

#define GPIO_MODER_INPUT   0b00u
#define GPIO_MODER_OUTPUT  0b01u
#define GPIO_MODER_AF      0b10u
#define GPIO_MODER_ANALOG  0b11u

void gpio_set_mode(GPIO_TypeDef *gpio, uint8_t pin, uint8_t mode)
{
    /* TODO：MODER 在位置 (pin*2) 处每个 pin 有 2 位
     * 清除 2 位，然后设置模式值
     * 模式：GPIO_MODER_INPUT、GPIO_MODER_OUTPUT等*/
    (void)gpio; (void)pin; (void)mode;
}

void gpio_write_pin_atomic(GPIO_TypeDef *gpio, uint8_t pin, uint8_t value)
{
    /* TODO：使用 BSRR 寄存器来设置/清除atomic（无读-改-写）
     * BSRR[15:0] = 设置位（写入 1 进行设置）
     * BSRR[31:16] = 复位位（写 1 清除）
     * 如果值 == 1：通过将 (1u << pin) 写入 BSRR 来设置 pin
     * 如果值 == 0：通过将 (1u << (pin + 16)) 写入 BSRR 来清除pin*/
    (void)gpio; (void)pin; (void)value;
}

uint8_t gpio_read_input(GPIO_TypeDef *gpio, uint8_t pin)
{
    /* TODO：从 IDR 读取，返回 位位于'pin'*/
    (void)gpio; (void)pin;
    return 0;
}

/* ============================================================
 * 任务 3 — 位域 用于寄存器访问
 *
 * 位域 让你命名寄存器内的各个位。
 * 方便阅读，但是：位顺序是由具体实现决定。
 * 切勿使用位域跨不同编译器/机器进行协议解析。
 * 安全使用：仅限本地寄存器抽象。
 * ============================================================ */

typedef union {
    uint32_t raw;
    struct {
        uint32_t PE    :  1;   /* 位 0：外设使能*/
        uint32_t TXIE  :  1;   /* 位 1：TX 中断使能*/
        uint32_t RXIE  :  1;   /* 位 2：RX 中断使能*/
        uint32_t       :  5;   /* 位 3-7：保留位*/
        uint32_t SPEED :  3;   /* 位 8-10：速度选择*/
        uint32_t       : 21;   /* 位 11-31：保留位*/
    } bits;
} ControlReg;

void task3_bitfield_demo(void)
{
    ControlReg cr = {0};

    /* TODO：使用位域启用外设*/
    /* TODO：使用位域启用TX中断*/
    /* TODO：使用 位域 将速度设置为 5*/
    /* TODO：以十六进制打印 cr.raw 并验证预期值：
     *       PE=1（位 0），TXIE=1（位 1），SPEED=5（位 10:8）
     *       expected raw = 0x00000503 */

    printf("Control register raw = 0x%08X (expected 0x00000503)\n", cr.raw);
    assert(cr.raw == 0x00000503u);
}

/* ============================================================
 * 任务 4——协议帧解析联合
 *
 * 来自 CAN 帧的 4 字节传感器数据字：
 *   字节 [3:2] = 0.1°C 单位的温度 (int16_t, 大端序)
 *   字节 [1:0] = 湿度，单位为 0.1% units (uint16_t, 大端序)
 *
 * 使用 union 进行解析 — 没有指针转换，没有 memcpy。
 * ============================================================ */

typedef union {
    uint8_t  raw[4];
    struct {
        /* TODO：在这里声明字段——记住大端序的意思
         * 高字节在原始数组中排在第一位*/
        /* 提示：读取后可能需要交换字节*/
    } fields;
} SensorWord;

void task4_parse_sensor_word(const uint8_t data[4],
                              int16_t *temperature_tenth_C,
                              uint16_t *humidity_tenth_pct)
{
    SensorWord sw;
    /* TODO：将数据[0..3]复制到sw.raw中*/
    /* TODO：温度 = (int16_t)((sw.raw[0] << 8) | sw.raw[1])*/
    /* TODO：湿度 = (uint16_t)((sw.raw[2] << 8) | sw.raw[3])*/
    (void)data; (void)temperature_tenth_C; (void)humidity_tenth_pct;
}

/* ============================================================
 * 任务 5 — 打包 struct 用于 UART 帧序列化
 *
 * 序列化/反序列化 6 字节命令帧：
 *   [0]   SOF     = 0xA5
 *   [1]   CMD     = uint8_t
 *   [2:3] 值 = uint16_t, 小端序
 *   [4] 校验和 = XOR 字节 [0:3]
 *   [5]   EOF     = 0x5A
 * ============================================================ */

typedef struct __attribute__((packed)) {
    uint8_t  sof;
    uint8_t  cmd;
    uint16_t value;
    uint8_t  checksum;
    uint8_t  eof;
} CommandFrame;

void build_command_frame(uint8_t cmd, uint16_t value, uint8_t out[6])
{
    CommandFrame *f = (CommandFrame *)out;
    /* TODO：填写 sof、cmd、value 字段*/
    /* TODO：校验和 = sof ^ cmd ^ (值 & 0xFF) ^ (值 >> 8)*/
    /* TODO：填充eof*/
    (void)cmd; (void)value; (void)f;
}

int validate_command_frame(const uint8_t in[6])
{
    const CommandFrame *f = (const CommandFrame *)in;
    /* TODO：检查 sof == 0xA5 和 eof == 0x5A*/
    /* TODO：重新计算校验和并与f->校验和进行比较*/
    /* TODO: 返回 有效则为 1，无效则为 0*/
    (void)f;
    return 0;
}

/* ============================================================
 * 任务 6 — 找错题
 *
 * 下面的函数应该构建一个网络数据包头。
 * 它有 3 个错误。找到并标记每一个。
 * ============================================================ */

typedef struct {
    uint8_t  version;
    uint8_t  type;
    uint16_t length;
    uint32_t sequence;
} PacketHeader;

void build_header_BUGGY(PacketHeader *hdr, uint8_t type,
                         uint16_t payload_len, uint32_t seq)
{
    /* 错误1：？？？*/
    memset(hdr, 0, sizeof(PacketHeader *));   /* 尺寸错误 — 应该是 sizeof(*hdr)*/

    hdr->version  = 1;
    hdr->type     = type;

    /* 错误2：？？？*/
    hdr->length   = payload_len;              /* 应包括标题大小：
                                               * payload_len + sizeof(数据包头)*/

    /* 错误3：？？？*/
    hdr->sequence = seq;                      /* 对于网络协议应该是
                                               * htonl(seq) — 大端序 字节序*/
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1：为什么不应该使用位域来解析接收到的CAN帧？
 *     答案：TODO
 *
 * Q2：如何确保struct准确映射到硬件寄存器块
 *     没有编译器插入的填充？
 *     答案：TODO
 *
 * Q3：从 C 中的字节数组读取 float 的安全方法是什么？
 *     什么是不安全的方式（仍然有效但违反了严格的别名）？
 *     答案：TODO
 *
 * Q4：在一个奇怪的地址上打包的 struct 成员 uint32_t — 会发生什么
 *     ARM Cortex-M0 与 Cortex-M4 对比？
 *     答案：TODO
 *
 * Q5: 你想要覆盖从地址0x40020000 开始的寄存器映射struct。
 *     用 C 语言写出精确的一行代码。
 *     答案：TODO
 */

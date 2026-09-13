/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * 主题：嵌入式 C 中的指针和内存
 * 文件：01_C_Fundamentals/02_pointers_and_memory.c
 * ============================================================
 *
 * 提示是大多数面试候选人失败的地方。
 * 该文件涵盖：指针算术、void*、函数指针、
 * const 正确性、寄存器指针和常见陷阱。
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * THEORY
 * ============================================================
 *
 * 指针基础知识：
 *   uint8_t *p8 → 指向 1 字节值
 *   uint16_t *p16 → 指向2字节值，p16+1 = p16+2字节
 *   uint32_t *p32 → 指向4字节值，p32+1 = p32+4字节
 *
 * 内存映射 嵌入式寄存器：
 *   #define GPIOA_ODR  (*((volatile uint32_t*)0x40020014))
 *   volatile 告诉编译器：永远不要将其缓存在寄存器中。
 *
 * const 正确性：
 *   const uint8_t *p → 指向const的指针（数据不能改变）
 *   uint8_t *const p → const 指针（地址不能改变）
 *   const uint8_t *const p → 都是不可变的
 *
 * 函数指针：
 *   void (*isr_handler)(void) → 指向函数的指针，取void，返回void
 *   int (*cmp)(const void*, const void*) → qsort 比较器
 * ============================================================ */


/* ============================================================
 * 任务 1 — 字节缓冲区上的指针算术
 *
 * UART 接收缓冲区包含原始协议帧。
 * 仅使用指针算术解析它 - 没有数组索引 []。
 *
 * 帧格式（8字节）：
 *   [0] 起始字节 = 0xAA
 *   [1]     command     = uint8_t
 *   [2:3] payload_len = uint16_t, 大端序
 *   [4:7] 有效负载 = 4 字节数据
 * ============================================================ */

typedef struct {
    uint8_t  command;
    uint16_t payload_len;
    uint8_t  payload[4];
    int      valid;
} Frame;

Frame parse_frame(const uint8_t *buf)
{
    Frame f = {0};

    /* TODO：检查 *(buf + 0) == 0xAA，如果不是则设置 f.valid = 0*/
    /* TODO: f.command = *(buf + 1)*/
    /* TODO: f.payload_len = (uint16_t)(*(buf + 2) << 8) | *(buf + 3) ← 大端序*/
    /* TODO：使用指针运算将 4 个字节从 (buf + 4) 复制到 f.payload，
     *       没有 memcpy，没有数组索引 — 只是 *(dst+i) = *(src+i)*/
    /* TODO: f.valid = 1 */

    (void)buf;
    return f;
}

/* ============================================================
 * 任务 2 — 内存映射 寄存器访问
 *
 * 在真实的MCU上你会得到：
 *   #define PERIPH_BASE 0x40000000
 * 这里我们使用本地的struct来模拟寄存器。
 *
 * 规则：始终使用 volatile 作为 内存映射 寄存器。
 *       如果没有它，编译器可能会消除读/写。
 * ============================================================ */

typedef struct {
    volatile uint32_t CTRL;   /* 偏移0x00*/
    volatile uint32_t STATUS; /* 偏移0x04*/
    volatile uint32_t DATA;   /* 偏移0x08*/
} FakePeripheral;

#define CTRL_ENABLE   (1u << 0)
#define CTRL_RESET    (1u << 1)
#define STATUS_READY  (1u << 0)
#define STATUS_ERROR  (1u << 1)

void periph_init(FakePeripheral *p)
{
    /* TODO：复位外设（设置CTRL_RESET位）*/
    /* TODO：清除复位（清除CTRL_RESET位）*/
    /* TODO：使能外设（设置CTRL_ENABLE位）*/
    (void)p;
}

int periph_wait_ready(FakePeripheral *p, uint32_t timeout_loops)
{
    /* TODO：轮询STATUS_READY位，直到设置或timeout_loops耗尽
     * 返回 0 表示就绪，-1 表示超时*/
    (void)p; (void)timeout_loops;
    return -1;
}

void periph_write(FakePeripheral *p, uint32_t value)
{
    /* TODO：等待STATUS_READY（调用periph_wait_ready，循环1000次）
     * 如果没有准备好：返回不写
     * 将值写入数据寄存器*/
    (void)p; (void)value;
}

/* ============================================================
 * 任务 3 — void* 和类型双关
 *
 * memcpy() 采用 void* — 复制任何类型的标准方法。
 * 通过 union 进行类型双关是在 C（不是 C++）中定义的行为。
 * ============================================================ */

/* 使用 uint8_t 指针迭代实现你自己的 memcpy*/
void *my_memcpy(void *dst, const void *src, size_t n)
{
    /* TODO：将 dst 和 src 转换为 uint8_t*
     * 逐一复制n个字节
     * return original dst pointer */
    (void)dst; (void)src; (void)n;
    return dst;
}

/* 从字节缓冲区读取float（没有memcpy，使用union）*/
float bytes_to_float_union(const uint8_t buf[4])
{
    /* TODO：声明一个union { float f; uint8_t b[4]; } 你；
     * 将 4 个字节从 buf 复制到 u.b
     * return u.f
     * 这在 C99 及更高版本中是合法的 - 在 C++ 中不合法*/
    (void)buf;
    return 0.0f;
}

/* ============================================================
 * 任务 4 — 函数指针（回调和调度表）
 *
 * 嵌入式系统使用函数指针来：
 *   - 中断向量表
 *   - 命令调度（避免long if-else 链）
 *   - 插件/回调模式
 * ============================================================ */

typedef void (*EventCallback)(uint8_t event_id, uint32_t data);

typedef struct {
    uint8_t        event_id;
    EventCallback  handler;
} EventEntry;

static void on_button_press(uint8_t id, uint32_t data)
{
    printf("Button event %u: %u\n", id, (unsigned)data);
}

static void on_sensor_alert(uint8_t id, uint32_t data)
{
    printf("Sensor alert %u: %u\n", id, (unsigned)data);
}

static void on_comm_error(uint8_t id, uint32_t data)
{
    printf("Comm error %u: code=%u\n", id, (unsigned)data);
}

/* 调度表 — 将事件 ID 映射到处理程序*/
static const EventEntry g_dispatch[] = {
    { 0x01, on_button_press },
    { 0x02, on_sensor_alert },
    { 0x10, on_comm_error   },
};
#define DISPATCH_SIZE (sizeof(g_dispatch) / sizeof(g_dispatch[0]))

void dispatch_event(uint8_t event_id, uint32_t data)
{
    /* TODO：迭代g_dispatch，找到匹配的event_id，调用处理程序
     * 如果不匹配：打印"Unknown event: <id>"*/
    (void)event_id; (void)data;
}

/* ============================================================
 * 任务 5 — const 正确性
 *
 * const 是一份合同。正确处理可以防止错误并
 * 允许编译器将数据放置在 MCU 上的Flash (ROM) 中。
 * ============================================================ */

/* 对每个声明进行分类——哪个指针和/或数据是const？*/

/*  const uint8_t *p1 → 数据为const，指针可变
 *  uint8_t *const p2 → 指针为const，数据可变
 *  const uint8_t *const p3 → 两者const
 */

/* 实现：对只读缓冲区中的所有字节求和*/
uint32_t sum_bytes(const uint8_t *buf, size_t len)
{
    /* TODO：迭代、求和 — buf 是只读的，不能对其进行写入*/
    (void)buf; (void)len;
    return 0;
}

/* Flash 查找表 — 应位于 MCU 上的 ROM 中。
 * TODO：添加正确的限定符，以便将其放置在 .rodata 中*/
uint8_t /* TODO：这里是预选赛*/ crc_table[8] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15
};

/* ============================================================
 * 任务 6 — 找错题：指针陷阱
 *
 * 下面的函数应该返回数组中的最大值。
 * 它有 3 个错误。找到并标记每一个。
 * ============================================================ */

uint8_t find_max_BUGGY(uint8_t *arr, int len)
{
    uint8_t *max = arr;           /* 错误1：？？？*/

    for (int i = 0; i < len; i++) {
        if (arr[i] > *max) {
            max = arr + i;
        }
    }

    return *max;                  /* 错误2：？？？*/
    /* Bug 3：解引用之前不检查 arr == NULL 或 len == 0*/
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1：C语言中NULL、0、'\0'有什么区别？
 *     答案：TODO
 *
 * Q2：为什么硬件寄存器指针必须声明为volatile？
 *     volatile 阻止哪些具体优化？
 *     答案：TODO
 *
 * Q3：固件函数接收缓冲区指针。检查什么
 *     在使用它之前你应该先执行吗？
 *     答案：TODO
 *
 * Q4：什么是悬空指针？举个嵌入的例子
 *     这会导致难以发现的错误。
 *     答案：TODO
 *
 * Q5：如何将地址0x20000000表示为指针
 *     到 C 中的uint32_t寄存器？写出准确的声明。
 *     答案：TODO
 *
 * 问题 6：什么是严格别名？为什么它在嵌入式 C 中很重要？
 *     答案：TODO
 */

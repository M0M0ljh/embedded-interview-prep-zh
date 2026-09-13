/*
 * 答案：02_pointers_and_memory.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * 任务 1 — 仅使用指针算术解析 UART 帧
 * ============================================================ */

typedef struct {
    uint8_t  sof;
    uint8_t  cmd;
    uint16_t length;
    uint32_t payload;
    uint8_t  checksum;
} UartFrame;

UartFrame parse_frame(const uint8_t *raw)
{
    UartFrame f;
    f.sof      = *raw;
    f.cmd      = *(raw + 1);
    f.length   = (uint16_t)((uint16_t)(*(raw + 2)) | ((uint16_t)(*(raw + 3)) << 8));
    f.payload  = (uint32_t)(*(raw+4)) | ((uint32_t)(*(raw+5))<<8) |
                 ((uint32_t)(*(raw+6))<<16) | ((uint32_t)(*(raw+7))<<24);
    f.checksum = *(raw + 8);   /* 最后一个字节*/
    return f;
}

/* ============================================================
 * 任务 2 — 内存映射 外设访问
 * ============================================================ */

typedef struct {
    volatile uint32_t CR;
    volatile uint32_t SR;
    volatile uint32_t DR;
    volatile uint32_t RESERVED;
} FakePeripheral;

static FakePeripheral _periph = {0};
FakePeripheral *PERIPH = &_periph;

#define PERIPH_CR_EN   (1u << 0)
#define PERIPH_SR_BUSY (1u << 1)
#define PERIPH_SR_DONE (1u << 0)

void periph_init(FakePeripheral *p)  { p->CR |= PERIPH_CR_EN; }
int  periph_wait_ready(FakePeripheral *p) {
    uint32_t t = 10000;
    while ((p->SR & PERIPH_SR_BUSY) && --t);
    return t ? 0 : -1;
}
void periph_write(FakePeripheral *p, uint32_t data) { p->DR = data; }

/* ============================================================
 * 任务 3 — memcpy 和 union 类型双关
 * ============================================================ */

void my_memcpy(uint8_t *dst, const uint8_t *src, uint16_t n)
{
    while (n--) *dst++ = *src++;
}

float bytes_to_float_union(const uint8_t *bytes)
{
    union { uint32_t u; float f; } pun;
    my_memcpy((uint8_t*)&pun.u, bytes, 4);
    return pun.f;
}

/* ============================================================
 * 任务 4 — 函数指针调度表
 * ============================================================ */

typedef void (*EventCallback)(void *data);

typedef struct {
    uint8_t       event_id;
    EventCallback callback;
} EventEntry;

static void on_button(void *d) { printf("Button event: %p\n", d); }
static void on_timer(void *d)  { printf("Timer event: %p\n", d); }
static void on_uart(void *d)   { printf("UART event: %p\n", d); }

static EventEntry dispatch_table[] = {
    {0x01, on_button},
    {0x02, on_timer},
    {0x03, on_uart},
};
#define DISPATCH_TABLE_SIZE (sizeof(dispatch_table)/sizeof(dispatch_table[0]))

void dispatch_event(uint8_t event_id, void *data)
{
    for (size_t i = 0; i < DISPATCH_TABLE_SIZE; i++) {
        if (dispatch_table[i].event_id == event_id) {
            dispatch_table[i].callback(data);
            return;
        }
    }
    printf("Unknown event: 0x%02X\n", event_id);
}

/* ============================================================
 * 任务 5 — const 正确性
 * ============================================================ */

uint32_t sum_bytes(const uint8_t *data, uint16_t len)
{
    uint32_t sum = 0;
    while (len--) sum += *data++;
    return sum;
}

static const uint8_t crc_table[256] = { /* 演示全部为零*/ 0 };

uint8_t crc8_lookup(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0;
    while (len--) crc = crc_table[crc ^ *data++];
    return crc;
}

/* ============================================================
 * 任务 6 — 找错题 已修复
 * ============================================================ */

/*
 * Bug 1：g_rx_pos不是volatile——编译器缓存在寄存器中。
 *        ISR 增加它，但 main() 永远不会看到变化。
 *        修复：volatileuint8_tg_rx_pos
 *
 * Bug 2：索引前没有边界检查g_rx_buf[g_rx_pos]
 *        如果ISR接收到的字节数超过缓冲区大小，则缓冲区溢出。
 *        修复：如果 (g_rx_pos < sizeof(g_rx_buf)) g_rx_buf[g_rx_pos++] = ...
 *
 * Bug 3: arr == NULL 在 find_max 访问 arr[0] 之前未检查
 *        修复： if (!arr || count == 0) 返回 0;
 */

static volatile uint8_t g_rx_pos_fixed = 0;
static uint8_t g_rx_buf_fixed[64];

void rx_isr_fixed(uint8_t byte)
{
    if (g_rx_pos_fixed < sizeof(g_rx_buf_fixed))
        g_rx_buf_fixed[g_rx_pos_fixed++] = byte;
}

uint8_t find_max_fixed(const uint8_t *arr, uint8_t count)
{
    if (!arr || count == 0) return 0;
    uint8_t max = arr[0];
    for (uint8_t i = 1; i < count; i++)
        if (arr[i] > max) max = arr[i];
    return max;
}

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1: 两者有什么区别：
    uint8_t*p，constuint8_t*p，uint8_t*constp，constuint8_t*constp？

答：uint8_t *p — 指向可变数据的可变指针。 p 和 *p 都可以改变。
   const uint8_t *p — 指向 const 数据的可变指针。可以改变p（指向其他地方），
                              不能写*p。用于只读输入缓冲区。
   uint8_t * const p — const 指向可变数据的指针。无法更改 p（始终点
                              到相同的地址），可以写*p。用于内存映射寄存器。
   const uint8_t * const p — const 指向 const 数据的指针。 p 和 *p 都不能改变。
                              用于通过指针查找表ROM。

Q2: 为什么void *p 在嵌入式中是危险的？

答：没有类型安全——任何指针都可以转换为void*，而不会发出警告。编译器不能
   检测大小不匹配（例如，在预期为 uint32_t* 的情况下传递 uint8_t*）。另外，
   void* 不能直接解引用——你必须先进行强制转换。在嵌入式寄存器访问中
   代码中，错误的转换意味着你访问错误大小的数据并损坏相邻的寄存器。

Q3：什么是悬空指针？ 3 种创建方法。

答：引用内存的指针不再有效。
   方式一：免费即可使用：free(p)； *p = 5；
   方式二：返回本地地址：int *f() { int x=1; 返回 &x; }
   方式3：指向过期栈帧的指针（函数返回，另一个函数
           重用栈 — *p 现在从不同的上下文读取栈数据）。

Q4：函数指针调用开销与常规调用相比？

A：常规调用：编译器在编译时就知道地址→直接分支（BL指令）。
   函数指针：运行时从内存加载的地址→间接分支（BLX Rn）。
   开销：1-2 个额外周期用于寄存器加载 + 潜在的分支预测器缺失。
   对于频繁调用的调度表，该表适合缓存，并且开销可以忽略不计。
   不要在紧密的 DSP 循环内使用函数指针。

Q5: memcpy 什么时候不安全？你用什么代替？

答：当源区域和目标区域重叠时，memcpy 是不安全的。写入 dst 可能会损坏
   复制之前的 src 数据。使用 memmove() — 它安全地处理重叠区域
   （通过中间缓冲区复制或根据需要反转方向）。
   示例：将缓冲区向左滑动 N 个字节：memmove(buf, buf+N, len-N);
*/

int main(void)
{
    /* 测试类型双关语*/
    uint8_t ieee754[] = {0x00, 0x00, 0x80, 0x3F};  /* 小端序 1.0f*/
    float f = bytes_to_float_union(ieee754);
    assert(f == 1.0f);

    /* 测试调度*/
    dispatch_event(0x01, NULL);
    dispatch_event(0x99, NULL);

    /* 测试sum_bytes*/
    uint8_t data[] = {1, 2, 3, 4};
    assert(sum_bytes(data, 4) == 10);

    /* 测试find_max*/
    uint8_t arr[] = {3, 7, 2, 9, 1};
    assert(find_max_fixed(arr, 5) == 9);
    assert(find_max_fixed(NULL, 5) == 0);

    printf("All pointer/memory answers verified.\n");
    return 0;
}

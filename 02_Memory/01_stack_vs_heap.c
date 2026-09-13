/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * 主题：栈与堆——嵌入式系统中的内存布局
 * 文件：02_Memory/01_stack_vs_heap.c
 * ============================================================
 *
 * MCU 上的内存是有限且宝贵的。
 * 出错 = 栈溢出、堆碎片或崩溃。
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * 理论——嵌入式内存区域
 * ============================================================
 *
 * 典型的 ARM Cortex-M 内存映射：
 *
 *  0x00000000  ┌──────────────┐
 *              │ Flash │ .text（代码），.rodata（const 数据）
 *  0x0007FFFF└────────────┘
 *
 *  0x20000000  ┌──────────────┐
 *              │ .data │ 初始化的全局变量（启动时从Flash复制）
 *              ├──────────────┤
 *              │ .bss │ 零初始化全局变量（启动时归零）
 *              ├──────────────┤
 *              │ HEAP │ malloc/free（向上增长↑）
 *              │     ↑↑↑      │
 *              │              │
 *              │     ↓↓↓      │
 *              │ STACK │ 局部变量，返回 地址（向下增长↓）
 *  0x2001FFFF └────────────┘
 *
 * 栈溢出：栈增长为堆或.bss→静默损坏
 * 堆碎片：许多小分配/free循环留下不可用的漏洞
 *
 * 嵌入的经验法则：
 *   避免在生产固件中动态分配 (malloc/free)。
 *   使用static分配：固定大小的缓冲区、内存池。
 *   如果必须使用动态：在启动时分配，切勿使用free。
 * ============================================================ */


/* ============================================================
 * 任务 1 — 对变量进行分类：每个变量住在哪里？
 *
 * 对于下面的每个变量，标识：Flash、.data、.bss、栈、堆
 * ============================================================ */

int       g_counter      = 42;          /* 居住地：TODO*/
int       g_uninitialized;              /* 居住于：TODO*/
const int g_config_value = 100;         /* 居住于：TODO*/

void task1_classify(void)
{
    int      local_var   = 5;           /* 居住于：TODO*/
    static int persist   = 0;           /* 居住于：TODO*/
    int     *dyn         = malloc(64);  /* 栈中的指针；数据在：TODO*/

    (void)local_var; (void)persist;
    if (dyn) free(dyn);
}

/* ============================================================
 * 任务 2 — 栈深度分析
 *
 * 递归函数可能会溢出 MCU 上的栈。
 * 实现常见递归算法的迭代版本。
 * ============================================================ */

/* 递归斐波那契 — 在具有小栈的 MCU 上是危险的*/
uint32_t fib_recursive(uint32_t n)
{
    if (n <= 1) return n;
    return fib_recursive(n-1) + fib_recursive(n-2);
}

/* TODO：实现迭代斐波那契 — O(1) 栈使用*/
uint32_t fib_iterative(uint32_t n)
{
    /* TODO：无递归，常量栈使用*/
    (void)n;
    return 0;
}

/* TODO：实现迭代幂函数（base^exp）*/
uint32_t power_iterative(uint32_t base, uint32_t exp)
{
    /* TODO：基数乘以自身exp次，无递归*/
    (void)base; (void)exp;
    return 0;
}

/* ============================================================
 * 任务 3 — 内存池：固定大小的块分配器
 *
 * 内存池预先分配 N 个固定大小的块。
 * 分配：O(1)，无碎片，确定性。
 * 用于RTOS消息队列、数据包缓冲区、传感器数据。
 * ============================================================ */

#define POOL_BLOCK_SIZE  32u
#define POOL_NUM_BLOCKS  16u

typedef struct {
    uint8_t  storage[POOL_NUM_BLOCKS][POOL_BLOCK_SIZE];
    uint8_t  used[POOL_NUM_BLOCKS];   /* 1=使用中，0=free*/
    uint8_t  num_free;
} MemPool;

void pool_init(MemPool *pool)
{
    /* TODO：零存储和已使用的数组
     * TODO: num_free = POOL_NUM_BLOCKS*/
    (void)pool;
}

void *pool_alloc(MemPool *pool)
{
    /* TODO：找到used[i] == 0的第一个块
     * 将其标记为已使用，递减 num_free
     * return pointer to storage[i]
     * return NULL if no free blocks */
    (void)pool;
    return NULL;
}

void pool_free(MemPool *pool, void *ptr)
{
    /* TODO：查找ptr指向哪个块（指针运算）
     * 验证它在池范围内
     * 标记为free，增量num_free
     * 如果 ptr 不是来自此池：返回 不执行任何操作*/
    (void)pool; (void)ptr;
}

uint8_t pool_num_free(const MemPool *pool)
{
    return pool->num_free;
}

/* ============================================================
 * 任务 4 — 栈水位 测量
 *
 * FreeRTOS 使用 栈填充法：启动时用 0xA5 填充栈，
 * 稍后使用 count 尾随 0xA5 字节来查找高水位线。
 * 实现相同的概念。
 * ============================================================ */

#define FAKE_STACK_SIZE  256u
static uint8_t fake_stack[FAKE_STACK_SIZE];

void stack_paint(void)
{
    /* TODO：用0xA5填充fake_stack*/
}

uint32_t stack_get_high_water_mark(void)
{
    /* TODO：从fake_stack结尾向开头扫描（索引FAKE_STACK_SIZE-1向下到0）
     * count 仍有多少字节0xA5（从未使用过）
     * return that count — it is the "slack" remaining
     * （FreeRTOS 将其返回为未使用的字，我们使用字节）*/
    return 0;
}

/* ============================================================
 * 任务 5 — 链接器段 放置
 *
 * 在嵌入式上：有时需要将缓冲区放置在特定的位置
 * 地址或特定部分。
 *
 * 填写正确的GCC属性：
 * ============================================================ */

/* TODO：将此 1KB 缓冲区放在 ".ccm" 部分（STM32F4 上的核心耦合内存）
 * 提示：__attribute__((节(".ccm")))*/
uint8_t fast_dma_buffer[1024];

/* TODO：此函数应始终放置在 SRAM 中（而不是Flash）
 * 因此它可以在Flash擦除操作期间运行
 * 提示： __attribute__((节(".ramfunc"))) 或 __attribute__((noinline, long_call))*/
void flash_unlock_sequence(void)
{
    /* Flash 编程序列 — 不得从 Flash 开始执行*/
}

/* ============================================================
 * 任务 6 — 找错题：内存管理错误
 *
 * 下面的代码管理数据包缓冲区。它有 4 个错误。
 * 找到并标记每一个。
 * ============================================================ */

typedef struct {
    uint8_t *data;
    uint16_t len;
} Packet;

Packet *create_packet_BUGGY(const uint8_t *src, uint16_t len)
{
    Packet *p = malloc(sizeof(Packet));

    /* 错误1：？？？*/
    /* malloc 之后没有NULL 检查*/

    /* 错误2：？？？*/
    p->data = malloc(len);     /* 如果 len == 0，则 malloc(0) 为 由具体实现决定*/
    memcpy(p->data, src, len);
    p->len = len;
    return p;
}

void destroy_packet_BUGGY(Packet *p)
{
    /* 错误3：？？？*/
    free(p->data);
    free(p);
    p->data = NULL;    /* 错误4：？？？ — p 已经被释放，写入它是 UB*/
}

/* ============================================================
 * SELF-TEST
 * ============================================================ */

static void test_memory(void)
{
    /* 测试斐波那契*/
    assert(fib_iterative(0)  == 0);
    assert(fib_iterative(1)  == 1);
    assert(fib_iterative(10) == 55);

    /* 测试内存池*/
    MemPool pool;
    pool_init(&pool);
    assert(pool_num_free(&pool) == POOL_NUM_BLOCKS);

    void *a = pool_alloc(&pool);
    void *b = pool_alloc(&pool);
    assert(a != NULL && b != NULL && a != b);
    assert(pool_num_free(&pool) == POOL_NUM_BLOCKS - 2);

    pool_free(&pool, a);
    assert(pool_num_free(&pool) == POOL_NUM_BLOCKS - 1);
    pool_free(&pool, a);   /* double-free — pool_free 应该忽略这个*/
    assert(pool_num_free(&pool) == POOL_NUM_BLOCKS - 1);

    /* 测试栈水位*/
    stack_paint();
    assert(stack_get_high_water_mark() == FAKE_STACK_SIZE);

    printf("All memory tests PASSED.\n");
}

int main(void)
{
    test_memory();
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1：为什么大多数嵌入式安全标准（IEC 61508、MISRA）
 *     禁止生产固件中的动态内存分配？
 *     答案：TODO
 *
 * Q2：.data 和 .bss 部分有什么区别？
 *     为什么 .bss 不占用 Flash 映像中的空间？
 *     答案：TODO
 *
 * Q3：FreeRTOS仓库将任务的栈高水位标记为12个字。
 *     这意味着什么？这安全吗？
 *     答案：TODO
 *
 * 问题 4：栈溢出 如何损坏裸机 系统上的堆？
 *     答案：TODO
 *
 * Q5：什么是内存碎片以及为什么它在内存碎片中很重要
 *     long-运行嵌入式系统？
 *     答案：TODO
 *
 * Q6：你有 4KB 的 SRAM，需要处理 10 个并发传感器
 *     每个读数 64 字节。设计记忆策略。
 *     答案：TODO
 */

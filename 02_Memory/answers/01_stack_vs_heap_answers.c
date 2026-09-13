/*
 * 答案：02_Memory/01_stack_vs_heap.c
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * 任务 1 — 变量分类 answers
 * ============================================================
 *
 * intg_counter = 42；          → .data（全局初始化，启动时从Flash复制）
 * intg_uninitialized；         → .bss（全局清零，启动时清零）
 * const int g_config_value=100;→ .rodata / Flash（const 全局 — 保留在 ROM）
 *
 * void task1_classify(void) {
 *   intlocal_var = 5；         → 栈（局部变量，位于栈帧）
 *   staticint持续= 0；    → .data（static 本地 - 在调用中保留，不在栈上）
 *   int *dyn = malloc(64);     → 栈上的指针； HEAP 上的指向内存
 * }
 */

int       g_counter       = 42;       /* .data*/
int       g_uninitialized;            /* .bss*/
const int g_config_value  = 100;      /* .rodata (flash) */

void task1_classify(void)
{
    int        local_var = 5;         /* 栈*/
    static int persist   = 0;         /* .data — static！*/
    int       *dyn       = malloc(64);/* dyn → 栈，*dyn → 堆*/
    (void)local_var; (void)persist;
    if (dyn) free(dyn);
}

/* ============================================================
 * 任务 2 — 迭代实现
 * ============================================================ */

uint32_t fib_iterative(uint32_t n)
{
    if (n <= 1) return n;
    uint32_t prev = 0, curr = 1;
    for (uint32_t i = 2; i <= n; i++) {
        uint32_t next = prev + curr;
        prev = curr;
        curr = next;
    }
    return curr;
}

uint32_t power_iterative(uint32_t base, uint32_t exp)
{
    uint32_t result = 1;
    while (exp--) result *= base;
    return result;
}

/* ============================================================
 * 任务 3 — 内存池
 * ============================================================ */

#define POOL_BLOCK_SIZE  32u
#define POOL_NUM_BLOCKS  16u

typedef struct {
    uint8_t  storage[POOL_NUM_BLOCKS][POOL_BLOCK_SIZE];
    uint8_t  used[POOL_NUM_BLOCKS];
    uint8_t  num_free;
} MemPool;

void pool_init(MemPool *pool)
{
    memset(pool->storage, 0, sizeof(pool->storage));
    memset(pool->used,    0, sizeof(pool->used));
    pool->num_free = POOL_NUM_BLOCKS;
}

void *pool_alloc(MemPool *pool)
{
    for (uint8_t i = 0; i < POOL_NUM_BLOCKS; i++) {
        if (!pool->used[i]) {
            pool->used[i] = 1;
            pool->num_free--;
            return pool->storage[i];
        }
    }
    return NULL;
}

void pool_free(MemPool *pool, void *ptr)
{
    uint8_t *p = (uint8_t *)ptr;
    for (uint8_t i = 0; i < POOL_NUM_BLOCKS; i++) {
        if (pool->storage[i] == p) {
            if (!pool->used[i]) return;  /* double-free 保护*/
            pool->used[i] = 0;
            pool->num_free++;
            return;
        }
    }
}

uint8_t pool_num_free(const MemPool *pool) { return pool->num_free; }

/* ============================================================
 * 任务 4 — 栈水位
 * ============================================================ */

#define FAKE_STACK_SIZE  256u
static uint8_t fake_stack[FAKE_STACK_SIZE];

void stack_paint(void)
{
    memset(fake_stack, 0xA5, FAKE_STACK_SIZE);
}

uint32_t stack_get_high_water_mark(void)
{
    /* 计算 END 中仍为 0xA5 （从未触及）的字节数*/
    uint32_t unused = 0;
    for (int i = (int)FAKE_STACK_SIZE - 1; i >= 0; i--) {
        if (fake_stack[i] == 0xA5u) unused++;
        else break;
    }
    return unused;
}

/* ============================================================
 * 任务 5 — 链接器段 属性 答案
 * ============================================================
 *
 * __attribute__((节(".ccm")))uint8_tfast_dma_buffer[1024];
 *   → 将缓冲区放置在 STM32F4 上的核心耦合内存中（64 KB，0x10000000）。
 *   → CPU 访问速度比 SRAM1 快； DMA 无法访问 CCM。
 *
 * __attribute__((节(".ramfunc"))) void flash_unlock_sequence(void)
 *   → 将函数放入 SRAM 中，以便在 Flash 擦除期间从 RAM 开始执行。
 *   → Flash编程时，从Flash开始执行为未定义行为。
 *   → 启动代码必须将 .ramfunc 部分从 Flash 复制到 SRAM。
 */

__attribute__((section(".ccm_sim")))
uint8_t fast_dma_buffer[1024];         /* ".ccm" 真实目标*/

__attribute__((section(".ramfunc_sim")))
void flash_unlock_sequence(void) {}    /* ".ramfunc" 真实目标*/

/* ============================================================
 * 任务 6 — 找错题 已修复
 *
 * Bug 1: malloc(sizeof(Packet))后没有NULL检查
 *        如果堆已满，p 为 NULL。 p->数据 = malloc(len) → 崩溃 (NULL deref)。
 *        修复：如果 (!p) 返回 NULL；
 *
 * Bug 2：malloc(0)是由具体实现决定（可能是返回NULL或唯一指针）。
 *        使用 NULL src/dst 调用 memcpy 是 UB。
 *        修复： if (len == 0) { p->data = NULL; p->len = 0； 返回p； }
 *             或者：malloc(len > 0 ? len : 1);
 *
 * Bug 3（销毁中）：在访问 p->data 之前没有NULL 检查 p。
 *        如果p是NULL：p->数据崩溃。
 *        修复：如果 (!p) 返回；
 *
 * 错误 4：在 free(p) 之后，p->data = NULL。写入已释放的内存是UB。
 *        p 位置处的内存可能已被另一个分配重用。
 *        修复：调用者应将其指针清空：caller_ptr = NULL；
 *             或者传递指针到指针：void destroy(Packet **pp)
 *             { free((*pp)->数据); free(*pp); *pp = NULL； }
 * ============================================================ */

typedef struct { uint8_t *data; uint16_t len; } Packet;

Packet *create_packet_fixed(const uint8_t *src, uint16_t len)
{
    Packet *p = malloc(sizeof(Packet));
    if (!p) return NULL;                    /* 错误1修复*/
    if (len == 0) {
        p->data = NULL; p->len = 0;
        return p;                           /* 错误2修复*/
    }
    p->data = malloc(len);
    if (!p->data) { free(p); return NULL; }
    memcpy(p->data, src, len);
    p->len = len;
    return p;
}

void destroy_packet_fixed(Packet **pp)
{
    if (!pp || !*pp) return;               /* 错误3修复*/
    free((*pp)->data);
    free(*pp);
    *pp = NULL;                            /* Bug 4修复：调用者的指针为空*/
}

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1：为什么安全标准（IEC 61508、MISRA）禁止动态分配？

A: (1) 非确定性时序：malloc() 运行时间取决于堆状态 — 可以
       需要微秒或毫秒，这在实时系统中是不可接受的。
   (2) 碎片：经过多次alloc/free循环后，free内存存在于
       不连续的块。即使总计，大量分配也可能会失败
       free 字节 > 请求的大小。
   (3) 无故障恢复：嵌入式系统无法显示"out of memory"对话框。
       malloc 失败 → NULL → 如果不检查则崩溃。
   (4) 难以分析：static 分析工具无法证明内存安全
       动态分配。
   替代方案：static 在启动时分配，固定大小的内存池。

Q2：.data 和 .bss 之间的区别？为什么.bss 不占用Flash 空间？

答：.data：初始化的全局变量（int x = 5）。值 5 必须位于 Flash 中，因此
   启动代码可以将其复制到SRAM。 Flash 包含：[.data 的初始值]。
   .bss：零初始化的全局变量（int y；）。整个 .bss 区域归零
   通过启动代码 - 无需在 Flash 中存储零（零是隐式的）。
   Flash 仅存储 .bss 的起始地址和大小，因此启动代码知道
   多少到零。保存Flash与.bss的大小成比例。

Q3: FreeRTOS 栈水位 = 12 个字。安全的？

答：12 个字 = 剩余 48 个字节。是否安全取决于上下文：
   - 如果任务永远不会更深地递归或创建更大的局部变量：可能没问题。
   - 48 字节很紧张——使用本地数组的单个嵌套函数调用
     在异常情况下可能会溢出。
   规则：水印应> 10% of总栈，或至少64字节。
   措施：将栈大小增加 50% a 并重新测量。切勿以 < 20 个字的长度发货。

Q4：栈溢出 如何损坏裸机上的堆？

A：栈向下增长。堆向上增长。在典型的嵌入式布局上：
   [.bss][堆→][...free...][←栈]
   如果栈增长超过栈区域的底部，则进入
   free 空间，然后是堆。 malloc() 内部元数据（块头，
   free列表指针）被栈帧s覆盖。
   结果：下一个malloc()或free()调用破坏了堆→崩溃或静默
   出现在完全不相关的代码路径中的内存损坏。

问题 5：什么是内存碎片？为什么它在运行 long 的系统中很重要？

答：经过多次不同大小的 malloc/free 循环后：
   免费：[4KB][2KB][4KB][2KB][4KB][2KB]（共18KBfree）
   请求：malloc(6KB) → 失败（不存在单个 6KB 块）
   这就是碎片——free内存存在，但碎片不可用。
   嵌入式：运行数月/数年的设备（工业、医疗）将
   最终将其堆碎片化到分配失败的程度，甚至
   尽管技术上有足够的内存。必须重新启动系统 —
   对于关键系统来说是不可接受的。
   解决方案：内存池（所有块大小相同→无碎片）。

Q6：4KB SRAM，10 个并发传感器读数，每个读数 64 字节。设计？

A：使用static内存池：
   staticuint8_tsensor_pool[10][64]；  // 640 字节 = 15.6% of 4KB
   static uint8_t pool_used[10] = {0}；
   alloc()：扫描pool_used查找free槽，返回指针。平均时间为 O(1)。
   free()：清除pool_used位。
   剩余 3456 字节：栈（每个任务 512B × 5 个任务 = 2560B）+ .bss + 堆。
   切勿在具有 4KB SRAM 的系统中使用malloc()。
*/

int main(void)
{
    assert(fib_iterative(0)  == 0);
    assert(fib_iterative(1)  == 1);
    assert(fib_iterative(10) == 55);
    assert(fib_iterative(20) == 6765);

    assert(power_iterative(2, 10) == 1024);
    assert(power_iterative(3, 3)  == 27);

    MemPool pool;
    pool_init(&pool);
    assert(pool_num_free(&pool) == POOL_NUM_BLOCKS);

    void *a = pool_alloc(&pool);
    void *b = pool_alloc(&pool);
    assert(a && b && a != b);
    assert(pool_num_free(&pool) == POOL_NUM_BLOCKS - 2);

    pool_free(&pool, a);
    assert(pool_num_free(&pool) == POOL_NUM_BLOCKS - 1);
    pool_free(&pool, a);   /* double-free — 必须忽略*/
    assert(pool_num_free(&pool) == POOL_NUM_BLOCKS - 1);

    stack_paint();
    assert(stack_get_high_water_mark() == FAKE_STACK_SIZE);
    fake_stack[255] = 0x00;   /* 在顶部模拟栈使用情况*/
    assert(stack_get_high_water_mark() == FAKE_STACK_SIZE - 1);

    uint8_t src[] = {1,2,3,4};
    Packet *pkt = create_packet_fixed(src, 4);
    assert(pkt && pkt->data && pkt->len == 4);
    assert(pkt->data[2] == 3);
    destroy_packet_fixed(&pkt);
    assert(pkt == NULL);

    printf("All memory answers verified.\n");
    return 0;
}

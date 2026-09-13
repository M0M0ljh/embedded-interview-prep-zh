/*
 * 答案：05_RTOS/02_semaphores_mutexes.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1：二进制信号量与互斥锁 — 主要区别。

A：二进制信号量：
   - 信号机制。用于ISR→任务或任务→任务通知。
   - 无所有权：任何任务（或ISR）都可以给予。
   - 否 优先级继承：如果高优先级任务等待，则低优先级任务
     持有者未升高 → 优先级反转 的风险。
   - xSemaphoreGiveFromISR() 是允许的。

   互斥锁：
   - 互斥（资源保护）。
   - 拥有所有权：接受它的任务必须是给予它的任务。
   - 优先级继承 内置：如果更高优先级的任务阻塞
     互斥锁，持有者的优先级暂时提高。
   - 不允许使用xSemaphoreGiveFromISR() (未定义行为)。
   - 无法从 ISR 给出。

   规则：信号量用于信令，互斥锁用于共享资源保护。

Q2：计算信号量——它的模型是什么。

答：计数信号量有一个整数 count [0..MAX]。
   xSemaphoreTake：递减count。如果 count == 0，则阻塞。
   xSemaphoreGive：增量count。解锁一名服务员。
   模型：资源池。
   示例：3 辆SPI 巴士可用。计数=3。
   任务乘坐SPI公交车：COUNT→2。另一项任务：COUNT→1。第三：COUNT→0。
   第四个任务：阻塞直到前三个任务之一返回。
   还模型：事件队列（ISR 在任务运行之间多次触发；
   每场火都需要给予；每个 Give 任务唤醒一次）。
   计数信号量≠队列：没有传输数据，只有count。

Q3：死锁 - define 并给出一个嵌入式示例。

A：死锁：两个或多个任务各自等待一个资源
   另一个→循环等待→全部永远阻塞。

   嵌入示例：
   任务A：获取Mutex_SPI，然后尝试获取Mutex_DMA。
   任务 B：获取Mutex_DMA，然后尝试获取Mutex_SPI。
   如果A同时取SPI，B同时取DMA：
   A 阻塞等待DMA（由 B 持有）。
   B 阻塞等待SPI（由 A 持有）。
   两者都无法继续。系统冻结。

   预防——锁定顺序：
   全局define：始终在DMA之前获取SPI。
   任务 A 和 B：先取SPI，然后取DMA。
   B 必须先尝试拿走 SPI → 看到 A 拿着它 → 阻止 → A 可以
   采取 DMA → 完成 → 给出两个 → B 运行。

Q4：可以从 FreeRTOS 中的 ISR 给出互斥锁吗？

答：互斥锁上的xSemaphoreTake()和xSemaphoreGive()可能不是
   从 ISR 调用。
   原因：互斥锁给予涉及优先级继承逻辑——可能需要
   更改任务的优先级，这不是 ISR 安全的。
   ISR 必须使用：xSemaphoreGiveFromISR() 与二进制或计数信号量。
   图案：
   ISR: xSemaphoreGiveFromISR(sem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);  // 触发上下文切换
   任务：xSemaphoreTake(sem, portMAX_DELAY);

Q5：ISR→任务数据传输的队列与信号量。

A：信号量：仅发出发生某事的信号。没有数据。
   如果 ISR 的触发速度快于任务进程，count 会累积，但
   在任务读取之前，ISR的数据可能已被覆盖。
   适当的时间：任务可以重新读取数据本身（例如，读取ADC寄存器）。

   队列：传输实际数据值。 ISR 将数据复制到队列条目中。
   任务唤醒并弹出数据。如果队列的深度为 N，则最多可以处理 N 个事件
   被缓冲。如果队列已满，xQueueSendFromISR返回pdFALSE（丢弃）。
   适用于：数据值很重要并且不能丢失。

   规则：对于ISR→有数据的任务→使用队列。
         对于ISR→仅任务信号→使用二进制信号量。

Q6: 什么是xHigherPriorityTaskWoken？为什么必须致电portYIELD_FROM_ISR？

答：当ISR给出信号量或发送到队列时，更高优先级的
   任务可能会被解锁。 ISR 无法立即切换上下文
   （它在中断上下文中）。相反：
   1. 如果优先级更高，FreeRTOS 设置 *pxHigherPriorityTaskWoken = pdTRUE
      任务已解锁。
   2. 在 ISR 的末尾，portYIELD_FROM_ISR(val) 检查 val：
      如果pdTRUE：挂起 PendSV 中断。当 ISR 返回时，PendSV 触发
      上下文切换 → 高优先级任务是否立即运行。
      若pdFALSE：ISR正常返回，则无上下文切换。
   没有portYIELD_FROM_ISR：解锁的高优先级任务将不会运行
   直到下一个SysTick（最多1毫秒后）→不必要的延迟。
*/

/* ============================================================
 * 模拟类型
 * ============================================================ */

typedef struct {
    volatile int32_t  count;
    int32_t           max_count;
    const char       *name;
} SimSemaphore;

typedef struct {
    uint8_t  buf[64];
    uint8_t  head, tail, count;
    uint8_t  item_size;
    uint8_t  depth;
} SimQueue;

/* ============================================================
 * 任务 1 — 二进制信号量（ISR → 任务信号）
 * ============================================================ */

static SimSemaphore g_uart_rx_sem = {0, 1, "uart_rx"};

void sim_sem_init_binary(SimSemaphore *s) { s->count = 0; s->max_count = 1; }

int sim_sem_give(SimSemaphore *s)
{
    if (s->count >= s->max_count) return 0;  /* 会溢出*/
    s->count++;
    return 1;
}

int sim_sem_give_from_isr(SimSemaphore *s, uint8_t *higher_prio_woken)
{
    int r = sim_sem_give(s);
    if (r && higher_prio_woken) *higher_prio_woken = 1;
    return r;
}

int sim_sem_take(SimSemaphore *s, uint32_t timeout_ticks)
{
    (void)timeout_ticks;
    if (s->count > 0) { s->count--; return 1; }
    return 0;  /* 真实情况下会阻塞RTOS*/
}

/* ISR：收到新的UART字节*/
static volatile uint8_t g_uart_byte = 0;
void uart_rx_isr_sim(uint8_t byte)
{
    g_uart_byte = byte;
    uint8_t woken = 0;
    sim_sem_give_from_isr(&g_uart_rx_sem, &woken);
    /* portYIELD_FROM_ISR（唤醒）— 会在真实MCU 上触发 PendSV*/
}

/* 任务：处理接收到的字节*/
void uart_process_task_sim(void)
{
    if (sim_sem_take(&g_uart_rx_sem, 1000)) {
        printf("  Received byte: 0x%02X\n", g_uart_byte);
    }
}

/* ============================================================
 * 任务 2 — 互斥（资源保护）
 * ============================================================ */

typedef struct { uint8_t locked; const char *owner; } SimMutex;

int mutex_take(SimMutex *m, const char *caller)
{
    if (m->locked) return 0;  /* 真实情况下会阻塞RTOS*/
    m->locked = 1;
    m->owner  = caller;
    return 1;
}

void mutex_give(SimMutex *m, const char *caller)
{
    if (!m->locked || m->owner != caller) {
        printf("ERROR: %s trying to give mutex owned by %s\n", caller, m->owner);
        return;
    }
    m->locked = 0;
    m->owner  = NULL;
}

static SimMutex g_spi_mutex = {0, NULL};
static uint8_t  g_spi_reg_value = 0;

void spi_write_protected(uint8_t reg, uint8_t val)
{
    if (!mutex_take(&g_spi_mutex, "spi_writer")) return;
    g_spi_reg_value = val;  /* 临界区*/
    mutex_give(&g_spi_mutex, "spi_writer");
}

/* ============================================================
 * 任务 3 — 队列（数据的生产者/消费者）
 * ============================================================ */

typedef struct { uint32_t timestamp_ms; float temperature; float humidity; } SensorSample;

#define QUEUE_DEPTH  4u
typedef struct {
    SensorSample buf[QUEUE_DEPTH];
    uint8_t head, tail, count;
} SensorQueue;

static SensorQueue g_sensor_q = {0};

int queue_send(SensorQueue *q, const SensorSample *s)
{
    if (q->count >= QUEUE_DEPTH) return 0;  /* 满*/
    q->buf[q->head] = *s;
    q->head = (q->head + 1) % QUEUE_DEPTH;
    q->count++;
    return 1;
}

int queue_recv(SensorQueue *q, SensorSample *out)
{
    if (q->count == 0) return 0;  /* 空的*/
    *out = q->buf[q->tail];
    q->tail = (q->tail + 1) % QUEUE_DEPTH;
    q->count--;
    return 1;
}

/* ============================================================
 * 任务 4 — 找错题 已修复
 *
 * 错误 1：spi_write() 访问 g_spi_data 时无需互斥。
 *        同时调用spi_write()的两个任务可能会交错
 *        他们的字节序列→损坏的SPI交易。
 *        修复：在修改共享 SPI 状态之前获取互斥锁，然后给出。
 *
 * Bug 2：uart_isr()从ISR上下文调用xSemaphoreTake()（阻塞）。
 *        ISR 绝不能调用阻塞 FreeRTOS API。 ISR 可能是
 *        在调度器暂停或临界区 中输入。
 *        调用 Take 可能会损坏 FreeRTOS 内部状态 → HardFault。
 *        修复：ISR 应调用 xSemaphoreGiveFromISR()。等待任务
 *        调用 xSemaphoreTake() 并超时。
 *
 * 错误 3：忽略返回 sem_take 的值。
 *        如果sem_take返回pdFALSE（超时已过），则代码继续
 *        使用陈旧数据就像新数据到达一样。
 *        修复：检查 返回 值并跳过超时/失败的处理。
 * ============================================================ */

int main(void)
{
    /* 二进制信号量测试*/
    sim_sem_init_binary(&g_uart_rx_sem);
    assert(g_uart_rx_sem.count == 0);

    uart_rx_isr_sim(0x42);
    assert(g_uart_rx_sem.count == 1);

    uart_process_task_sim();
    assert(g_uart_rx_sem.count == 0);

    /* 互斥测试*/
    spi_write_protected(0x01, 0xAB);
    assert(g_spi_reg_value == 0xAB);
    assert(!g_spi_mutex.locked);  /* 写入后释放*/

    /* 队列测试*/
    SensorSample s1 = {1000, 25.5f, 60.0f};
    SensorSample s2 = {2000, 26.0f, 61.0f};
    assert(queue_send(&g_sensor_q, &s1));
    assert(queue_send(&g_sensor_q, &s2));
    assert(g_sensor_q.count == 2);

    SensorSample out;
    assert(queue_recv(&g_sensor_q, &out));
    assert(out.timestamp_ms == 1000 && out.temperature == 25.5f);
    assert(g_sensor_q.count == 1);

    /* 队列完整测试*/
    SensorSample dummy = {0};
    queue_send(&g_sensor_q, &dummy);
    queue_send(&g_sensor_q, &dummy);
    queue_send(&g_sensor_q, &dummy);
    assert(g_sensor_q.count == QUEUE_DEPTH);
    assert(queue_send(&g_sensor_q, &dummy) == 0);  /* 满时必须失败*/

    printf("All RTOS semaphore/mutex answers verified.\n");
    return 0;
}

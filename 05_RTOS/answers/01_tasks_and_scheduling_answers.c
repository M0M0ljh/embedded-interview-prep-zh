/*
 * 答案：05_RTOS/01_tasks_and_scheduling.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1: FreeRTOS 抢占式 调度 — 它是如何工作的？

答：FreeRTOS 使用固定优先级 抢占式 调度器和 轮转调度
   对于同等优先级的任务。
   在每个SysTick中断（configTICK_RATE_HZ，通常为 1 个kHz）：
   1. SysTick_Handler 调用 xPortSysTickHandler()。
   2. 刻度线count递增；任何具有匹配唤醒时间的任务都是
      从阻塞状态→就绪状态。
   3. 如果更高优先级的任务现在已准备就绪，则 PendSV 处于挂起状态。
   4. PendSV 触发（最低优先级，始终延迟）并执行上下文
      switch：保存当前任务的上下文（R4-R11 + PSP），加载下一个任务的上下文。
   规则：最高优先级的就绪任务始终运行。
   如果两个任务具有相同的优先级：时间片轮转调度，则各自运行
   对于一个 Tick 时间片，然后将 让出 CPU 分配给另一个时间片。

Q2：vTaskDelay 与 vTaskDelayUntil。什么时候更喜欢每种？

答：vTaskDelay(n)：n 个 的块从调用时间开始。
   如果任务晚 5 毫秒被唤醒（例如，运行了更高优先级的任务），
   然后延迟 n 个 Tick → 实际周期变为 n + 抖动。经期漂移。

   vTaskDelayUntil(&xLastWakeTime, n)：阻塞直到绝对唤醒时间
   (xLastWakeTime+n)。调度器在唤醒时设置xLastWakeTime。
   即使任务在本周期晚了 5 毫秒唤醒，下一个周期也会开始
   在正确的绝对时间。无漂移积累。

   使用vTaskDelay：不需要精确计时的任务（UI、日志记录）。
   使用vTaskDelayUntil：周期性控制循环、传感器采样、PID、
   任何抖动累积很重要的任务。

问题 3：栈大小为 128 个字——为什么是字，而不是字节？

答："Words" = Cortex-M 上的 32 位值。 128 个字 = 512 个字节。
   FreeRTOS 栈深度以字指定，因为栈存储
   寄存器值（32 位）和局部变量自然对齐。
   栈大小（以字节为单位） = 深度 × sizeof(StackType_t) = 深度 × 4。
   最小栈 = ISR 帧 (8 寄存器 × 4B = 32B) + 任务序言寄存器
   (R4-R11 × 4B = 32B) + 局部变量。
   规则：用uxTaskGetStackHighWaterMark()测量。如果 < 20 个字 → 增加。

Q4：优先级反转 — 具体示例和两个解决方案。

答：场景：
   TaskL（优先级 1）获取互斥锁 M → 运行。
   任务H（优先级3）准备好→抢占任务L→尝试获取M→块。
   TaskM（优先级2）准备就绪→抢占TaskL（优先级高于L）→运行。
   TaskH STARVES 因为 TaskM 阻止 TaskL 释放 M。

   解决方案 1 — 优先级继承 (xSemaphoreCreateMutex)：
   当TaskH在M上阻塞时，TaskL的优先级会暂时提升到TaskH的优先级
   优先级 (3)。 TaskM 无法再抢占 TaskL。 TaskL完成，释放M，
   优先级返回1，TaskH 运行。

   解决方案 2 — 优先级上限协议：
   互斥锁 M 有一个预定义的 "ceiling priority" = 任何任务的最大优先级
   可能会接受。当任何任务占用 M 时，它会以最高优先级运行。
   即使不知道哪个高优先级任务将竞争，也能防止反转。
   更可预测，但需要在设计时了解所有互斥锁用户。

Q5：单核MCU可以同时运行多少个任务？为什么？

答：在任何给定时刻都运行一个任务（单核 = 单管道）。
   RTOS 通过快速切换任务来创建并发的幻觉
   （上下文切换ing，通常每 1 ms = SysTick 间隔）。
   ISR可以中断正在运行的任务，但它仍然是一个执行线程。
   创建的任务总数：受RAM限制（每个任务需要其栈 + TCB ~100 字节）。
   在 128 KB SRAM 下：512 字节栈的约 100 个任务 + 开销 = 最多 50-60 个任务。
   但实际上：对于嵌入式系统来说，5-15 个任务是典型的。

Q6：xTaskCreate之后任务无法开始。调试方法。

A: 1. 检查返回值：如果xTaskCreate返回pdFAIL，则RAM太小
      栈+TCB。减少栈大小或free其他内存。
   2. 检查vTaskStartScheduler已被调用。任务在调度器启动之前不会运行。
   3. 检查优先级：如果优先级=0（与空闲任务相同），任务可能不会运行
      如果 让出 CPU 永远不会空闲（configUSE_IDLE_HOOK=1，挂钩中无限循环）。
   4. 检查栈溢出：是否有另一个任务溢出其栈、堆/TCB
      损坏会阻止任务运行。使用configCHECK_FOR_STACK_OVERFLOW=2。
   5. 检查vTaskStartScheduler中的断言/硬故障：堆不足
      对于空闲任务栈，或者configMINIMAL_STACK_SIZE太小。
   6. 添加跟踪：configUSE_TRACE_FACILITY=1，然后使用Segger SystemView或
      类似于查看任务状态转换。
*/

/* ============================================================
 * 模拟 FreeRTOS 类型进行编译，无需 RTOS
 * ============================================================ */

typedef uint32_t TickType_t;
typedef void (*TaskFunction_t)(void *);

typedef struct {
    char     name[16];
    uint32_t priority;
    uint32_t stack_depth_words;
    uint8_t  running;
} SimTask;

#define MAX_TASKS  8
static SimTask g_tasks[MAX_TASKS];
static uint8_t g_task_count = 0;
static uint8_t g_scheduler_started = 0;

/* ============================================================
 * 任务 1 — xTaskCreate 模拟
 * ============================================================ */

int xTaskCreate_sim(TaskFunction_t fn, const char *name,
                    uint32_t stack_depth, void *param,
                    uint32_t priority, void **handle)
{
    (void)fn; (void)param; (void)handle;

    /* 验证参数（捕获常见错误）*/
    if (priority == 0)        { printf("WARN: priority 0 = Idle priority\n"); }
    if (stack_depth < 64)     { printf("WARN: stack too small (%u words)\n", stack_depth); }
    if (g_task_count >= MAX_TASKS) return 0;  /* pdFAIL*/

    SimTask *t = &g_tasks[g_task_count++];
    strncpy(t->name, name, 15);
    t->priority = priority;
    t->stack_depth_words = stack_depth;
    t->running = 0;
    return 1;  /* pdPASS*/
}

void vTaskStartScheduler_sim(void)
{
    g_scheduler_started = 1;
    /* 查找最高优先级任务*/
    uint32_t max_pri = 0;
    for (int i = 0; i < g_task_count; i++)
        if (g_tasks[i].priority > max_pri) max_pri = g_tasks[i].priority;
    for (int i = 0; i < g_task_count; i++)
        if (g_tasks[i].priority == max_pri) g_tasks[i].running = 1;
}

/* ============================================================
 * 任务 2 — vTaskDelayUntil 模式
 * ============================================================ */

static TickType_t g_simulated_tick = 0;

TickType_t xTaskGetTickCount_sim(void) { return g_simulated_tick; }

/* 正确的周期性任务模式*/
void periodic_sensor_task_sim(uint32_t period_ticks)
{
    TickType_t xLastWakeTime = xTaskGetTickCount_sim();
    for (int i = 0; i < 5; i++) {
        /* vTaskDelayUntil(&xLastWakeTime, period_ticks) 会放在这里*/
        xLastWakeTime += period_ticks;  /* 模拟绝对时间提前*/
        /* 做工作...*/
    }
}

/* ============================================================
 * 任务 3 — 优先级反转 演示
 * ============================================================ */

typedef struct { uint8_t locked; uint8_t holder_priority; } SimMutex;

void mutex_take_sim(SimMutex *m, uint8_t caller_priority)
{
    if (m->locked) {
        /* 优先级继承：如果需要，将持有者提升为呼叫者的优先级*/
        if (caller_priority > m->holder_priority)
            m->holder_priority = caller_priority;
        printf("Task pri=%u BLOCKED on mutex (holder raised to %u)\n",
               caller_priority, m->holder_priority);
    } else {
        m->locked = 1;
        m->holder_priority = caller_priority;
    }
}

void mutex_give_sim(SimMutex *m)
{
    m->locked = 0;
    m->holder_priority = 0;
}

/* ============================================================
 * 任务 4 — ms_to_ticks / ticks_to_ms
 * ============================================================ */

#define CONFIG_TICK_RATE_HZ  1000u

TickType_t ms_to_ticks(uint32_t ms)
{
    return (TickType_t)((ms * CONFIG_TICK_RATE_HZ) / 1000u);
}

uint32_t ticks_to_ms(TickType_t ticks)
{
    return (uint32_t)((ticks * 1000u) / CONFIG_TICK_RATE_HZ);
}

/* ============================================================
 * 任务 5 — 找错题 分析
 *
 * Bug 1：栈大小 = 32 个字 = 128 个字节。
 *        FreeRTOS 最小值为 configMINIMAL_STACK_SIZE（通常为 128 个字）。
 *        即使是一个空任务也需要大约 40 个单词来保存上下文。
 *        结果：第一次函数调用时出现栈溢出 → HardFault。
 *        修复：每个任务增加到至少 128 个字（512 字节）。
 *
 * Bug 2：优先级 = 0 = 与空闲任务相同。
 *        在FreeRTOS中，空闲任务以优先级0运行。
 *        优先级为 0 的任务与空闲任务共享时间。如果空闲钩子有工作
 *        或者configIDLE_SHOULD_YIELD=0，用户任务可能几乎无法运行。
 *        修复：用户任务应使用优先级 >= 1。为空闲保留 0。
 *
 * 错误 3：vTaskStartScheduler() 未调用。
 *        xTaskCreate 创建挂起状态的任务；他们只会跑
 *        一旦调度器启动。没有 vTaskStartScheduler()：
 *        任务永远不会执行，在main()末尾无限循环。
 *        修复：在所有 xTaskCreate() 调用之后添加 vTaskStartScheduler()。
 *        注意：vTaskStartScheduler() 永远不会返回。如果是这样的话：
 *        堆对于空闲任务栈来说太小。
 * ============================================================ */

int main(void)
{
    /* 任务创建测试*/
    int r1 = xTaskCreate_sim(NULL, "LED_Task",    128, NULL, 2, NULL);
    int r2 = xTaskCreate_sim(NULL, "Sensor_Task", 256, NULL, 3, NULL);
    assert(r1 == 1 && r2 == 1);
    assert(g_task_count == 2);

    vTaskStartScheduler_sim();
    assert(g_scheduler_started);
    /* Sensor_Task 具有更高优先级 → 应首先运行*/
    assert(g_tasks[1].running == 1);

    /* ms_to_ticks 测试*/
    assert(ms_to_ticks(1000) == 1000);
    assert(ms_to_ticks(100)  == 100);
    assert(ticks_to_ms(500)  == 500);

    /* 优先级反转 演示*/
    SimMutex m = {0};
    mutex_take_sim(&m, 1);  /* 低优先级任务需要互斥*/
    assert(m.locked && m.holder_priority == 1);
    mutex_take_sim(&m, 3);  /* 高优先级任务被阻止 → 提高持有者*/
    assert(m.holder_priority == 3);  /* 优先继承*/
    mutex_give_sim(&m);
    assert(!m.locked);

    printf("All RTOS task/scheduling answers verified.\n");
    return 0;
}

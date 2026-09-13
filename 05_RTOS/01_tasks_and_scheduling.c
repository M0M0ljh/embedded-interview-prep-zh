/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * 主题：RTOS — 任务、调度和优先级
 * 文件：05_RTOS/01_tasks_and_scheduling.c
 * ============================================================
 *
 * FreeRTOS 是嵌入式面试中最常见的 RTOS。
 * 本文件模拟 FreeRTOS API 的概念，无需
 * 真实的 FreeRTOS 移植，可直接在主机上编译运行。
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * 理论 — RTOS 调度
 * ============================================================
 *
 * FreeRTOS 中的任务状态：
 *   RUNNING — 当前在 CPU 上执行
 *   READY — 准备运行，等待调度器
 *   BLOCKED — 等待事件（延迟、信号量、队列）
 *   SUSPENDED — 显式挂起 (vTaskSuspend)
 *
 * 调度器类型：
 *   抢占式：高优先级任务立即抢占
 *               低优先级任务（FreeRTOS 默认方式）
 *   协作式：任务运行直到让出 CPU（无抢占）
 *   时间片轮转：同等优先级的任务轮流使用 CPU 时间
 *
 * 优先级：在 FreeRTOS 中，数值越大，优先级越高
 *   （与 OSEK/AUTOSAR 等其他一些 RTOS 相反！）
 *
 * 栈：每个任务都有自己的栈。
 *   FreeRTOS 任务栈大小以机器字（word）为单位；在 32 位平台上，一个 word 通常为 4 字节
 *   configMINIMAL_STACK_SIZE 若为 128 word，在 32 位平台上对应 512 字节
 *
 * Tick：RTOS 系统节拍由周期性中断产生（ARM Cortex-M 上通常使用 SysTick）
 *   configTICK_RATE_HZ = 1000 → 每个 Tick 为 1 ms
 *   vTaskDelay(100) = 延迟 100 个 Tick = 100 ms
 *   pdMS_TO_TICKS(250) 可移植地将 250 ms 转换为 Tick 数
 *
 * 上下文切换：
 *   在 Cortex-M 上：PendSV 处理程序保存/恢复 R4-R11
 *   CPU 硬件自动保存 R0–R3、R12、LR、PC、xPSR
 * ============================================================ */

/* ============================================================
 * 模拟FreeRTOS API（用于主机端练习）
 * ============================================================ */

typedef void* TaskHandle_t;
typedef void* QueueHandle_t;
typedef void* SemaphoreHandle_t;
typedef uint32_t TickType_t;
typedef uint32_t BaseType_t;
typedef uint32_t UBaseType_t;

#define pdTRUE   1u
#define pdFALSE  0u
#define pdPASS   1u
#define pdFAIL   0u
#define portMAX_DELAY  0xFFFFFFFFu

#define configTICK_RATE_HZ    1000u
#define pdMS_TO_TICKS(ms)     ((TickType_t)((ms) * configTICK_RATE_HZ / 1000u))

/* 模拟任务创建（主机上没有真正的调度）*/
static int g_task_count = 0;

BaseType_t xTaskCreate_sim(void (*task_fn)(void *),
                           const char *name,
                           uint16_t stack_words,
                           void *param,
                           UBaseType_t priority,
                           TaskHandle_t *handle)
{
    printf("[RTOS] Task created: %s (stack=%u words, priority=%u)\n",
           name, stack_words, (unsigned)priority);
    g_task_count++;
    if (handle) *handle = (void *)(uintptr_t)g_task_count;
    (void)task_fn; (void)param;
    return pdPASS;
}

/* ============================================================
 * 任务 1 — 任务创建和生命周期
 *
 * 创建两个任务：一个 LED 闪烁任务、一个传感器读取器。
 * 传感器任务具有更高的优先级。
 * ============================================================ */

volatile uint8_t g_led_state   = 0;
volatile uint8_t g_sensor_data = 0;

/* LED 闪烁器任务 — 以低优先级运行，闪烁 500 毫秒*/
void led_task(void *param)
{
    /* TODO：在实际系统中，无限循环：
     *   while (1) {
     *       gpio_toggle_pin(GPIOA,5);
     *       vTaskDelay(pdMS_TO_TICKS(500));
     *   }
     * 在主机上：只需切换一次标志*/
    (void)param;
    g_led_state ^= 1;
    printf("LED task: LED %s\n", g_led_state ? "ON" : "OFF");
}

/* 传感器读取器任务 — 每 100 毫秒以高优先级运行一次*/
void sensor_task(void *param)
{
    /* TODO：在真实系统中：
     *   while (1) {
     *       g_sensor_data = read_adc_channel(0);
     *       vTaskDelay(pdMS_TO_TICKS(100));
     *   }
     * 在主机上：模拟读取*/
    (void)param;
    g_sensor_data = 42;
    printf("Sensor task: data = %u\n", g_sensor_data);
}

void create_tasks(void)
{
    /* TODO：创建优先级为1的led_task，堆叠256个字*/
    /* TODO：创建优先级为3的sensor_task，堆叠512个字*/
    /* TODO：启动调度器：vTaskStartScheduler()*/
    TaskHandle_t led_handle, sensor_handle;

    xTaskCreate_sim(led_task, "LED", 256, NULL, 1, &led_handle);
    xTaskCreate_sim(sensor_task, "Sensor", 512, NULL, 3, &sensor_handle);
    (void)led_handle; (void)sensor_handle;
}

/* ============================================================
 * 任务 2 — 优先级反转 场景
 *
 * 经典嵌入式面试题：什么是优先级反转？
 * 具有 优先级继承 的互斥锁如何修复它？
 *
 * 场景：
 *   任务 H（高优先级）需要互斥锁 M
 *   任务 L（低优先级）持有互斥锁 M
 *   任务 M（中优先级）可运行（不需要 M）
 *
 * 没有 优先级继承：
 *   L 跑，但 M 抢占 L (M > L) → H 挨饿！
 *   H 正在等待 L 持有的 M，但 L 无法运行，因为 M 抢占了它。
 *
 * 对于优先级继承（FreeRTOS中的xSemaphoreCreateMutex）：
 *   当 H 阻塞由 L 持有的互斥锁时，L 会暂时提升到 H 的优先级。
 *   L > M，因此 L 运行并释放互斥锁。
 *   H解除阻塞并立即运行。
 * ============================================================ */

void priority_inversion_demo(void)
{
    printf("\n--- Priority Inversion Demo ---\n");
    printf("Task H (prio=3) needs mutex.\n");
    printf("Task L (prio=1) holds mutex, is preempted by Task M (prio=2).\n");
    printf("Result WITHOUT priority inheritance: H starves behind M behind L.\n");
    printf("Result WITH priority inheritance (xSemaphoreCreateMutex):\n");
    printf("  L is boosted to prio=3, completes, H runs. M runs last.\n");
}

/* ============================================================
 * 任务 3 — 节拍率和延迟精度
 * ============================================================ */

uint32_t ms_to_ticks(uint32_t ms)
{
    /* TODO: 返回 pdMS_TO_TICKS(ms) — 使用宏*/
    return ms * configTICK_RATE_HZ / 1000;
}

uint32_t ticks_to_ms(uint32_t ticks)
{
    /* TODO: 返回 个 刻度 * 1000 / configTICK_RATE_HZ*/
    return ticks * 1000 / configTICK_RATE_HZ;
}

/* ============================================================
 * 任务 4 — 栈大小估计
 *
 * 常见面试问题：如何选择任务栈大小？
 * 经验法则：
 *   - 计算局部变量、函数调用深度、字符串缓冲区
 *   - FreeRTOS uxTaskGetStackHighWaterMark() 仓库rts未使用的字数
 *   - 从512个字开始，测量后调低
 *   - 始终添加 20% s安全裕度
 *   - 在 Cortex-M 上：每个上下文保存 = 16 个寄存器 × 4 字节 = 64 字节
 * ============================================================ */

uint32_t estimate_task_stack_bytes(uint32_t max_local_vars_bytes,
                                   uint8_t max_call_depth,
                                   uint32_t interrupt_context_bytes)
{
    /* TODO：粗略估计：
     * frame_per_call = max_local_vars_bytes / max_call_depth（平均值）
     * 总计 = frame_per_call * max_call_depth
     *       + interrupt_context_bytes（上下文保存在ISR条目上：8个寄存器= 32字节）
     *       + 20% s安全
     * 四舍五入到最接近的 2 次方（常见做法）
     * 返回 以字节为单位（除以 4 即可得到 FreeRTOS 栈参数的字）*/
    (void)max_local_vars_bytes; (void)max_call_depth; (void)interrupt_context_bytes;
    return 512;   /* 占位符*/
}

/* ============================================================
 * 任务 5 — vTaskDelayUntil 模式（固定频率循环）
 *
 * vTaskDelay(N) = 延迟 N 个 从现在开始勾选
 * vTaskDelayUntil(&last_wake, N) = 延迟直到 N 个 从上次唤醒开始计时
 *
 * 使用 vTaskDelayUntil 执行精确的周期性任务。
 * vTaskDelay 存在漂移，因为不包括处理时间。
 * ============================================================ */

void periodic_task_correct(void *param)
{
    /* 模拟 — 显示模式*/
    TickType_t last_wake = 0;   /* 真实代码：开始时的xTaskGetTickCount()*/
    const TickType_t period = pdMS_TO_TICKS(10);   /* 10毫秒周期*/

    for (int i = 0; i < 3; i++) {
        /* TODO：在真实硬件上：vTaskDelayUntil（&last_wake，期间）*/
        last_wake += period;   /* 模拟*/
        printf("Periodic task tick at t=%u ms\n", (unsigned)ticks_to_ms(last_wake));
    }
    (void)param;
}

/* ============================================================
 * 任务 6 — 找错题：任务创建和调度错误
 *
 * 下面的代码错误地创建了任务。
 * 发现 3 个错误。
 * ============================================================ */

void worker_task_fn(void *p) { (void)p; }

void setup_tasks_BUGGY(void)
{
    TaskHandle_t h;

    /* Bug 1：32 个字（128 字节）的栈大小太小。
     * FreeRTOS 开销 + ISR 上下文 = 最少约 100 字节。
     * 任何函数调用都会溢出。建议最少：128 个字。*/
    xTaskCreate_sim(worker_task_fn, "Worker", 32, NULL, 2, &h);

    /* Bug 2：priority=0是空闲任务优先级。
     * 真正的任务应该有优先级 >= 1，否则它
     * 与闲置任务竞争（并可能导致闲置任务挨饿），
     * 防止堆清理和栈水位检查。*/
    xTaskCreate_sim(worker_task_fn, "Worker2", 256, NULL, 0, &h);

    /* 错误 3：vTaskStartScheduler() 未调用 — 任务已创建但从未运行。
     * 创建所有任务后，必须调用vTaskStartScheduler()。
     * 此调用之后的代码永远不会执行（调度器接管）。*/
}

/* ============================================================
 * SELF-TEST
 * ============================================================ */

int main(void)
{
    create_tasks();
    led_task(NULL);
    sensor_task(NULL);
    priority_inversion_demo();
    periodic_task_correct(NULL);

    assert(ms_to_ticks(500)  == 500);
    assert(ticks_to_ms(1000) == 1000);

    printf("\nAll RTOS task tests PASSED.\n");
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1：vTaskDelay和vTaskDelayUntil有什么区别？
 *     什么时候重要？
 *     答案：TODO
 *
 * Q2：解释一下优先级反转。什么RTOS原语可以阻止它？
 *     答案：TODO
 *
 * Q3：在FreeRTOS启用抢占的情况下，两个任务具有相同的优先级：
 *     一个会抢占另一个吗？是什么配置了这种行为？
 *     答案：TODO
 *
 * Q4：任务的栈高水位标记为 8 个字。这安全吗？
 *     你对此做了什么？
 *     答案：TODO
 *
 * Q5：如果你从不调用堆和空闲任务会发生什么
 *     vTaskStartScheduler()？
 *     答案：TODO
 *
 * Q6：你的系统有 5 个任务。最高优先级任务每 1 毫秒运行一次。
 *     绘制一条时间线，显示优先级较低的任务如何获得 CPU 时间。
 *     答案：TODO
 */

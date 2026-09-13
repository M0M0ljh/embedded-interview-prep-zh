/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * 主题：Linux 嵌入式 — sysfs、进程、套接字
 * 文件：07_Linux_Embedded/01_linux_drivers_sysfs.c
 * ============================================================
 *
 * Linux 嵌入式（Yocto、Buildroot、Raspberry Pi、i.MX）
 * 是独立于裸机的技能。
 * 了解用户空间、内核接口和调试工具。
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>

/* ============================================================
 * 理论 — Linux 嵌入式架构
 * ============================================================
 *
 * 用户空间 ↔ 内核边界：
 *
 *   Application (C/Python)
 *       ↕ 文件I/O（打开/读/写/ioctl）
 *   虚拟文件系统（VFS）
 *       ↕
 *   ┌─────────────────────────────────┐
 *   │ sysfs (/sys/class/gpio/...) │ ← 通过文件进行硬件控制
 *   │ devfs (/dev/ttyS0, /dev/i2c) │ ← 字符设备
 *   │ procfs (/proc/meminfo, ...) │ ← 内核运行时信息
 *   └─────────────────────────────────┘
 *       ↕ 内核驱动
 *   硬件（GPIO、SPI、I2C、UART、...）
 *
 * 关键接口：
 *   /sys/class/gpio/ — GPIO 通过 sysfs 控制
 *   /sys/class/leds/ — LED 控制（亮度、触发）
 *   /sys/class/hwmon/ — 硬件监控（温度、电压）
 *   /sys/bus/i2c/ — I2C 设备树
 *   /dev/i2c-N — I2C 用户空间访问（I2C_RDWR ioctl）
 *   /dev/spidevN.N — SPI 用户空间访问 (SPI_IOC_MESSAGE ioctl)
 *   /proc/meminfo — 内存统计
 *   /proc/net/dev — 网络接口统计信息
 * ============================================================ */

/* ============================================================
 * 任务 1 — GPIO 通过 sysfs 控制
 *
 * 传统接口（仍广泛用于嵌入式Linux）。
 * 新项目使用 libgpiod 和 /dev/gpiochipN 代替。
 * ============================================================ */

#define SYSFS_GPIO_PATH  "/sys/class/gpio"

int sysfs_gpio_export(uint32_t pin)
{
    /* TODO：打开"/sys/class/gpio/export"进行写入
     * 将 pin 数字写为字符串（例如，"17\n"）
     * 关闭文件；成功返回 0，出错返回 -1*/
    char path[64];
    snprintf(path, sizeof(path), "%s/export", SYSFS_GPIO_PATH);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;   /* 需要 root 或 GPIO 组*/
    char buf[8];
    int n = snprintf(buf, sizeof(buf), "%u", (unsigned)pin);
    int ret = (write(fd, buf, (size_t)n) == n) ? 0 : -1;
    close(fd);
    return ret;
}

int sysfs_gpio_set_direction(uint32_t pin, const char *direction)
{
    /* TODO：打开"/sys/class/gpio/gpioN/direction"进行写入
     * 写"in"或"out"
     * 成功返回 0，出错返回 -1 */
    char path[64];
    snprintf(path, sizeof(path), "%s/gpio%u/direction", SYSFS_GPIO_PATH, (unsigned)pin);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    ssize_t written = write(fd, direction, strlen(direction));
    close(fd);
    return (written == (ssize_t)strlen(direction)) ? 0 : -1;
}

int sysfs_gpio_write(uint32_t pin, uint8_t value)
{
    /* TODO：打开"/sys/class/gpio/gpioN/value"进行写入
     * 写"1"或"0"*/
    char path[64];
    snprintf(path, sizeof(path), "%s/gpio%u/value", SYSFS_GPIO_PATH, (unsigned)pin);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    char c = value ? '1' : '0';
    int ret = (write(fd, &c, 1) == 1) ? 0 : -1;
    close(fd);
    return ret;
}

int sysfs_gpio_read(uint32_t pin)
{
    /* TODO：打开读取，读取1char、返回 0或1*/
    char path[64];
    snprintf(path, sizeof(path), "%s/gpio%u/value", SYSFS_GPIO_PATH, (unsigned)pin);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    char c = '0';
    ssize_t r = read(fd, &c, 1);
    close(fd);
    return (r == 1) ? (c == '1' ? 1 : 0) : -1;
}

int sysfs_gpio_unexport(uint32_t pin)
{
    /* TODO：将pin号码写入/sys/class/gpio/unexport*/
    char path[64], buf[8];
    snprintf(path, sizeof(path), "%s/unexport", SYSFS_GPIO_PATH);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    int n = snprintf(buf, sizeof(buf), "%u", (unsigned)pin);
    int ret = (write(fd, buf, (size_t)n) == n) ? 0 : -1;
    close(fd);
    return ret;
}

/* ============================================================
 * 任务 2 — 通过 hwmon sysfs 读取 CPU 温度
 * ============================================================ */

float read_cpu_temp_celsius(void)
{
    /* 首先尝试 thermal_zone0（在 SBC 上最常见）*/
    const char *paths[] = {
        "/sys/class/thermal/thermal_zone0/temp",
        "/sys/class/hwmon/hwmon0/temp1_input",
        NULL
    };

    for (int i = 0; paths[i]; i++) {
        int fd = open(paths[i], O_RDONLY);
        if (fd < 0) continue;
        char buf[16] = {0};
        ssize_t r = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (r > 0) {
            long millideg = strtol(buf, NULL, 10);
            return (float)millideg / 1000.0f;
        }
    }
    return -1.0f;   /* 未找到*/
}

/* ============================================================
 * 任务 3 — 解析 /proc/meminfo
 * ============================================================ */

typedef struct {
    uint64_t total_kb;
    uint64_t free_kb;
    uint64_t available_kb;
    uint64_t buffers_kb;
    uint64_t cached_kb;
} MemInfo;

int parse_meminfo(MemInfo *info)
{
    /* TODO：打开/proc/meminfo
     * 逐行读取，解析"Key: value kB"格式。
     * 填写总计，free，可用，缓冲区，缓存。
     * 返回 0 表示成功，-1 表示错误。*/
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1;
    memset(info, 0, sizeof(*info));
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        uint64_t val = 0;
        if      (sscanf(line, "MemTotal:     %llu kB", (unsigned long long*)&val) == 1) info->total_kb     = val;
        else if (sscanf(line, "MemFree:      %llu kB", (unsigned long long*)&val) == 1) info->free_kb      = val;
        else if (sscanf(line, "MemAvailable: %llu kB", (unsigned long long*)&val) == 1) info->available_kb = val;
        else if (sscanf(line, "Buffers:      %llu kB", (unsigned long long*)&val) == 1) info->buffers_kb   = val;
        else if (sscanf(line, "Cached:       %llu kB", (unsigned long long*)&val) == 1) info->cached_kb    = val;
    }
    fclose(f);
    return 0;
}

/* ============================================================
 * 任务 4 — LED 触发控制（心跳、定时器等）
 *
 * /sys/class/leds/<name>/trigger — 设置触发类型
 * /sys/class/leds/<name>/brightness — 设置 0 或 max_brightness
 * 定时器触发：
 *   /sys/class/leds/<name>/delay_on — 毫秒
 *   /sys/class/leds/<name>/delay_off — 毫秒关闭
 * ============================================================ */

int led_set_trigger(const char *led_name, const char *trigger)
{
    /* TODO：将触发器写入/sys/class/leds/led_name/trigger*/
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/leds/%s/trigger", led_name);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    ssize_t r = write(fd, trigger, strlen(trigger));
    close(fd);
    return (r == (ssize_t)strlen(trigger)) ? 0 : -1;
}

int led_set_brightness(const char *led_name, uint32_t brightness)
{
    char path[128], buf[16];
    snprintf(path, sizeof(path), "/sys/class/leds/%s/brightness", led_name);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    int n = snprintf(buf, sizeof(buf), "%u", (unsigned)brightness);
    ssize_t r = write(fd, buf, (size_t)n);
    close(fd);
    return (r == n) ? 0 : -1;
}

int led_blink_timer(const char *led_name, uint32_t on_ms, uint32_t off_ms)
{
    /* TODO: 1. 将触发器设置为"timer"
     * 2. 将on_ms写入delay_on
     * 3. 将off_ms写入delay_off*/
    if (led_set_trigger(led_name, "timer") < 0) return -1;

    char path[128], buf[16];
    snprintf(path, sizeof(path), "/sys/class/leds/%s/delay_on", led_name);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    int n = snprintf(buf, sizeof(buf), "%u", (unsigned)on_ms);
    write(fd, buf, (size_t)n);
    close(fd);

    snprintf(path, sizeof(path), "/sys/class/leds/%s/delay_off", led_name);
    fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    n = snprintf(buf, sizeof(buf), "%u", (unsigned)off_ms);
    write(fd, buf, (size_t)n);
    close(fd);
    return 0;
}

/* ============================================================
 * 任务 5 — 读取网络接口统计信息
 * ============================================================ */

typedef struct {
    uint64_t rx_bytes;
    uint64_t rx_packets;
    uint64_t rx_errors;
    uint64_t tx_bytes;
    uint64_t tx_packets;
    uint64_t tx_errors;
} NetIfStats;

int read_netif_stats(const char *ifname, NetIfStats *stats)
{
    /* /sys/class/net/<ifname>/statistics/rx_bytes等*/
    struct { const char *file; uint64_t *field; } fields[] = {
        {"rx_bytes",   &stats->rx_bytes},
        {"rx_packets", &stats->rx_packets},
        {"rx_errors",  &stats->rx_errors},
        {"tx_bytes",   &stats->tx_bytes},
        {"tx_packets", &stats->tx_packets},
        {"tx_errors",  &stats->tx_errors},
        {NULL, NULL}
    };
    memset(stats, 0, sizeof(*stats));
    for (int i = 0; fields[i].file; i++) {
        char path[128], buf[32] = {0};
        snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/%s", ifname, fields[i].file);
        int fd = open(path, O_RDONLY);
        if (fd < 0) continue;
        read(fd, buf, sizeof(buf)-1);
        close(fd);
        *fields[i].field = (uint64_t)strtoull(buf, NULL, 10);
    }
    return 0;
}

/* ============================================================
 * 任务 6 — 找错题：sysfs GPIO 使用错误
 *
 * 下面的函数导出一个GPIO并使其闪烁。
 * 它有 3 个错误。
 * ============================================================ */

void gpio_blink_BUGGY(uint32_t pin)
{
    char path[64], buf[8];

    /* Bug 1：导出路径错误*/
    snprintf(path, sizeof(path), "/sys/class/gpio/%u/export", pin);  /* 应该是/sys/class/gpio/export*/
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
        snprintf(buf, sizeof(buf), "%u", (unsigned)pin);
        write(fd, buf, strlen(buf));
        close(fd);
    }

    /* Bug 2：导出后没有睡眠/延迟 - 方向文件可能尚不存在
     * 导出后内核需要时间创建 sysfs 条目*/
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%u/direction", pin);
    fd = open(path, O_WRONLY);
    if (fd >= 0) {
        write(fd, "out", 3);
        close(fd);
    }

    /* Bug 3：值文件在下次写入之前未关闭 - fd 在循环中泄漏*/
    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof(path), "/sys/class/gpio/gpio%u/value", pin);
        fd = open(path, O_WRONLY);
        write(fd, "1", 1);
        /* 缺少：关闭（fd）；*/
        usleep(500000);
        /* fd = open(路径, O_WRONLY); — 但上面的 fd 从未关闭过！*/
        write(fd, "0", 1);
        close(fd);
        usleep(500000);
    }
}

/* ============================================================
 * MAIN — 如果在 Linux 上运行则打印系统信息
 * ============================================================ */

int main(void)
{
    printf("Linux Embedded sysfs demo\n");

    float temp = read_cpu_temp_celsius();
    if (temp > 0)
        printf("CPU temperature: %.1f °C\n", (double)temp);
    else
        printf("Temperature not available (run on Linux target)\n");

    MemInfo mem = {0};
    if (parse_meminfo(&mem) == 0) {
        printf("Memory: total=%llu kB, available=%llu kB\n",
               (unsigned long long)mem.total_kb,
               (unsigned long long)mem.available_kb);
    }

    printf("sysfs demo complete.\n");
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1: /sys/class/gpio (sysfs) 有什么区别
 *     和/dev/gpiochipN（libgpiod）？现在首选哪个？
 *     答案：TODO
 *
 * Q2：你写信给/sys/class/gpio/export并得到EBUSY。
 *     这是什么意思以及如何解决它？
 *     答案：TODO
 *
 * Q3：如何从用户空间读取硬件传感器？
 *     命名内核接口和典型文件路径。
 *     答案：TODO
 *
 * Q4：什么是内核模块？如何装载/卸载？
 *     你什么时候会编写内核驱动程序与用户空间驱动程序？
 *     答案：TODO
 *
 * Q5：解释一下字符设备的区别（/dev/ttyS0）
 *     和 sysfs 属性 (/sys/...)。各自什么时候使用？
 *     答案：TODO
 */

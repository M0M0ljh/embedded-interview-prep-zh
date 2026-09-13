/*
 * 答案：07_Linux_Embedded/01_linux_drivers_sysfs.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1：sysfs 与 /dev — 什么时候使用它们？

答：/dev：字符或块设备文件。使用 open()/read()/write()/ioctl() 访问。
   用于: UART (/dev/ttyS0), SPI (/dev/spidevX.Y), I2C (/dev/i2c-N),
   GPIO（旧版/dev/gpiochip0，通过 ioctl）、视频 (/dev/videoN)、音频。
   这些是流式或事务性的——你交换原始字节/帧。

   sysfs (/sys)：公开内核对象属性的虚拟文件系统。
   每个文件代表一个属性（整数、字符串、十六进制值）。
   使用 open()/read()/write() 或 echo/cat 从 shell 读取/写入。
   用于：GPIO方向/值（旧版sysfs-gpio，已弃用），
   LED 触发/亮度、hwmon 传感器（温度、电压）、
   网络接口统计、电源管理、设备属性。

   规则：sysfs 用于配置和状态属性。
         /dev 用于数据流和命令。
   现代 GPIO：使用 libgpiod (/dev/gpiochipN + ioctl) 而不是 sysfs-gpio。

Q2：sysfs 文件是"virtual"是什么意思？

答：磁盘上不存在/sys中的文件。内核将它们综合起来
   每次读/写。当你阅读/sys/class/gpio/gpio17/value时：
   1. VFS 调用为该属性注册的 sysfs show() 回调。
   2. 回调读取实际的GPIO 寄存器并返回"0\n"或"1\n"。
   3. 字节从不接触存储。
   类似地，对于写入：write()→store()回调→设置GPIO 寄存器。
   无论内容如何，由 stat() 控制的大小 仓库 始终为 4096（页面大小）。
   读取后的文件位置：前进过去的内容→下一次读取返回EOF。
   始终重新打开或 lseek(0) 重新读取当前值。

Q3：为什么 libgpiod 优于 sysfs-gpio？

答：sysfs-gpio（CONFIG_GPIO_SYSFS）问题：
   1. 自 Linux 4.8 起已弃用 — 可能会在未来的内核中删除。
   2. 无法自动读取多个 GPIO 值。
   3. 轮询需要 inotify 或慢速文件读取；没有有效的事件
      低延迟边缘检测通知。
   4. 竞态条件：导出创建 sysfs 文件，但有延迟
      在 udev 设置权限之前 — 脚本必须重试或休眠。
   5. 无法声明 GPIO 独占所有权 — 两个进程可能会发生冲突。

   libgpiod (gpiod_*)：基于 ioctl，使用 /dev/gpiochipN.
   优点：atomic多pin读/写，正确的边缘检测
   （poll()/POLLIN于gpiod_line_event_fd），专线预约，
   积极维护，专为用户空间GPIO设计。
   API：gpiod_chip_open、gpiod_chip_get_line、gpiod_line_request_output、
        gpiod_line_set_value，gpiod_line_release。

Q4：读取CPU温度——哪个文件，单位是什么？

答：路径：/sys/class/thermal/thermal_zone0/temp
   单位：毫摄氏度（整数）。
   示例：读数 "45234" = 45.234°C。
   转换： float temp_c = atoi(buf) / 1000.0f;

   对于硬件监视器（ADT7461、LM75等，通过I2C连接）：
   /sys/class/hwmon/hwmon0/temp1_input（相同的毫度格式）。

   在 Raspberry Pi 上：thermal_zone0 是 SoC (BCM2835/BCM2711) 结温。
   通常在负载下运行 40-60°C。节流阀温度为 80°C。

Q5：/proc/meminfo — MemAvailable 与 MemFree 意味着什么？

答：MemFree：物理RAM 页面中什么都没有——真正闲置。
   MemAvailable（自 Linux 3.14 起）：估计实际内存量
   可用于启动新应用程序而无需交换。
   包括：MemFree + 可回收缓存（页面缓存、slab 可回收）
   减去无法释放的部分。
   为什么差异很重要：
   Linux使用"free"RAM作为磁盘缓存（PageCache）来加速I/O。
   MemFree 忽略此缓存；如果你请求内存，内核会回收缓存。
   MemAvailable 考虑可回收缓存 → 更准确的测量
   "can I start this process?" 无需点击交换。
   对于嵌入式：使用MemAvailable判断内存压力。

Q6: LED 触发器"timer" — 什么 sysfs 文件可以设置闪烁率？

A: 将触发器设置为"timer"后：
   /sys/class/leds/<name>/delay_on — LED 每个周期保持开启状态的毫秒数
   /sys/class/leds/<name>/delay_off — 每个周期 LED 保持关闭状态的毫秒数
   示例：500 毫秒打开，500 毫秒关闭（1 Hz 闪烁）：
   回声 "timer" > /sys/class/leds/led0/trigger
   回声 "500" > /sys/class/leds/led0/delay_on
   回声 "500" > /sys/class/leds/led0/delay_off
   LED子系统使用内核定时器；没有CPU忙等待。
   其他触发器："heartbeat"（内核负载比例）、"mmc0"（磁盘活动）、
   "netdev"（网络流量）、"cpu0"（CPU活动）。
*/

/* ============================================================
 * Helper：将字符串写入sysfs文件
 * ============================================================ */

static int sysfs_write(const char *path, const char *value)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -errno;
    int r = (int)write(fd, value, strlen(value));
    close(fd);
    return (r < 0) ? -errno : 0;
}

static int sysfs_read(const char *path, char *buf, size_t bufsz)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -errno;
    ssize_t n = read(fd, buf, bufsz - 1);
    close(fd);
    if (n < 0) return -errno;
    buf[n] = '\0';
    /* 去掉尾随换行符*/
    if (n > 0 && buf[n-1] == '\n') buf[n-1] = '\0';
    return 0;
}

/* ============================================================
 * 任务 1 — GPIO sysfs 操作
 * ============================================================ */

int sysfs_gpio_export(unsigned int gpio_num)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", gpio_num);
    return sysfs_write("/sys/class/gpio/export", buf);
}

int sysfs_gpio_unexport(unsigned int gpio_num)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", gpio_num);
    return sysfs_write("/sys/class/gpio/unexport", buf);
}

int sysfs_gpio_set_direction(unsigned int gpio_num, const char *direction)
{
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%u/direction", gpio_num);
    return sysfs_write(path, direction);
}

int sysfs_gpio_write(unsigned int gpio_num, uint8_t value)
{
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%u/value", gpio_num);
    return sysfs_write(path, value ? "1" : "0");
}

int sysfs_gpio_read(unsigned int gpio_num, uint8_t *value)
{
    char path[64], buf[4];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%u/value", gpio_num);
    int r = sysfs_read(path, buf, sizeof(buf));
    if (r < 0) return r;
    *value = (uint8_t)atoi(buf);
    return 0;
}

/* ============================================================
 * 任务 2 — CPU 温度
 * ============================================================ */

int read_cpu_temp_celsius(float *temp_out)
{
    char buf[16];
    int r = sysfs_read("/sys/class/thermal/thermal_zone0/temp", buf, sizeof(buf));
    if (r < 0) {
        /* 尝试 hwmon 后备*/
        r = sysfs_read("/sys/class/hwmon/hwmon0/temp1_input", buf, sizeof(buf));
        if (r < 0) return r;
    }
    *temp_out = (float)atoi(buf) / 1000.0f;  /* 毫度 → 摄氏度*/
    return 0;
}

/* ============================================================
 * 任务 3 — /proc/meminfo 解析器
 * ============================================================ */

typedef struct {
    uint64_t mem_total_kb;
    uint64_t mem_free_kb;
    uint64_t mem_available_kb;
    uint64_t buffers_kb;
    uint64_t cached_kb;
} MemInfo;

int parse_meminfo(MemInfo *out)
{
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -errno;

    char line[128];
    memset(out, 0, sizeof(*out));

    while (fgets(line, sizeof(line), f)) {
        uint64_t val;
        if (sscanf(line, "MemTotal: %llu kB", (unsigned long long *)&val) == 1)
            out->mem_total_kb = val;
        else if (sscanf(line, "MemFree: %llu kB", (unsigned long long *)&val) == 1)
            out->mem_free_kb = val;
        else if (sscanf(line, "MemAvailable: %llu kB", (unsigned long long *)&val) == 1)
            out->mem_available_kb = val;
        else if (sscanf(line, "Buffers: %llu kB", (unsigned long long *)&val) == 1)
            out->buffers_kb = val;
        else if (sscanf(line, "Cached: %llu kB", (unsigned long long *)&val) == 1)
            out->cached_kb = val;
    }
    fclose(f);
    return 0;
}

/* ============================================================
 * 任务 4 — LED sysfs 控制
 * ============================================================ */

int led_set_trigger(const char *led_name, const char *trigger)
{
    char path[96];
    snprintf(path, sizeof(path), "/sys/class/leds/%s/trigger", led_name);
    return sysfs_write(path, trigger);
}

int led_set_brightness(const char *led_name, uint8_t brightness)
{
    char path[96], buf[8];
    snprintf(path, sizeof(path), "/sys/class/leds/%s/brightness", led_name);
    snprintf(buf, sizeof(buf), "%u", brightness);
    return sysfs_write(path, buf);
}

int led_blink_timer(const char *led_name, uint32_t delay_on_ms, uint32_t delay_off_ms)
{
    int r;
    char buf[16];

    r = led_set_trigger(led_name, "timer");
    if (r < 0) return r;

    char path[96];
    snprintf(path, sizeof(path), "/sys/class/leds/%s/delay_on", led_name);
    snprintf(buf, sizeof(buf), "%u", delay_on_ms);
    r = sysfs_write(path, buf);
    if (r < 0) return r;

    snprintf(path, sizeof(path), "/sys/class/leds/%s/delay_off", led_name);
    snprintf(buf, sizeof(buf), "%u", delay_off_ms);
    return sysfs_write(path, buf);
}

/* ============================================================
 * 任务 5 — 网络接口统计
 * ============================================================ */

typedef struct { uint64_t rx_bytes, tx_bytes, rx_packets, tx_packets; } NetStats;

int read_netif_stats(const char *ifname, NetStats *out)
{
    char path[96], buf[32];
    int  r;

    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_bytes", ifname);
    r = sysfs_read(path, buf, sizeof(buf));
    if (r < 0) return r;
    out->rx_bytes = (uint64_t)strtoull(buf, NULL, 10);

    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_bytes", ifname);
    r = sysfs_read(path, buf, sizeof(buf));
    if (r < 0) return r;
    out->tx_bytes = (uint64_t)strtoull(buf, NULL, 10);

    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_packets", ifname);
    r = sysfs_read(path, buf, sizeof(buf));
    if (r < 0) return r;
    out->rx_packets = (uint64_t)strtoull(buf, NULL, 10);

    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_packets", ifname);
    r = sysfs_read(path, buf, sizeof(buf));
    if (r < 0) return r;
    out->tx_packets = (uint64_t)strtoull(buf, NULL, 10);

    return 0;
}

/* ============================================================
 * 任务 6 — 找错题 已修复 (gpio_blink_BUGGY)
 *
 * Bug 1：导出路径错误——"/sys/gpio/export"不存在。
 *        正确路径："/sys/class/gpio/export"
 *        修复：使用正确的 sysfs 路径。
 *
 * Bug 2：导出后设置方向前没有延迟。
 *        写入/sys/class/gpio/export后，udev创建gpio
 *        目录并异步设置权限。 竞态条件：
 *        如果方向文件打开得太早，open()返回EACCES或ENOENT。
 *        修复：导出后使用重试循环或 usleep(100000)。
 *        更好：poll() 文件出现。
 *
 * Bug 3：fd 在循环中打开，但在每次迭代时从未关闭。
 *        LED fd 每次迭代都会打开而不关闭 → fd 泄漏。
 *        约 1024 次迭代后：EMFILE（打开文件太多）→ 写入失败。
 *        修复：每次写入后关闭（fd），或在循环前打开一次
 *        和 lseek(fd, 0, SEEK_SET) 在每次写入之前。
 * ============================================================ */

int main(void)
{
    /* 单元测试在主机上运行，主机上不会有/sys/class/gpio，
       所以我们只需验证函数签名编译和
       sysfs_write/sysfs_read 帮助程序处理已知文件。*/

    printf("Linux sysfs answer file compiled successfully.\n");
    printf("Run on target (Linux + GPIO hardware) to test sysfs functions.\n");

    /* 验证 sysfs_read 正常处理丢失的文件*/
    char buf[16];
    int r = sysfs_read("/nonexistent/path", buf, sizeof(buf));
    printf("sysfs_read missing file returned %d (expected negative)\n", r);

    return 0;
}

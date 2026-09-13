/*
 * 答案：08_IoT_Protocols/01_mqtt_coap.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1：MQTT QoS 0、1、2 — 差异以及何时使用它们。

答：QoS 0 — 最多一次（即发即忘）：
   发布者发送一次。无确认。经纪人可能会也可能不会向订户交付。
   开销：只是 PUBLISH 数据包。
   用于：损耗正常的高频遥测（1 Hz 温度），
   实时仪表板，其中缺少一点是可以接受的。

   QoS 1 — 至少一次：
   发布者发送 PUBLISH。代理通过 PUBACK 确认。
   如果没有收到 PUBACK：发布者使用 DUP 标志重新传输。
   订阅者可能会收到重复项（DUP 标志是一个提示，但不能保证）。
   用于：传感器警报、每个事件必须到达的设备状态变化
   但消费者可以处理重复项（幂等处理）。

   QoS 2 — 恰好一次：
   四路握手：PUBLISH → PUBREC → PUBREL → PUBCOMP。
   保证一次交货。最高的开销。
   用于：支付事件、执行器命令（仅打开阀门一次）、
   计算重复项破坏结果的事件。

Q2：MQTT主题通配符——单级（+）与多级（#）。

A：+（加号）：恰好匹配一个主题级别。
   示例：sensors/+/temperature 匹配：
     传感器/厨房/温度 ✓
     传感器/卧室/温度 ✓
     传感器/楼层 1/房间 2/温度 ✗（两个级别）

   # (hash)：匹配零个或多个主题级别。必须是最后一个字符。
   示例：sensors/# 匹配：
     传感器/厨房/温度 ✓
     传感器/卧室/湿度 ✓
     传感器/ ✓（传感器/后为零电平）
   Sensors/#/temperature 无效（# 必须是最后一个）。

   无法组合：根据 MQTT 规范，传感器/+/# 无效。
   $SYS/# 是订阅代理状态的常见模式。

Q3：对于受限设备，MQTT 与 CoAP。

答：MQTT：
   - 基于TCP → 可靠交付、有序、连接开销。
   - 发布/订阅：代理将发布者和订阅者解耦。
   - 适用于：NB-IoT、LTE-M，连接稳定。
   - 所需代理：单点故障。需要持久的 TCP 连接。
   - 最小数据包：~2 字节 (PINGREQ)。连接开销：显着。

   CoAP：
   - 基于UDP→低开销，容忍丢包（可确认模式增加ACK）。
   - 类似 REST：像 HTTP 一样 GET/POST/PUT/DELETE。
   - 适用于：6LoWPAN、Zigbee、Thread、有损网络。
   - 无代理：直接客户端-服务器。 CoAP 观察推送通知。
   - 最小数据包：4 字节固定报头。

   规则：MQTT 适用于具有稳定 IP 连接的云连接设备。
         CoAP 适用于受限网状网络 (802.15.4) 内的 M2M。

Q4：嵌入式上的 TLS — mbedTLS 与 wolfSSL。

答：mbedTLS（以前为 PolarSSL，现在为 Arm 的 Mbed TLS）：
   - Mbed OS、ESP-IDF、Zephyr 的默认值。
   - 模块化配置 (mbedtls_config.h) — 禁用未使用的算法以减小大小。
   - Flash 占用空间：对于 TLS 1.2，使用 RSA + AES，约为 60-100 KB。
   - 良好的文档和活跃的社区。
   - FIPS 140-2 验证：提供商业版本。

   wolfSSL：
   - 专为嵌入式（以前称为 CyaSSL）而设计。
   - 经过 FIPS 140-2 验证，开箱即用 (wolfCrypt FIPS)。
   - 较小的默认占用空间（~20-100 KB，具体取决于配置）。
   - 更多商业级认证。

   均支持：TLS 1.2/1.3、mTLS（相互认证）、PSK 模式。
   PSK（预共享密钥）：无需证书协商，开销最低，
   非常适合双方均已配置的设备到代理。
   使用：MQTT 的端口 8883 通过 TLS（相对于明文的 1883）。

Q5: MQTT PUBLISH 数据包二进制格式。

A：固定头（1字节）：
   位 [7:4] = 数据包类型（3 = 发布）
   位 3 = DUP 标志
   Bits [2:1] = QoS (0/1/2)
   Bit  0     = RETAIN
   QoS=0 的第一个字节，非保留：0x30

   剩余长度（1-4字节，变长编码）：
   如果长度 < 128：1 个字节且 MSB=0。
   如果长度≥128：MSB=1，低7位=低位部分，下一个字节=继续。

   变量头：
   - 主题长度：2字节大端序
   - 主题字符串：N 字节 UTF-8
   - 数据包标识符：2 字节（仅限 QoS 1/2）

   有效负载：剩余字节=消息有效负载。

Q6: MQTT 中的保留标志是什么？

A：当PUBLISH消息的RETAIN=1时：
   代理存储该主题的最后保留消息。
   订阅该主题的任何订阅者（现在或将来）
   订阅后立即收到最后保留的消息。
   使用案例：
   - 设备状态：将"online"/"offline"发布为保留。新订阅者
     立即了解设备状态，无需等待下一次发布。
   - 配置：发布保留的配置消息；新设备实例
     连接和订阅后立即获取当前配置。
   清除保留消息：使用 RETAIN=1 发布空负载 (len=0)。
   每个主题仅保留一条消息。每个新保留的发布都会替换之前的发布。
*/

/* ============================================================
 * 任务 1 — MQTT 主题验证
 * ============================================================ */

int mqtt_topic_valid(const char *topic, uint8_t is_subscription)
{
    if (!topic || *topic == '\0') return 0;

    uint8_t has_hash = 0;
    const char *p = topic;

    while (*p) {
        if (*p == '#') {
            /* '#' 必须是最后一个字符，前面有 '/' 或位于第一个字符*/
            if (*(p + 1) != '\0') return 0;   /* # 不是最后*/
            if (p != topic && *(p-1) != '/') return 0;  /* 例如，"a#" 无效*/
            has_hash = 1;
        } else if (*p == '+') {
            /* “+”必须占据整个级别：前面是“/”或开始，后面是“/”或结束*/
            if (p != topic && *(p-1) != '/') return 0;
            if (*(p+1) != '\0' && *(p+1) != '/') return 0;
        }
        p++;
    }
    (void)has_hash;

    /* 仅在订阅中允许使用通配符，在发布主题中不允许使用通配符*/
    if (!is_subscription) {
        for (p = topic; *p; p++)
            if (*p == '#' || *p == '+') return 0;
    }
    return 1;
}

/* ============================================================
 * 任务 2 — MQTT 剩余长度编码
 * ============================================================ */

uint8_t mqtt_encode_remaining_length(uint32_t length, uint8_t *out)
{
    uint8_t n = 0;
    do {
        uint8_t encoded = (uint8_t)(length % 128u);
        length /= 128u;
        if (length > 0) encoded |= 0x80u;
        out[n++] = encoded;
    } while (length > 0);
    return n;
}

uint32_t mqtt_decode_remaining_length(const uint8_t *in, uint8_t *bytes_used)
{
    uint32_t value = 0;
    uint8_t  shift = 0;
    uint8_t  i = 0;
    do {
        if (i > 3) break;
        value |= (uint32_t)(in[i] & 0x7Fu) << shift;
        shift += 7;
    } while (in[i++] & 0x80u);
    *bytes_used = i;
    return value;
}

/* ============================================================
 * 任务 3 — MQTT 发布数据包生成器
 * ============================================================ */

uint16_t mqtt_build_publish(const char *topic, const uint8_t *payload,
                            uint16_t payload_len, uint8_t qos, uint8_t retain,
                            uint8_t *out, uint16_t out_max)
{
    uint16_t topic_len = (uint16_t)strlen(topic);
    uint32_t remaining = 2u + topic_len + payload_len;
    if (qos > 0) remaining += 2u;  /* 数据包标识符*/

    uint8_t rem_buf[4];
    uint8_t rem_bytes = mqtt_encode_remaining_length(remaining, rem_buf);

    uint16_t total = 1u + rem_bytes + (uint16_t)remaining;
    if (total > out_max) return 0;

    uint16_t i = 0;
    out[i++] = (uint8_t)(0x30u | (qos << 1) | retain);
    memcpy(&out[i], rem_buf, rem_bytes); i += rem_bytes;
    out[i++] = (uint8_t)(topic_len >> 8);
    out[i++] = (uint8_t)(topic_len & 0xFF);
    memcpy(&out[i], topic, topic_len); i += topic_len;
    if (qos > 0) { out[i++] = 0x00; out[i++] = 0x01; }  /* packet id = 1 */
    memcpy(&out[i], payload, payload_len); i += payload_len;
    return i;
}

/* ============================================================
 * 任务 4 — JSON 遥测构建器
 * ============================================================ */

int build_telemetry_json(char *buf, uint16_t bufsz,
                         uint32_t device_id, float temperature,
                         float humidity, uint32_t timestamp)
{
    return snprintf(buf, bufsz,
        "{\"device_id\":%u,\"temperature\":%.2f,"
        "\"humidity\":%.2f,\"timestamp\":%u}",
        device_id, (double)temperature, (double)humidity, timestamp);
}

/* ============================================================
 * 任务 5 — JSON 值提取器
 * ============================================================ */

float json_get_float(const char *json, const char *key)
{
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return 0.0f;
    p += strlen(search);
    while (*p == ' ' || *p == '\t') p++;
    return strtof(p, NULL);
}

int json_get_int(const char *json, const char *key)
{
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return 0;
    p += strlen(search);
    while (*p == ' ' || *p == '\t') p++;
    return atoi(p);
}

/* ============================================================
 * 任务 6 — 找错题：无效主题
 *
 * Bug 1："#/temperature" — '#' 不是最后一个字符。
 *        MQTT 规范 4.7.1.2：使用“#”时，它必须是最后一个字符。
 *        这将导致经纪人拒绝认购。
 *
 * Bug 2：""（空字符串）——根据 MQTT 规范，主题不得为空。
 *        PUBLISH 中的零长度主题无效。经纪人拒绝了。
 *
 * Bug 3："+x" —“+”必须占据整个主题级别。
 *        “+”必须是其级别中的唯一字符（位于“/”分隔符之间）。
 *        "+x" 不是有效的级别通配符。
 *
 * 有效示例：
 *   "sensors/+/temperature" — 有效，+ 匹配一级
 *   "devices/#" — 有效，# 匹配所有剩余级别
 *   "data" — 有效、准确的主题
 *   "a/b/c" — 有效的三级主题
 * ============================================================ */

int main(void)
{
    /* 主题验证*/
    assert(mqtt_topic_valid("sensors/+/temperature", 1) == 1);   /* 有效订阅*/
    assert(mqtt_topic_valid("devices/#", 1) == 1);
    assert(mqtt_topic_valid("#/temperature", 1) == 0);   /* Bug 1: # 不是最后一个*/
    assert(mqtt_topic_valid("", 1) == 0);                /* 错误2：空*/
    assert(mqtt_topic_valid("+x", 1) == 0);              /* Bug 3: + 未满级*/
    assert(mqtt_topic_valid("sensors/temp", 0) == 1);    /* 有效的发布主题*/
    assert(mqtt_topic_valid("sensors/+", 0) == 0);       /* 发布中的通配符 = 无效*/

    /* 剩余长度编码*/
    uint8_t rem[4];
    uint8_t n = mqtt_encode_remaining_length(0, rem);
    assert(n == 1 && rem[0] == 0x00);
    n = mqtt_encode_remaining_length(127, rem);
    assert(n == 1 && rem[0] == 127);
    n = mqtt_encode_remaining_length(128, rem);
    assert(n == 2 && rem[0] == 0x80 && rem[1] == 0x01);
    n = mqtt_encode_remaining_length(16383, rem);
    assert(n == 2);

    uint8_t used;
    uint32_t val = mqtt_decode_remaining_length(rem, &used);
    assert(val == 16383 && used == 2);

    /* 发布数据包*/
    uint8_t pkt[128];
    const char *topic = "sensor/temp";
    const uint8_t payload[] = {'2','5','.','5'};
    uint16_t plen = mqtt_build_publish(topic, payload, 4, 0, 0, pkt, sizeof(pkt));
    assert(plen > 0);
    assert(pkt[0] == 0x30);  /* 发布，QoS=0，无保留*/
    /* 主题长度（字节 2-3）*/
    uint16_t topic_in_pkt = ((uint16_t)pkt[2] << 8) | pkt[3];
    assert(topic_in_pkt == (uint16_t)strlen(topic));

    /* JSON 建造者*/
    char json[128];
    build_telemetry_json(json, sizeof(json), 42, 25.5f, 60.0f, 1000000);
    printf("JSON: %s\n", json);
    assert(strstr(json, "\"device_id\":42") != NULL);

    float t = json_get_float(json, "temperature");
    assert(t > 25.4f && t < 25.6f);

    printf("All MQTT/CoAP answers verified.\n");
    return 0;
}

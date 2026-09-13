/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * 主题：通信协议 — UART 成帧 + CRC
 * 文件：06_Communication_Protocols/01_uart_framing_crc.c
 * ============================================================
 *
 * 设计基于UART的可靠二进制协议是核心
 * 嵌入式技能。这包括框架设计、CRC计算、
 * 和一个完整的数据包编码器/解码器。
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * 理论——二进制协议设计
 * ============================================================
 *
 * 一个设计良好的基于UART的二进制协议需要：
 *
 * 1. 成帧：接收方如何知道数据包从哪里开始/结束？
 *    选项：
 *    a) 带字节填充的开始/结束字节 (SOF/EOF)
 *    b) 长度前缀 (SOF + LENGTH + PAYLOAD + CRC)
 *    c) 定长包（最简单，浪费带宽）
 *
 * 2. 错误检测：我们如何知道数据是否正确？
 *    a) 校验和：XOR 或 sum mod 256（弱）
 *    b) CRC-8：捕获所有 1 位错误和最多 8 位突发错误
 *    c) CRC-16：工业标准（Modbus使用CRC-16/IBM）
 *    d) CRC-32：以太网、USB、ZIP
 *
 * 框架结构（我们的自定义协议）：
 *   [SOF:1][CMD:1][LEN:1][有效负载:LEN][CRC16_LO:1][CRC16_HI:1]
 *
 *   SOF = 0xAA（帧开始）
 *   CMD = 命令/消息类型（0x01=数据，0x02=确认，0xFF=错误）
 *   LEN = 有效负载长度 (0..MAX_PAYLOAD)
 *   PAYLOAD = len 字节的应用程序数据
 *   CRC16 = CRC-16/IBM 通过 CMD+LEN+PAYLOAD (小端序)
 *
 * CRC-16/IBM（由Modbus RTU 使用）：
 *   多项式：0x8005（正态）= 0xA001（反射/反转）
 *   初始化：0xFFFF
 *   输入/输出反映：是（所以我们使用0xA001）
 * ============================================================ */

#define FRAME_SOF        0xAAu
#define MAX_PAYLOAD_SIZE 64u
#define HEADER_SIZE      3u    /* SOF + CMD + LEN*/
#define CRC_SIZE         2u

#define CMD_DATA   0x01u
#define CMD_ACK    0x02u
#define CMD_NACK   0x03u
#define CMD_PING   0x04u
#define CMD_PONG   0x05u

/* ============================================================
 * 任务 1 — CRC-16/IBM (Modbus) 实现
 * ============================================================ */

uint16_t crc16_modbus(const uint8_t *data, uint16_t len)
{
    /* TODO：初始化CRC = 0xFFFF
     * 对于每个字节：
     *   crc ^= 字节
     *   对于 0..7 内的 i：
     *     如果 crc & 1: crc = (crc >> 1) ^ 0xA001
     *     else:       crc >>= 1
     * return crc */
    (void)data; (void)len;
    return 0;
}

/* 表驱动CRC-16（速度更快，用于生产）*/
static uint16_t crc16_table[256];
static uint8_t  g_crc_table_init = 0;

void crc16_build_table(void)
{
    /* TODO：对于 0..255 中的每个 i：
     *   crc = i
     *   对于 0..7 中的位：
     *     如果 crc & 1: crc = (crc >> 1) ^ 0xA001
     *     else:       crc >>= 1
     *   crc16_table[i] = CRC
     * 设置 g_crc_table_init = 1*/
    for (uint16_t i = 0; i < 256; i++) {
        uint16_t crc = i;
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001u : crc >> 1;
        }
        crc16_table[i] = crc;
    }
    g_crc_table_init = 1;
}

uint16_t crc16_modbus_fast(const uint8_t *data, uint16_t len)
{
    /* TODO：如果！g_crc_table_init：crc16_build_table()
     * crc = 0xFFFF
     * 对于每个字节：crc = (crc >> 8) ^ crc16_table[(crc ^ byte) & 0xFF]
     * return crc */
    if (!g_crc_table_init) crc16_build_table();
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc16_table[(crc ^ data[i]) & 0xFFu];
    }
    return crc;
}

/* ============================================================
 * 任务 2 — 帧编码器
 *
 * 在输出缓冲区中构建完整的帧。
 * 返回总帧长度，如果出错则返回 -1。
 * ============================================================ */

int frame_encode(uint8_t cmd, const uint8_t *payload, uint8_t payload_len,
                 uint8_t *out_buf, uint16_t out_max)
{
    /* TODO：验证payload_len <= MAX_PAYLOAD_SIZE*/
    /* TODO：验证 out_max >= HEADER_SIZE + payload_len + CRC_SIZE*/
    /* TODO: out_buf[0] = FRAME_SOF*/
    /* TODO: out_buf[1] = cmd*/
    /* TODO: out_buf[2] = payload_len*/
    /* TODO: memcpy(out_buf + 3, 有效负载, payload_len)*/
    /* TODO：计算CRC超过out_buf[1..2+payload_len]（cmd+len+payload）*/
    /* TODO: out_buf[3+payload_len] = crc & 0xFF（低字节）*/
    /* TODO: out_buf[3+payload_len+1] = (crc >> 8) & 0xFF（高字节）*/
    /* TODO: 返回 HEADER_SIZE + payload_len + CRC_SIZE*/

    if (payload_len > MAX_PAYLOAD_SIZE) return -1;
    uint16_t total = HEADER_SIZE + payload_len + CRC_SIZE;
    if (total > out_max) return -1;

    out_buf[0] = FRAME_SOF;
    out_buf[1] = cmd;
    out_buf[2] = payload_len;
    if (payload && payload_len) memcpy(out_buf + 3, payload, payload_len);

    uint16_t crc = crc16_modbus_fast(out_buf + 1, HEADER_SIZE - 1 + payload_len);
    out_buf[3 + payload_len]     = (uint8_t)(crc & 0xFFu);
    out_buf[3 + payload_len + 1] = (uint8_t)(crc >> 8);

    return (int)total;
}

/* ============================================================
 * 任务 3 — 帧解码器（状态机）
 *
 * 真实的接收器从UARTISR一次处理一个字节。
 * 解码器是一个状态机。
 * ============================================================ */

typedef enum {
    PARSE_WAIT_SOF,
    PARSE_CMD,
    PARSE_LEN,
    PARSE_PAYLOAD,
    PARSE_CRC_LO,
    PARSE_CRC_HI
} ParseState;

typedef struct {
    uint8_t  cmd;
    uint8_t  payload[MAX_PAYLOAD_SIZE];
    uint8_t  len;
    uint16_t crc_received;
    uint16_t crc_computed;
    uint8_t  valid;   /* 1 = 帧完成且 CRC 正常*/
} ParsedFrame;

typedef struct {
    ParseState   state;
    ParsedFrame  frame;
    uint8_t      payload_idx;
} FrameParser;

void parser_reset(FrameParser *p)
{
    p->state = PARSE_WAIT_SOF;
    p->payload_idx = 0;
    memset(&p->frame, 0, sizeof(p->frame));
}

/* 当收到完整的有效帧时返回 1*/
int parser_feed_byte(FrameParser *p, uint8_t byte)
{
    /* TODO：实现状态机：
     * PARSE_WAIT_SOF：如果字节==FRAME_SOF→PARSE_CMD，否则保留
     * PARSE_CMD：保存cmd，→PARSE_LEN
     * PARSE_LEN: 如果 len > MAX_PAYLOAD_SIZE → 复位；否则保存长度
     *                 如果 len == 0 → PARSE_CRC_LO 否则 → PARSE_PAYLOAD
     * PARSE_PAYLOAD：保存到payload[idx++]，如果idx == len → PARSE_CRC_LO
     * PARSE_CRC_LO：保存crc低字节→PARSE_CRC_HI
     * PARSE_CRC_HI：保存crc高字节，计算CRC，验证，设置有效
     *                 return 1
     */
    switch (p->state) {
        case PARSE_WAIT_SOF:
            if (byte == FRAME_SOF) p->state = PARSE_CMD;
            break;
        case PARSE_CMD:
            p->frame.cmd = byte;
            p->state = PARSE_LEN;
            break;
        case PARSE_LEN:
            if (byte > MAX_PAYLOAD_SIZE) { parser_reset(p); break; }
            p->frame.len = byte;
            p->payload_idx = 0;
            p->state = (byte == 0) ? PARSE_CRC_LO : PARSE_PAYLOAD;
            break;
        case PARSE_PAYLOAD:
            p->frame.payload[p->payload_idx++] = byte;
            if (p->payload_idx == p->frame.len) p->state = PARSE_CRC_LO;
            break;
        case PARSE_CRC_LO:
            p->frame.crc_received = byte;
            p->state = PARSE_CRC_HI;
            break;
        case PARSE_CRC_HI: {
            p->frame.crc_received |= (uint16_t)((uint16_t)byte << 8);
            /* 通过 CMD + LEN + PAYLOAD 计算 CRC*/
            uint8_t crc_data[2 + MAX_PAYLOAD_SIZE];
            crc_data[0] = p->frame.cmd;
            crc_data[1] = p->frame.len;
            memcpy(crc_data + 2, p->frame.payload, p->frame.len);
            p->frame.crc_computed = crc16_modbus_fast(crc_data, 2 + p->frame.len);
            p->frame.valid = (p->frame.crc_received == p->frame.crc_computed) ? 1 : 0;
            parser_reset(p);
            return 1;
        }
    }
    return 0;
}

/* ============================================================
 * 任务 4 — 字节填充（转义序列）
 *
 * 问题：如果有效负载包含0xAA（SOF字节）怎么办？
 * 解决方案：字节填充——转义特殊字节。
 *
 * 规则：
 *   负载中0xAA→发送0xBB0x01
 *   负载中0xBB→发送0xBB0x02
 * ============================================================ */

#define STUFF_ESC   0xBBu
#define STUFF_AA    0x01u
#define STUFF_BB    0x02u

int stuff_encode(const uint8_t *src, uint16_t src_len, uint8_t *dst, uint16_t dst_max)
{
    /* TODO：对于 src 中的每个字节：
     *   if byte == 0xAA：将 0xBB 0x01 写入 dst
     *   if byte == 0xBB：将 0xBB 0x02 写入 dst
     *   else: 将字节写入 dst
     * return number of bytes written, or -1 if dst too small */
    uint16_t out = 0;
    for (uint16_t i = 0; i < src_len; i++) {
        if (src[i] == 0xAAu || src[i] == 0xBBu) {
            if (out + 2 > dst_max) return -1;
            dst[out++] = STUFF_ESC;
            dst[out++] = (src[i] == 0xAAu) ? STUFF_AA : STUFF_BB;
        } else {
            if (out + 1 > dst_max) return -1;
            dst[out++] = src[i];
        }
    }
    return (int)out;
}

int stuff_decode(const uint8_t *src, uint16_t src_len, uint8_t *dst, uint16_t dst_max)
{
    /* TODO：编码的反向操作 — 当你看到 0xBB 时，读取下一个字节并取消填充*/
    uint16_t out = 0;
    for (uint16_t i = 0; i < src_len; i++) {
        if (src[i] == STUFF_ESC) {
            if (i + 1 >= src_len) return -1;
            i++;
            dst[out++] = (src[i] == STUFF_AA) ? 0xAAu : 0xBBu;
        } else {
            if (out >= dst_max) return -1;
            dst[out++] = src[i];
        }
    }
    return (int)out;
}

/* ============================================================
 * 任务 5 — 找错题：CRC 和框架错误
 *
 * 下面的编码器有 3 个错误。找到并标记每一个。
 * ============================================================ */

int frame_encode_BUGGY(uint8_t cmd, const uint8_t *payload, uint8_t len,
                       uint8_t *out, uint16_t out_max)
{
    (void)out_max;
    out[0] = FRAME_SOF;
    out[1] = cmd;
    out[2] = len;
    memcpy(out + 3, payload, len);

    /* Bug 1: CRC 计算超出了错误的范围 - SOF 包含在内，但不应该包含在内*/
    uint16_t crc = crc16_modbus_fast(out, HEADER_SIZE + len);   /* 应该从 out+1 开始*/

    /* Bug 2：CRC存储的字节大端序（高字节在前）
     * 协议指定小端序（低字节在前）*/
    out[3 + len]     = (uint8_t)(crc >> 8);     /* Bug：这是高字节*/
    out[3 + len + 1] = (uint8_t)(crc & 0xFFu);  /* Bug：这是低字节*/

    /* Bug 3：返回值不包含CRC字节*/
    return HEADER_SIZE + len;   /* 应该是 HEADER_SIZE + len + CRC_SIZE*/
}

/* ============================================================
 * SELF-TEST
 * ============================================================ */

int main(void)
{
    /* CRC 测试 — 已知 Modbus CRC 值*/
    uint8_t test_data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    uint16_t crc = crc16_modbus_fast(test_data, sizeof(test_data));
    /* 已知良好：{0x01、0x03、0x00、0x00、0x00、0x0A} CRC = 0xC50E*/
    assert(crc == 0xC50Eu);

    /* 编码/解码往返*/
    uint8_t payload[] = {0x11, 0x22, 0x33};
    uint8_t frame[64] = {0};
    int flen = frame_encode(CMD_DATA, payload, sizeof(payload), frame, sizeof(frame));
    assert(flen == (int)(HEADER_SIZE + sizeof(payload) + CRC_SIZE));

    FrameParser parser;
    parser_reset(&parser);
    int complete = 0;
    for (int i = 0; i < flen; i++) {
        complete = parser_feed_byte(&parser, frame[i]);
    }
    assert(complete == 0);   /* 最后一个字节触发CRC检查解析器内部*/
    /* 在最后一个字节之后，解析器重置。通过直接CRC测试进行检查。*/

    /* 字节填充测试*/
    uint8_t src[] = {0xAA, 0x42, 0xBB};
    uint8_t stuffed[16] = {0};
    uint8_t unstuffed[16] = {0};
    int slen = stuff_encode(src, sizeof(src), stuffed, sizeof(stuffed));
    assert(slen == 5);   /* 0xBB 0x01、0x42、0xBB 0x02*/
    int ulen = stuff_decode(stuffed, (uint16_t)slen, unstuffed, sizeof(unstuffed));
    assert(ulen == 3);
    assert(memcmp(src, unstuffed, 3) == 0);

    printf("All protocol framing tests PASSED.\n");
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1：为什么CRC-16优于简单的校验和（XOR/总和）？
 *     答案：TODO
 *
 * Q2：CRC-16/IBM 的汉明距离是多少？
 *     这实际上意味着什么？
 *     答案：TODO
 *
 * Q3：为什么通过 CMD+LEN+PAYLOAD 计算CRC 而不是通过 SOF 计算CRC？
 *     答案：TODO
 *
 * Q4：描述设计二进制串行的完整顺序
 *     对噪声、丢失字节和重置具有鲁棒性的协议。
 *     答案：TODO
 *
 * Q5：什么是COBS（一致开销字节填充）？
 *     它与上面简单的字节填充有何不同？
 *     答案：TODO
 */

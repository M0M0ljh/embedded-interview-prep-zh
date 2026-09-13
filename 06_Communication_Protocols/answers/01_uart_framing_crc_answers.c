/*
 * 答案：06_Communication_Protocols/01_uart_framing_crc.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1：CRC vs 校验和 vs 哈希 — 哪个适用于嵌入式协议？

答：简单校验和（字节总和，XOR）：O(n)，无需硬件。
   检测所有 1 位错误；遗漏许多多位错误和突发错误。
   足以满足：short数据包、非关键数据、Flash闪存页面
   其中位错误并不常见，并在写入时进行验证。

   CRC（循环冗余检查）：除以 GF(2) 上的多项式。
   CRC-16 检测：所有 1 位和 2 位错误、所有奇数位错误
   (CRC-CCITT)，所有长度≤ 16 位的突发错误。
   STM32 上的硬件CRC 外设：单周期计算。
   用于：通信协议（Modbus、CANopen、UART 帧）。

   加密哈希 (SHA-256)：256 位输出、单向、抗冲突。
   检测意外 AND 故意损坏。昂贵（数千个周期）。
   用于：固件更新完整性、安全启动映像验证。

   对于嵌入式协议：CRC-16 是最佳选择。足够快ISR
   使用，比校验和强得多，不需要安全开销。

Q2: 为什么0xA001对于ModbusCRC-16？

答：Modbus 使用CRC-16/IBM（也称为CRC-16/ARC）。
   生成多项式：正常形式的 x^16 + x^15 + x^2 + 1 = 0x8005。
   0xA001 是0x8005 的反射（LSB-第一个）表示。
   反思：串行数据先发送LSB，所以多项式为
   镜像以在简单的位循环中处理从LSB到MSB的位。
   0x8005 反转：bit15↔bit0、bit14↔bit1、... = 0xA001。
   反射算法在 C 中实现起来更简单（基于右移位）
   并且与反射数据上的未反射左移位算法的结果相同。

问题 3：字节填充如何工作以及为什么需要它？

答：问题：SOF 字节（我们协议中的0xAA）可以显示为数据。
   扫描 SOF 的接收器将在包含 0xAA 的数据上触发false。

   字节填充：传输前，扫描 PAYLOAD 中的 保留位 字节
   并逃脱他们：
   0xAA → 0xBB 0x01（SOF 替换）
   0xBB→0xBB0x02（转义字节本身必须转义）

   接收时：当看到0xBB时，设置转义标志，解码下一个字节：
   0x01→原来是0xAA
   0x02→原来是0xBB

   CRC 是根据 UNSTUFFED 数据计算的。馅料对CRC来说是透明的。
   长度field应反映原始数据长度（填充之前）。
   替代方案：具有 0x7E 开始/结束和 0x7D 转义的 HDLC 帧。
   COBS（Constant Overhead Byte Stuffing）更高效：开销
   每 254 个字节最多 1 个字节，空字节（0x00）可以是 SOF。

Q4：状态机解析器与线性解析器。什么时候使用状态机？

A：线性解析器：一次读取整个数据包，索引到缓冲区。
   简单，但需要在解析之前将完整的数据包放入缓冲区中。
   问题：对于流式串行数据，你无法假设数据包边界。
   传入字节在 ISR 中一次到达一个。

   状态机解析器：一次处理一个字节，记住状态。
   状态：WAIT_SOF→CMD→长度→有效负载→CRC_LOW→CRC_HIGH
   每个状态消耗一个字节并转换到下一个状态。
   自然处理：部分数据包、噪声（WAIT_SOF 在坏字节上重置）、
   连续数据包（完成数据包后返回WAIT_SOF）。

   始终对 UART/I2C/SPI 流协议使用状态机。
   仅当你拥有可靠的帧传输（USB，TCP）时才使用线性解析器
   保证完整的数据包交付。

Q5：CRC 计算出错误的数据 - 你如何找到这个错误？

答：系统方法：
   1. 确认CRC涵盖哪些字节：仅有效负载，还是报头+有效负载？
      协议规范具有权威性。大多数协议：CRC涵盖所有内容
      SOF 之后，CRC field 本身之前。
   2. 手动注入已知数据包。手动计算CRC（在线工具或
      Python：crcmod 库）。与你的代码产生的结果进行比较。
   3. 添加调试打印：printf字节被CRC'd，逐字节。
   4. 检查字节序：CRC存储的LE（低字节在前）与BE？ Modbus：LE。
   5. 检查初始值和最终值XOR：CRC-16/IBM：init=0xFFFF，无XOR。
      CRC-16/CCITT：init=0xFFFF 或 0x0000 取决于变体。
   6. 使用Wireshark或逻辑分析仪抓包并验证。

Q6：为什么长度field包含在帧头中，而不是从CRC推断出来？

A：接收方需要知道有效负载要接收多少字节
   在它可以计算CRC之前。没有长度field：
   - 接收器需要以某种方式扫描数据包末尾（基于超时）。
   - 超时会引入可变延迟并且容易出错。
   - 使用字节填充时，帧尾不能是固定的字节序列
     除非它在有效负载中的任何地方都被转义。
   长度 field：接收器读取 LEN 字节 → 根据已知负载计算 CRC
   → 对照收到的 CRC 进行验证。干净且确定性。
   安全说明：长度 field 应根据最大长度进行验证
   在分配缓冲区之前（防止格式错误的数据包上的缓冲区溢出）。
*/

/* ============================================================
 * 任务 1 — CRC-16/Modbus 逐位
 * ============================================================ */

uint16_t crc16_modbus(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 1u)
                crc = (crc >> 1) ^ 0xA001u;
            else
                crc >>= 1;
        }
    }
    return crc;
}

/* ============================================================
 * 任务 2 — CRC-16 表驱动
 * ============================================================ */

static uint16_t g_crc_table[256];
static uint8_t  g_crc_table_ready = 0;

void crc16_build_table(void)
{
    for (uint16_t i = 0; i < 256; i++) {
        uint16_t crc = i;
        for (int b = 0; b < 8; b++) {
            if (crc & 1u) crc = (crc >> 1) ^ 0xA001u;
            else           crc >>= 1;
        }
        g_crc_table[i] = crc;
    }
    g_crc_table_ready = 1;
}

uint16_t crc16_modbus_fast(const uint8_t *data, uint16_t len)
{
    if (!g_crc_table_ready) crc16_build_table();
    uint16_t crc = 0xFFFFu;
    for (uint16_t i = 0; i < len; i++)
        crc = (crc >> 8) ^ g_crc_table[(crc ^ data[i]) & 0xFFu];
    return crc;
}

/* ============================================================
 * 任务 3 — 帧编码
 * ============================================================ */

#define SOF_BYTE   0xAAu
#define FRAME_HDR  4u   /* SOF + CMD + LEN_HI + LEN_LO*/
#define FRAME_CRC  2u

uint16_t frame_encode(uint8_t cmd, const uint8_t *payload, uint16_t payload_len,
                      uint8_t *out_buf, uint16_t out_max)
{
    uint16_t total = FRAME_HDR + payload_len + FRAME_CRC;
    if (total > out_max) return 0;

    out_buf[0] = SOF_BYTE;
    out_buf[1] = cmd;
    out_buf[2] = (uint8_t)(payload_len >> 8);
    out_buf[3] = (uint8_t)(payload_len & 0xFF);
    memcpy(&out_buf[4], payload, payload_len);

    /* CRC 通过 CMD + LEN_HI + LEN_LO + 有效负载*/
    uint16_t crc = crc16_modbus(&out_buf[1], 1u + 2u + payload_len);
    out_buf[4 + payload_len]     = (uint8_t)(crc & 0xFF);  /* CRC 低*/
    out_buf[4 + payload_len + 1] = (uint8_t)(crc >> 8);    /* CRC 高*/

    return total;
}

/* ============================================================
 * 任务 4 — 帧解析器状态机
 * ============================================================ */

typedef enum {
    PARSER_WAIT_SOF = 0,
    PARSER_CMD,
    PARSER_LEN_HI,
    PARSER_LEN_LO,
    PARSER_PAYLOAD,
    PARSER_CRC_LO,
    PARSER_CRC_HI
} ParserState;

typedef struct {
    ParserState state;
    uint8_t     cmd;
    uint16_t    payload_len;
    uint16_t    payload_idx;
    uint8_t     payload_buf[256];
    uint8_t     crc_lo;
    uint8_t     frame_ready;
    uint8_t     frame_error;
} FrameParser;

void parser_init(FrameParser *p)
{
    memset(p, 0, sizeof(*p));
    p->state = PARSER_WAIT_SOF;
}

void parser_feed(FrameParser *p, uint8_t byte)
{
    p->frame_ready = 0;
    p->frame_error = 0;

    switch (p->state) {
    case PARSER_WAIT_SOF:
        if (byte == SOF_BYTE) p->state = PARSER_CMD;
        break;
    case PARSER_CMD:
        p->cmd   = byte;
        p->state = PARSER_LEN_HI;
        break;
    case PARSER_LEN_HI:
        p->payload_len = (uint16_t)byte << 8;
        p->state = PARSER_LEN_LO;
        break;
    case PARSER_LEN_LO:
        p->payload_len |= byte;
        if (p->payload_len == 0) { p->state = PARSER_CRC_LO; break; }
        if (p->payload_len > sizeof(p->payload_buf)) {
            p->frame_error = 1; p->state = PARSER_WAIT_SOF; break;
        }
        p->payload_idx = 0;
        p->state = PARSER_PAYLOAD;
        break;
    case PARSER_PAYLOAD:
        p->payload_buf[p->payload_idx++] = byte;
        if (p->payload_idx >= p->payload_len) p->state = PARSER_CRC_LO;
        break;
    case PARSER_CRC_LO:
        p->crc_lo = byte;
        p->state  = PARSER_CRC_HI;
        break;
    case PARSER_CRC_HI: {
        uint16_t rx_crc = (uint16_t)p->crc_lo | ((uint16_t)byte << 8);
        /* 重建 CRC 的计算结果：[cmd, len_hi, len_lo, payload]*/
        uint8_t hdr[3] = { p->cmd,
                           (uint8_t)(p->payload_len >> 8),
                           (uint8_t)(p->payload_len & 0xFF) };
        uint16_t calc = crc16_modbus(hdr, 3u);
        if (p->payload_len > 0)
            calc = crc16_modbus_fast(p->payload_buf, p->payload_len);
        /* 更简单：像编码器中一样重新计算完整的报头+有效负载*/
        /* 重新编码以验证（匹配frame_encode逻辑）*/
        uint8_t verify[260];
        verify[0] = p->cmd;
        verify[1] = (uint8_t)(p->payload_len >> 8);
        verify[2] = (uint8_t)(p->payload_len & 0xFF);
        memcpy(&verify[3], p->payload_buf, p->payload_len);
        calc = crc16_modbus(verify, 3u + p->payload_len);
        if (calc == rx_crc) p->frame_ready = 1;
        else                p->frame_error = 1;
        p->state = PARSER_WAIT_SOF;
        break;
    }
    }
}

/* ============================================================
 * 任务 5 — 字节填充
 * ============================================================ */

#define STUFF_ESC   0xBBu
#define STUFF_SOF   0xAAu

uint16_t stuff_encode(const uint8_t *in, uint16_t len, uint8_t *out, uint16_t out_max)
{
    uint16_t j = 0;
    for (uint16_t i = 0; i < len; i++) {
        if (in[i] == STUFF_SOF || in[i] == STUFF_ESC) {
            if (j + 2 > out_max) return 0;
            out[j++] = STUFF_ESC;
            out[j++] = (in[i] == STUFF_SOF) ? 0x01u : 0x02u;
        } else {
            if (j + 1 > out_max) return 0;
            out[j++] = in[i];
        }
    }
    return j;
}

uint16_t stuff_decode(const uint8_t *in, uint16_t len, uint8_t *out, uint16_t out_max)
{
    uint16_t j = 0;
    uint8_t  esc = 0;
    for (uint16_t i = 0; i < len; i++) {
        if (esc) {
            if (j >= out_max) return 0;
            out[j++] = (in[i] == 0x01u) ? STUFF_SOF : STUFF_ESC;
            esc = 0;
        } else if (in[i] == STUFF_ESC) {
            esc = 1;
        } else {
            if (j >= out_max) return 0;
            out[j++] = in[i];
        }
    }
    return j;
}

int main(void)
{
    crc16_build_table();

    /* CRC 已知值："123456789" → 0xBB3D 对于 CRC-16/IBM*/
    const uint8_t test_str[] = "123456789";
    uint16_t crc1 = crc16_modbus(test_str, 9);
    uint16_t crc2 = crc16_modbus_fast(test_str, 9);
    assert(crc1 == crc2);  /* 两种算法必须一致*/
    assert(crc1 == 0xBB3Du);

    /* 帧编码/解码往返*/
    uint8_t payload[] = {0x01, 0x02, 0x03};
    uint8_t frame[32];
    uint16_t flen = frame_encode(0x10, payload, 3, frame, sizeof(frame));
    assert(flen > 0 && frame[0] == SOF_BYTE && frame[1] == 0x10);

    FrameParser parser;
    parser_init(&parser);
    for (uint16_t i = 0; i < flen; i++) parser_feed(&parser, frame[i]);
    assert(parser.frame_ready);
    assert(parser.cmd == 0x10);
    assert(parser.payload_len == 3);
    assert(parser.payload_buf[0] == 0x01);

    /* 字节填充*/
    uint8_t raw[]     = {0xAA, 0xBB, 0x01, 0xAA};
    uint8_t stuffed[16], unstuffed[16];
    uint16_t slen = stuff_encode(raw, 4, stuffed, sizeof(stuffed));
    assert(slen == 4 + 3);  /* 2×AA→BB01 + 1×BB→BB02，1个0x01不变*/
    uint16_t ulen = stuff_decode(stuffed, slen, unstuffed, sizeof(unstuffed));
    assert(ulen == 4);
    assert(memcmp(raw, unstuffed, 4) == 0);

    printf("All UART framing/CRC answers verified.\n");
    return 0;
}

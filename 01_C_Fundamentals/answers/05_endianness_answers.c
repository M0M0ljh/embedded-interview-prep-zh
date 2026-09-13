/*
 * 答案：05_endianness.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * 任务 1 — 字节序检测和字节交换
 * ============================================================ */

int system_is_little_endian(void)
{
    /* 将 uint16_t 1 转换为 uint8_t* — 在 LE 上，byte[0]==1*/
    uint16_t x = 1;
    return *((uint8_t *)&x) == 1;
}

uint16_t bswap16(uint16_t x)
{
    return (uint16_t)((x << 8) | (x >> 8));
}

uint32_t bswap32(uint32_t x)
{
    return ((x & 0x000000FFu) << 24) |
           ((x & 0x0000FF00u) <<  8) |
           ((x & 0x00FF0000u) >>  8) |
           ((x & 0xFF000000u) >> 24);
}

uint64_t bswap64(uint64_t x)
{
    return ((uint64_t)bswap32((uint32_t)(x & 0xFFFFFFFFu)) << 32) |
           ((uint64_t)bswap32((uint32_t)(x >> 32)));
}

/* ============================================================
 * 任务 2 — 从字节缓冲区解析 big/小端序
 * ============================================================ */

uint16_t read_be16(const uint8_t *buf)
{
    return (uint16_t)(((uint16_t)buf[0] << 8) | buf[1]);
}

uint32_t read_be32(const uint8_t *buf)
{
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] <<  8) |  (uint32_t)buf[3];
}

uint16_t read_le16(const uint8_t *buf)
{
    return (uint16_t)(buf[0] | ((uint16_t)buf[1] << 8));
}

uint32_t read_le32(const uint8_t *buf)
{
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

/* ============================================================
 * 任务 3 — 将 big/小端序 写入字节缓冲区
 * ============================================================ */

void write_be16(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val & 0xFFu);
}

void write_be32(uint8_t *buf, uint32_t val)
{
    buf[0] = (uint8_t)(val >> 24);
    buf[1] = (uint8_t)(val >> 16);
    buf[2] = (uint8_t)(val >>  8);
    buf[3] = (uint8_t)(val & 0xFFu);
}

void write_le16(uint8_t *buf, uint16_t val)
{
    buf[0] = (uint8_t)(val & 0xFFu);
    buf[1] = (uint8_t)(val >> 8);
}

/* ============================================================
 * 任务 4 — Modbus TCP 标题
 * ============================================================ */

typedef struct {
    uint16_t transaction_id;
    uint16_t protocol_id;
    uint16_t length;
    uint8_t  unit_id;
    uint8_t  function_code;
} ModbusTCPHeader;

int parse_modbus_tcp_header(const uint8_t *frame, uint16_t frame_len,
                             ModbusTCPHeader *out)
{
    if (frame_len < 8) return -1;
    out->transaction_id = read_be16(frame + 0);
    out->protocol_id    = read_be16(frame + 2);
    if (out->protocol_id != 0x0000u) return -1;   /* Modbus 必须为 0*/
    out->length         = read_be16(frame + 4);
    out->unit_id        = frame[6];
    out->function_code  = frame[7];
    return 0;
}

void build_modbus_tcp_request(uint8_t *out, uint16_t transaction_id,
                               uint8_t unit_id, const uint8_t *pdu, uint8_t pdu_len)
{
    write_be16(out + 0, transaction_id);
    write_be16(out + 2, 0x0000u);                        /* 协议号*/
    write_be16(out + 4, (uint16_t)(1u + pdu_len));       /* unit_id字节+PDU*/
    out[6] = unit_id;
    memcpy(out + 7, pdu, pdu_len);
}

/* ============================================================
 * 任务 5 — CAN Intel 信号提取
 * ============================================================ */

uint64_t can_extract_intel_signal(const uint8_t *payload, uint8_t dlc,
                                   uint8_t start_bit, uint8_t bit_length)
{
    if (dlc > 8 || bit_length > 64) return 0;
    uint64_t raw = 0;
    for (uint8_t i = 0; i < dlc; i++)
        raw |= ((uint64_t)payload[i] << (i * 8));
    uint64_t mask = (bit_length == 64) ? 0xFFFFFFFFFFFFFFFFULL : ((1ULL << bit_length) - 1);
    return (raw >> start_bit) & mask;
}

/* ============================================================
 * 任务 6 — 找错题 已修复
 *
 * 错误 1：buf[0] | (buf[1] << 8) — 这是 小端序 ​​而不是 大端序。
 *        对于 大端序： (buf[0] << 8) |缓冲区[1]。
 *        修复： pkt.temp_centideg = (int16_t)(((uint16_t)buf[0] << 8) | buf[1]);
 *
 * Bug 2: *(uint16_t*)(buf + 2) — 两道题：
 *        (a) 严格别名违规：通过 uint16_t* 访问 uint8_t 数组。
 *        (b) 潜在的错位：buf+2 可能不是 2 字节对齐的。
 *        两者都是UB。使用显式的逐字节读取。
 *        修复：raw_pressure = (uint16_t)(((uint16_t)buf[2] << 8) | buf[3]);
 *
 * 错误 3：(buf[4] << 8) — buf[4] 是 uint8_t。当 buf[4] >= 0x80（第 7 位设置）时，
 *        整数提升使其成为移位之前的signed int，
 *        并且符号扩展给出0xFFFFxx80，使得OR结果错误。
 *        修复：((uint16_t)buf[4] << 8) | buf[5] — 首先转换为 uint16_t。
 * ============================================================ */

typedef struct { int16_t temp_centideg; uint16_t pressure_pa; uint16_t humid_tenth; } SensorPacket;

SensorPacket parse_sensor_packet_FIXED(const uint8_t *buf)
{
    SensorPacket pkt;
    pkt.temp_centideg = (int16_t)(((uint16_t)buf[0] << 8) | buf[1]);   /* 是，signed*/
    pkt.pressure_pa   = (uint16_t)(((uint16_t)buf[2] << 8) | buf[3]);  /* 是，unsigned*/
    pkt.humid_tenth   = (uint16_t)(((uint16_t)buf[4] << 8) | buf[5]);  /* BE，先施放*/
    return pkt;
}

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1：在运行时检测字节序的单行代码。

答：int is_le = (*(uint8_t *)&(uint16_t){1}) == 1;
   说明：复合文字 (uint16_t){1} 存储值 0x0001。
   在 LE 上：存储为 [01][00] → byte[0] == 1 → true。
   BE 上：存储为 [00][01] → byte[0] == 0 → false。

Q2：你收到的uint32_t超过TCP。使用之前必须做什么？

A：从网络字节序（大端序）转换到主机字节序：
   uint32_t host_val = ntohl(network_val);
   在 大端序 主机上：ntohl() 是无操作。
   在 小端序 主机（x86、ARM）上：ntohl() 反转字节。
   切勿直接使用原始值——这在 LE 主机上会出错。

Q3：CAN信号EngineSpeed：start_bit=0，长度=16，Intel，因子=0.25，偏移=0。
    有效负载[0xE8][0x03]。物理价值？

答：英特尔字节序，start_bit=0，长度=16。
   raw = 有效负载为 uint64_t LE = 0x03E8 （来自字节 [E8][03] → LE = 0x03E8 = 1000）
   信号 = (原始 >> 0) & 0xFFFF = 1000
   物理 = 1000 * 0.25 + 0 = 250 RPM...等等：
   Actually 0x03E8 = 1000. Physical = 1000 * 0.25 = 250.
   （但典型的 EngineSpeed：系数=0.25，因此 4000 RPM = 原始 16000 = 0x3E80）

Q4: 为什么 *(uint16_t*)(buf+1) 即使在 小端序 ARM 上也是危险的？

答：有两个原因：
   (1) 严格别名：编译器假定uint8_t*和uint16_t*从不使用别名。
       通过uint16_t*读取通过uint8_t*存储的内容是未定义行为 —
       编译器可以使用缓存的寄存器值而不是从内存中读取。
   (2) 错位：buf+1 位于奇数地址。在 Cortex-M0/M0+ 上，未对齐
       半字加载会导致 HardFault。在 M3/M4 上它可以工作，但可能会很慢。
   安全替代方案：memcpy(&val, buf+1, 2)；  - 始终定义，始终对齐。

Q5: 什么是htonl()？为什么UDP需要它？

答：htonl() = 主机到网络长。转换来自主机的 32 位值 字节序
   至网络字节序 (大端序)。
   POSIX 套接字期望网络中的所有多字节头字段字节序。
   如果你在 小端序 主机上发送没有 htonl() 的 uint32_t：
   值0x00000001在内存中存储为[01][00][00][00]，
   但你需要在线上[00][00][00][01]。
   ntohl() 是相反的：Network TO Host Long（在 recvfrom() 之后使用）。
*/

int main(void)
{
    const uint8_t be_data[] = {0x12, 0x34, 0x56, 0x78};
    const uint8_t le_data[] = {0x78, 0x56, 0x34, 0x12};

    assert(read_be16(be_data) == 0x1234u);
    assert(read_be32(be_data) == 0x12345678u);
    assert(read_le16(le_data) == 0x1234u);
    assert(read_le32(le_data) == 0x12345678u);

    assert(bswap16(0xAABBu) == 0xBBAAu);
    assert(bswap32(0xAABBCCDDu) == 0xDDCCBBAAu);
    assert(bswap64(0x0102030405060708ULL) == 0x0807060504030201ULL);

    uint8_t out[4] = {0};
    write_be32(out, 0x12345678u);
    assert(out[0]==0x12 && out[1]==0x34 && out[2]==0x56 && out[3]==0x78);

    /* CAN 信号测试*/
    uint8_t can_data[] = {0xE8, 0x03, 0,0,0,0,0,0};
    assert(can_extract_intel_signal(can_data, 8, 0, 16) == 0x03E8u);

    /* 传感器数据包固定解析*/
    uint8_t sp[] = {0x01, 0x2C, 0x00, 0x64, 0x01, 0xF4}; /* 温度=300，压力=100，嗡嗡声=500*/
    SensorPacket pkt = parse_sensor_packet_FIXED(sp);
    assert(pkt.temp_centideg == 300);
    assert(pkt.pressure_pa   == 100);
    assert(pkt.humid_tenth   == 500);

    printf("Endianness: %s\n", system_is_little_endian() ? "little-endian" : "big-endian");
    printf("All endianness answers verified.\n");
    return 0;
}

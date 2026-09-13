/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * 主题：字节序——检测、转换、协议解析
 * 文件：01_C_Fundamentals/05_endianness.c
 * ============================================================
 *
 * 在使用以下内容时，你将被问及字节顺序：
 * CAN、以太网、UART 协议、Modbus、SOME/IP、USB、BLE。
 * 大多数 MCU 为小端序。大多数网络协议是大端序。
 * 犯这个错误=无声的数据损坏。
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * THEORY
 * ============================================================
 *
 * 小端序 (LE)：LSB 位于最低地址
 *   值0x12345678存储为：[78][56][34][12]
 *   使用者：x86、ARM（默认）、RISC-V（默认）
 *
 * 大端序（比利时）：MSB 位于最低地址
 *   值0x12345678存储为：[12][34][56][78]
 *   使用者：网络协议 (htons/htonl)、Motorola、SPARC
 *            CAN 信号可以是（DBC 指定每个信号）
 *
 * 混合（PDP-endian）：罕见，过时
 *
 * 网络 字节序 = 大端序
 *   htons() = 主机到网络short（16 位）
 *   htonl() = 主机到网络long（32 位）
 *   ntohs() = 网络到主机short
 *   ntohl() = 网络到主机long
 *
 * CAN 信号：
 *   Intel 字节序 = 小端序（起始位 = LSB 位置）
 *   Motorola 字节序 = 大端序（起始位 = MSB 位置）
 * ============================================================ */


/* ============================================================
 * 任务 1 — 字节序检测和手动字节交换
 * ============================================================ */

int system_is_little_endian(void)
{
    /* TODO：运行时检测 — 没有库调用*/
    return -1;
}

uint16_t bswap16(uint16_t x)
{
    /* TODO：交换字节：0xAABB → 0xBBAA*/
    (void)x; return 0;
}

uint32_t bswap32(uint32_t x)
{
    /* TODO：交换字节：0xAABBCCDD → 0xDDCCBBAA*/
    (void)x; return 0;
}

uint64_t bswap64(uint64_t x)
{
    /* TODO：交换64位值的字节
     * 提示：在每一半上使用 bswap32 并交换一半*/
    (void)x; return 0;
}

/* ============================================================
 * 任务 2 — 从字节缓冲区中解析 大端序 多字节值
 *
 * 这是解析协议帧时最常见的操作：
 * Modbus TCP、SOME/IP、UDP 报头、BLE ATT PDU。
 * ============================================================ */

uint16_t read_be16(const uint8_t *buf)
{
    /* TODO：从buf读取2个字节大端序
     * buf[0] = MSB，buf[1] = LSB
     * return as host uint16_t */
    (void)buf; return 0;
}

uint32_t read_be32(const uint8_t *buf)
{
    /* TODO：读取4个字节大端序*/
    (void)buf; return 0;
}

uint16_t read_le16(const uint8_t *buf)
{
    /* TODO：读取2个字节小端序
     * buf[0] = LSB，buf[1] = MSB*/
    (void)buf; return 0;
}

uint32_t read_le32(const uint8_t *buf)
{
    /* TODO：读取4个字节小端序*/
    (void)buf; return 0;
}

/* ============================================================
 * 任务 3 — 将 大端序 值写入字节缓冲区
 * （序列化传出协议帧）
 * ============================================================ */

void write_be16(uint8_t *buf, uint16_t val)
{
    /* TODO：buf[0] = MSB，buf[1] = LSB*/
    (void)buf; (void)val;
}

void write_be32(uint8_t *buf, uint32_t val)
{
    /* TODO: buf[0..3] 大端序*/
    (void)buf; (void)val;
}

void write_le16(uint8_t *buf, uint16_t val)
{
    /* TODO: buf[0] = LSB, buf[1] = MSB*/
    (void)buf; (void)val;
}

/* ============================================================
 * 任务 4 — Modbus TCP MBAP 报头解析
 *
 * Modbus TCP 报头（6 字节，所有字段大端序）：
 *   [0:1] 交易 ID — uint16_t
 *   [2:3] 协议 ID — uint16_t（始终为 0x0000）
 *   [4:5] 长度 — uint16_t（后面的字节）
 *
 * 然后是 PDU：
 *   [6] 单位 ID — uint8_t
 *   [7] 功能码 — uint8_t
 *   [8..] Data
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

    /* TODO: 输出->transaction_id = read_be16(帧 + 0)*/
    /* TODO: 输出->protocol_id = read_be16(帧 + 2)*/
    /* TODO：验证protocol_id == 0*/
    /* TODO: 出->长度 = read_be16(帧 + 4)*/
    /* TODO: 输出->unit_id = 帧[6]*/
    /* TODO: 输出->function_code = 帧[7]*/

    (void)frame; (void)out;
    return 0;
}

void build_modbus_tcp_request(uint8_t *out, uint16_t transaction_id,
                               uint8_t unit_id, const uint8_t *pdu, uint8_t pdu_len)
{
    /* TODO: write_be16(输出 + 0, transaction_id)*/
    /* TODO: write_be16(out + 2, 0x0000) — 协议 ID*/
    /* TODO: write_be16(out + 4, (uint16_t)(1 + pdu_len)) — unit_id 字节 + PDU*/
    /* TODO: 输出[6] = unit_id*/
    /* TODO：memcpy（输出+7，pdu，pdu_len）*/
    (void)out; (void)transaction_id; (void)unit_id; (void)pdu; (void)pdu_len;
}

/* ============================================================
 * 任务 5 — CAN Intel 与 Motorola 信号提取
 *
 * 在CAN DBC 文件中，每个信号指定：
 *   - start_bit：LSB（Intel）或MSB（摩托罗拉）的位位置
 *   - bit_length：位数
 *   - byte_order：0=摩托罗拉，1=英特尔
 *
 * 英特尔（小端序）：
 *   start_bit = LSB 位置，从字节 0 的位 0 开始计数
 *   摘录：(原始>>start_bit)&((1<<bit_length)-1)
 *
 * 摩托罗拉更为复杂——请参阅answers/了解完整实现。
 * ============================================================ */

uint64_t can_extract_intel_signal(const uint8_t *payload, uint8_t dlc,
                                   uint8_t start_bit, uint8_t bit_length)
{
    if (dlc > 8 || bit_length > 64) return 0;

    /* 将有效负载复制到uint64_t (小端序)*/
    uint64_t raw = 0;
    /* TODO：对于 0..dlc-1 中的 i：原始 |= ((uint64_t)payload[i] << (i*8))*/

    /* TODO：提取物：（原始>>start_bit）和面膜
     * 其中掩码 = (bit_length == 64) ？ 0xFFFFFFFFFFFFFFFF : (1ULL << bit_length) - 1*/

    (void)payload; (void)dlc; (void)start_bit; (void)bit_length;
    return raw;
}

/* ============================================================
 * 任务 6 — 找错题：传感器数据解析器中的字节顺序错误
 *
 * 下面的函数解析来自网络设备的 6 字节传感器数据包。
 * 数据包格式（所有字段大端序）：
 *   [0:1] 温度 0.01°C (int16_t)
 *   [2:3] 压力（单位：Pa） (uint16_t)
 *   [4:5] 0.1% 湿度 (uint16_t)
 * 它有 3 个错误。找到并标记每一个。
 * ============================================================ */

typedef struct { int16_t temp_centideg; uint16_t pressure_pa; uint16_t humid_tenth; } SensorPacket;

SensorPacket parse_sensor_packet_BUGGY(const uint8_t *buf)
{
    SensorPacket pkt;

    /* 错误1：？？？*/
    pkt.temp_centideg = (int16_t)(buf[0] | (buf[1] << 8));   /* LE 不是 BE*/

    /* 错误2：？？？*/
    uint16_t raw_pressure = *(uint16_t*)(buf + 2);            /* 未对齐 + 锯齿 UB*/

    /* 错误3：？？？*/
    pkt.humid_tenth = (uint16_t)((buf[4] << 8) | buf[5]);    /* buf[4] 是uint8_t，
                                                                * 移位乘8就可以了，
                                                                * 但是：如果 buf[4] >= 0x80
                                                                * 在OR之前签署扩展名。
                                                                * 修复：首先转换为uint16_t*/
    pkt.pressure_pa  = raw_pressure;
    return pkt;
}

/* ============================================================
 * SELF-TEST
 * ============================================================ */

static void test_endianness(void)
{
    const uint8_t be_data[] = {0x12, 0x34, 0x56, 0x78};
    const uint8_t le_data[] = {0x78, 0x56, 0x34, 0x12};

    assert(read_be16(be_data) == 0x1234);
    assert(read_be32(be_data) == 0x12345678);
    assert(read_le16(le_data) == 0x1234);
    assert(read_le32(le_data) == 0x12345678);

    assert(bswap16(0xAABB) == 0xBBAA);
    assert(bswap32(0xAABBCCDD) == 0xDDCCBBAA);

    uint8_t out[4] = {0};
    write_be32(out, 0x12345678);
    assert(out[0]==0x12 && out[1]==0x34 && out[2]==0x56 && out[3]==0x78);

    printf("All endianness tests PASSED.\n");
}

int main(void)
{
    test_endianness();
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1：编写一行代码来在运行时检测字节序。
 *     答案：TODO
 *
 * Q2：你通过 TCP 套接字收到uint32_t。你必须做什么
 *     在 x86 主机上使用它的值之前？
 *     答案：TODO
 *
 * Q3：CAN信号"EngineSpeed"定义为：
 *     start_bit=0，长度=16，byte_order=英特尔，因子=0.25，偏移=0。
 *     有效负载字节：[0xE8][0x03][...]。
 *     物理价值是多少？
 *     答案：TODO
 *
 * Q4: 为什么 *(uint16_t*)(buf+1) 即使在 小端序 ARM 上也是危险的？
 *     答案：TODO
 *
 * Q5: 什么是htonl()以及为什么在发送uint32_t时需要它
 *     通过UDP插座？
 *     答案：TODO
 */

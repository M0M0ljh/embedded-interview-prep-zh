/*
 * 答案：06_Communication_Protocols/03_modbus_can.c
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * INTERVIEW QUESTION ANSWERS
 * ============================================================

Q1：Modbus FC=03 vs FC=06 vs FC=16。

A：FC=03（读保持寄存器）：主机请求N个寄存器开始
   在地址 ADDR 处。从机响应 2×N 个字节（每个reg = 2 个字节，BE）。
   用于：读取传感器值、状态寄存器、计数器。

   FC=06（写入单个寄存器）：主机向 ADDR 写入 2 个字节（一个寄存器）。
   从机回显请求作为确认。
   用于：设置设定点、写入单个配置值。

   FC=16（写入多个寄存器）：主机写入从 ADDR 开始的 N 个寄存器。
   从机以 ADDR + QTY 响应（回显，6 字节）。
   用于：写入配置块、电机命令（速度+方向+斜坡）。

Q2：ModbusRTUCRC字节序。

A: Modbus RTU CRC-16: 小端序（帧中低字节在前）。
   计算uint16_t CRC后：
   帧[n] = crc & 0xFF;     // 低字节
   帧[n+1] = crc>>8；       // 高字节
   CRC 多项式 (0xA001) 已反映用于 LSB 优先处理。
   Modbus帧中的寄存器地址和数据值是大端序。
   常见错误：将 CRC 存储为 大端序 → 设备拒绝每一帧。

Q3：CAN 总线仲裁——谁赢了？

答：CAN 对标识符field 使用非破坏性按位仲裁。
   规则：显性位（0）胜过隐性位（1）。
   所有节点从标识符的位 0 (MSB) 开始同时传输。
   每个节点都会将其传输的内容与其读回的内容进行比较。
   如果一个节点传输隐性 (1) 但读取显性 (0)：另一个节点
   传输 0 → 该节点获胜 → 我们的节点停止传输并且
   成为接收者。当总线空闲时它将重试。
   较低的CAN ID = 较高优先级（前导 0 位越多，越早赢得仲裁）。
   ID=0x000 的帧战胜所有其他帧。
   这就是为什么安全关键消息（例如制动命令）的 ID 较低。

Q4：CAN 错误计数器 — TEC/REC。什么触发Bus-Off？

A：TEC（传输错误计数器）：传输错误时递增（+8
   大多数错误）。传输成功后减 1。
   REC（接收错误计数器）：与接收错误类似。

   国家：
   错误主动（正常）：TEC < 128 && REC < 128
   被动错误：TEC ≥ 128 OR REC ≥ 128
     在Error-Passive中：节点发送被动错误帧（隐性位），
     依然可以传送。
   Bus-Off：TEC ≥ 256
     节点与总线完全断开。无法发送或接收。
     恢复：11 个连续隐性位出现 128 次（总线空闲）。

   Bus-Off 中的节点通常是硬件故障的标志（short 电路，
   错误的比特率或断开的电缆）并且可能需要MCU重置。

Q5：CAN 11 位标识符与 29 位标识符（CAN 2.0A 与 2.0B）。

答：CAN 2.0A（标准帧）：11 位标识符 → 2048 个可能的 ID。
   仲裁 field = 11 位。更小的 SOF + ID + RTR + 控制 = 更少的开销。
   CAN 2.0B（扩展帧）：29位标识符→5.36亿个ID。
   使用场合：大型系统（具有数百个 ECU 的汽车）、SAE J1939
   （卡车/公共汽车）、ISO 15765 (OBD-II)。
   帧头中的 IDE 位区分标准与扩展。
   设置为 2.0B 的CAN 控制器可以接收标准帧和扩展帧。
   在同一网络中：如果前 11 位为扩展帧，则扩展帧的优先级较低
   匹配标准帧 ID（IDE 位为隐性=1，输给显性=0）。

Q6：用于信号提取的CAN Intel 字节序 — 是什么意思？

答：CAN 数据库（DBC 文件）define Intel 信号 (小端序) 或
   摩托罗拉 (大端序) 字节序。
   Intel 字节序：起始位是信号的LSB。
   信号跨越多个字节：LSB位于start_bit位置，
   并且信号向更高位位置增长（包括跨字节）
   小端序顺序：bit 7→bit 8表示字节0的bit 7到字节1的bit 0）。
   Motorola 字节序：起始位是信号的 MSB。
   英特尔提取：
   1. 从字节（start_bit/8）开始从帧中提取 N 个字节。
   2. 视为 uint64_t 小端序。
   3. 移位 右边 (start_bit % 8)。
   4. 掩码到信号长度：&((1<<length)-1).
   5. Sign-extend if signed.
*/

/* ============================================================
 * CRC-16/Modbus
 * ============================================================ */

uint16_t crc16_modbus(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc & 1u) ? ((crc >> 1) ^ 0xA001u) : (crc >> 1);
    }
    return crc;
}

/* ============================================================
 * TASK 1 — Modbus FC=03 request builder
 * ============================================================ */

uint8_t modbus_build_fc03_request(uint8_t slave_id, uint16_t start_addr,
                                  uint16_t quantity, uint8_t *out)
{
    out[0] = slave_id;
    out[1] = 0x03;
    out[2] = (uint8_t)(start_addr >> 8);    /* Big-endian*/
    out[3] = (uint8_t)(start_addr & 0xFF);
    out[4] = (uint8_t)(quantity >> 8);
    out[5] = (uint8_t)(quantity & 0xFF);
    uint16_t crc = crc16_modbus(out, 6u);
    out[6] = (uint8_t)(crc & 0xFF);         /* CRC: low byte first (LE)*/
    out[7] = (uint8_t)(crc >> 8);
    return 8u;
}

/* ============================================================
 * TASK 2 — Modbus FC=03 response parser
 * ============================================================ */

int modbus_parse_fc03_response(const uint8_t *resp, uint16_t resp_len,
                               uint16_t *regs_out, uint8_t max_regs)
{
    if (resp_len < 5) return -1;

    uint8_t byte_count = resp[2];
    if (byte_count % 2 != 0) return -2;
    uint8_t num_regs = byte_count / 2u;
    if (num_regs > max_regs) return -3;
    if (resp_len < (uint16_t)(3u + byte_count + 2u)) return -4;

    /* Verify CRC*/
    uint16_t calc_crc = crc16_modbus(resp, (uint16_t)(3u + byte_count));
    uint16_t rx_crc   = (uint16_t)resp[3 + byte_count] |
                        ((uint16_t)resp[3 + byte_count + 1] << 8);
    if (calc_crc != rx_crc) return -5;

    for (uint8_t i = 0; i < num_regs; i++) {
        regs_out[i] = ((uint16_t)resp[3 + i*2] << 8) | resp[3 + i*2 + 1];
    }
    return num_regs;
}

/* ============================================================
 * TASK 3 — Modbus FC=16 builder
 * ============================================================ */

uint16_t modbus_build_fc10_request(uint8_t slave_id, uint16_t start_addr,
                                   const uint16_t *regs, uint8_t num_regs,
                                   uint8_t *out, uint16_t out_max)
{
    uint16_t total = (uint16_t)(7u + 2u * num_regs + 2u);
    if (total > out_max) return 0;

    out[0] = slave_id;
    out[1] = 0x10;
    out[2] = (uint8_t)(start_addr >> 8);
    out[3] = (uint8_t)(start_addr & 0xFF);
    out[4] = 0x00;
    out[5] = num_regs;
    out[6] = (uint8_t)(num_regs * 2u);   /* byte count*/

    for (uint8_t i = 0; i < num_regs; i++) {
        out[7 + i*2]     = (uint8_t)(regs[i] >> 8);
        out[7 + i*2 + 1] = (uint8_t)(regs[i] & 0xFF);
    }
    uint16_t crc = crc16_modbus(out, (uint16_t)(7u + 2u * num_regs));
    out[7 + 2*num_regs]     = (uint8_t)(crc & 0xFF);
    out[7 + 2*num_regs + 1] = (uint8_t)(crc >> 8);
    return total;
}

/* ============================================================
 * TASK 4 — CAN frame validation
 * ============================================================ */

typedef struct {
    uint32_t id;
    uint8_t  dlc;
    uint8_t  data[8];
    uint8_t  is_extended;
} CanFrame;

typedef enum { CAN_OK=0, CAN_ERR_DLC, CAN_ERR_ID } CanValidResult;

CanValidResult can_frame_validate(const CanFrame *f)
{
    if (f->dlc > 8) return CAN_ERR_DLC;
    if (f->is_extended && f->id > 0x1FFFFFFFu) return CAN_ERR_ID;
    if (!f->is_extended && f->id > 0x7FFu)     return CAN_ERR_ID;
    return CAN_OK;
}

/* ============================================================
 * TASK 5 — CAN Intel byte order signal extraction
 * ============================================================ */

int32_t can_signal_extract(const uint8_t *data, uint8_t dlc,
                           uint8_t start_bit, uint8_t length,
                           uint8_t is_signed)
{
    (void)dlc;
    /* Intel byte order: start_bit is the LSB position.
       Build a 64-bit value from the data bytes, then extract.*/
    uint64_t raw = 0;
    for (int i = 0; i < 8; i++)
        raw |= (uint64_t)data[i] << (i * 8);

    uint64_t mask = (length == 64) ? ~0ULL : ((1ULL << length) - 1ULL);
    uint64_t val  = (raw >> start_bit) & mask;

    if (is_signed && (val >> (length - 1))) {
        /* Sign extend*/
        val |= ~mask;
        return (int32_t)(int64_t)val;
    }
    return (int32_t)val;
}

/* ============================================================
 * TASK 6 — CAN error state
 * ============================================================ */

typedef enum { CAN_STATE_ACTIVE=0, CAN_STATE_PASSIVE, CAN_STATE_BUS_OFF } CanErrorState;

CanErrorState can_get_error_state(uint8_t tec, uint8_t rec)
{
    if (tec >= 255)           return CAN_STATE_BUS_OFF;
    if (tec >= 128 || rec >= 128) return CAN_STATE_PASSIVE;
    return CAN_STATE_ACTIVE;
}

/* ============================================================
 * Bug hunt FIXED
 *
 * Bug 1: modbus_fc03_BUGGY uses LE byte order for register address.
 *        frame[2] = addr & 0xFF; frame[3] = addr >> 8;
 *        Modbus 使用大端序 作为寄存器地址和数据。
 *        修复：frame[2] = addr >> 8；帧[3] = 地址 & 0xFF;
 *
 * Bug 2：CRC存储为大端序。
 *        帧[6] = crc>>8；帧[7] = crc & 0xFF;
 *        Modbus CRC 是小端序（低字节在前）。
 *        修复：帧[6] = crc & 0xFF；帧[7] = crc>>8；
 *
 * Bug 3：返回 7 而不是 8。
 *        FC=03 请求：1(ID) + 1(FC) + 2(ADDR) + 2(数量) + 2(CRC) = 8 字节。
 *        修复：返回8；
 * ============================================================ */

int main(void)
{
    /* FC=03 request */
    uint8_t req[8];
    uint8_t n = modbus_build_fc03_request(0x01, 0x0064, 0x0002, req);
    assert(n == 8);
    assert(req[0] == 0x01 && req[1] == 0x03);
    assert(req[2] == 0x00 && req[3] == 0x64);  /* 是地址*/
    assert(req[4] == 0x00 && req[5] == 0x02);
    /* 验证CRC*/
    uint16_t crc = crc16_modbus(req, 6);
    assert(req[6] == (crc & 0xFF) && req[7] == (crc >> 8));

    /* FC=03 响应解析*/
    uint8_t resp[] = {0x01, 0x03, 0x04, 0x00, 0x0A, 0x00, 0x14, 0, 0};
    uint16_t resp_crc = crc16_modbus(resp, 7);
    resp[7] = (uint8_t)(resp_crc & 0xFF);
    resp[8] = (uint8_t)(resp_crc >> 8);
    uint16_t regs[4];
    int cnt = modbus_parse_fc03_response(resp, 9, regs, 4);
    assert(cnt == 2);
    assert(regs[0] == 10 && regs[1] == 20);

    /* CAN 信号提取*/
    uint8_t can_data[8] = {0xAB, 0xCD, 0, 0, 0, 0, 0, 0};
    /* 16 位 Intel 信号位于 start_bit=0，长度=16*/
    int32_t sig = can_signal_extract(can_data, 8, 0, 16, 0);
    assert(sig == 0xCDAB);  /* 英特尔 LE：0xAB + 0xCD<<8*/

    /* CAN 错误状态*/
    assert(can_get_error_state(100, 50)  == CAN_STATE_ACTIVE);
    assert(can_get_error_state(130, 50)  == CAN_STATE_PASSIVE);
    assert(can_get_error_state(255, 50)  == CAN_STATE_BUS_OFF);

    printf("All Modbus/CAN answers verified.\n");
    return 0;
}

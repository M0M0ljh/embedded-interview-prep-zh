/*
 * ============================================================
 * EMBEDDED INTERVIEW PREP
 * 主题 : Modbus RTU/TCP 和 CAN 总线
 * 文件：06_Communication_Protocols/03_modbus_can.c
 * ============================================================
 *
 * Modbus是工业嵌入式（PLC、SCADA）的标准。
 * CAN 对于汽车和工业物联网来说是强制性的。
 * ============================================================ */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

/* ============================================================
 * 理论 — Modbus RTU
 * ============================================================
 *
 * Modbus RTU 帧：
 *   [地址:1][FC:1][数据:N][CRC_LO:1][CRC_HI:1]
 *
 *   ADDR：从机地址（1-247，0=广播）
 *   FC：功能码
 *   数据：取决于 FC
 *   CRC : CRC-16/IBM (0xA001), 小端序（LO 首先）
 *
 * 常用功能代码：
 *   0x01 读取线圈
 *   0x02 读取离散输入
 *   0x03 读取保持寄存器 ← 最常见
 *   0x04 读取输入寄存器
 *   0x05 写入单线圈
 *   0x06 写单个寄存器
 *   0x10 写入多个寄存器
 *
 * FC=03 请求（共 8 个字节）：
 *   [地址][0x03][START_ADDR_HI][START_ADDR_LO][QUANTITY_HI][QUANTITY_LO][CRC_LO][CRC_HI]
 *
 * FC=03 Response:
 *   [地址][0x03][BYTE_COUNT][REG0_HI][REG0_LO]...[REGn_HI][REGn_LO][CRC_LO][CRC_HI]
 *   BYTE_COUNT = 数量 * 2
 *
 * 帧之间的静默间隔：>= 3.5 个字符时间
 *   9600 波特率时：3.5 * (1/9600) * 11 位 ≈ 4 ms
 * ============================================================ */

/* ============================================================
 * 理论 — CAN 巴士
 * ============================================================
 *
 * CAN（控制器局域网）属性：
 *   - 多主差分总线 (CAN_H / CAN_L)
 *   - 非破坏性按位仲裁（ID仲裁）
 *   - 内置错误检测：CRC-15、位填充、ACK
 *   - 最大 8 字节有效负载（经典CAN），64 字节（CAN FD）
 *
 * CAN帧结构（标准11位ID）：
 *   SOF[1] + ID[11] + RTR[1] + IDE[1] + r0[1] + DLC[4] + DATA[0-64位] +
 *   CRC[15] + CRC_DEL[1] + ACK[1] + ACK_DEL[1] + EOF[7]
 *
 * 仲裁：如果两个节点同时传输，则具有
 * 较低的 ID "wins"（显性位覆盖隐性位）。
 * 高优先级帧具有低 ID。
 *
 * 错误帧：
 *   主动错误：6 个显性位 + 8 个隐性位（总线功能）
 *   被动错误：6个隐性位（减少节点影响）
 *   总线关闭：TEC > 255 → 节点进入静默状态
 *
 * TEC/REC：发送/接收错误计数器
 *   成功TX：TEC -= 1
 *   失败TX：TEC += 8
 *   TEC > 127：被动错误
 *   TEC > 255：Bus-Off
 * ============================================================ */

/* ============================================================
 * MODBUS IMPLEMENTATION
 * ============================================================ */

/* 重用之前的 CRC（inline 对于独立文件）*/
static uint16_t modbus_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001u : crc >> 1;
    }
    return crc;
}

/* ============================================================
 * 任务 1 — 构建 FC=03 请求
 * ============================================================ */

int modbus_build_fc03_request(uint8_t slave_addr, uint16_t start_reg,
                               uint16_t quantity, uint8_t *out)
{
    /* TODO: 输出[0] = slave_addr
     * TODO: out[1] = 0x03
     * TODO: 输出[2] = start_reg >> 8
     * TODO: 输出[3] = start_reg & 0xFF
     * TODO: out[4] = 数量 >> 8
     * TODO: out[5] = 数量 & 0xFF
     * TODO：crc = modbus_crc16（输出，6）
     * TODO：out[6] = crc & 0xFF（低字节在前）
     * TODO: 输出[7] = CRC >> 8
     * TODO: 返回 8*/
    out[0] = slave_addr;
    out[1] = 0x03;
    out[2] = (uint8_t)(start_reg >> 8);
    out[3] = (uint8_t)(start_reg & 0xFFu);
    out[4] = (uint8_t)(quantity >> 8);
    out[5] = (uint8_t)(quantity & 0xFFu);
    uint16_t crc = modbus_crc16(out, 6);
    out[6] = (uint8_t)(crc & 0xFFu);
    out[7] = (uint8_t)(crc >> 8);
    return 8;
}

/* ============================================================
 * 任务 2 — 解析 FC=03 响应
 *
 * 返回已解析的寄存器数，如果出错则返回 -1。
 * ============================================================ */

int modbus_parse_fc03_response(const uint8_t *resp, uint16_t resp_len,
                                uint8_t expected_slave, uint16_t *regs, uint16_t max_regs)
{
    /* TODO：最小帧 = 5 字节（addr+fc+count+crc）*/
    if (resp_len < 5) return -1;

    /* TODO：验证 resp[0] == expected_slave*/
    if (resp[0] != expected_slave) return -1;

    /* TODO：验证 resp[1] == 0x03*/
    if (resp[1] != 0x03) return -1;

    uint8_t byte_count = resp[2];

    /* TODO：验证byte_count是偶数*/
    if (byte_count % 2 != 0) return -1;

    /* TODO：验证 resp_len >= 3 + byte_count + 2*/
    if (resp_len < (uint16_t)(5 + byte_count)) return -1;

    /* TODO：通过 resp[0..2+byte_count] 验证 CRC*/
    uint16_t crc_calc = modbus_crc16(resp, 3 + byte_count);
    uint16_t crc_recv = (uint16_t)(resp[3 + byte_count] | ((uint16_t)resp[4 + byte_count] << 8));
    if (crc_calc != crc_recv) return -1;

    /* TODO：提取寄存器（大端序对）*/
    uint16_t num_regs = byte_count / 2;
    if (num_regs > max_regs) num_regs = max_regs;
    for (uint16_t i = 0; i < num_regs; i++) {
        regs[i] = (uint16_t)((uint16_t)resp[3 + i*2] << 8) | resp[4 + i*2];
    }
    return (int)num_regs;
}

/* ============================================================
 * 任务 3 — 构建 FC=10（写入多个寄存器）请求
 * ============================================================ */

int modbus_build_fc10_request(uint8_t slave_addr, uint16_t start_reg,
                               const uint16_t *regs, uint16_t count, uint8_t *out)
{
    /* FC=10 PDU:
     * [地址][0x10][START_HI][START_LO][COUNT_HI][ COUNT_LO][BYTE_COUNT][数据...][CRC_LO][CRC_HI]
     * BYTE_COUNT = count * 2
     * 数据：每个寄存器大端序*/
    out[0] = slave_addr;
    out[1] = 0x10;
    out[2] = (uint8_t)(start_reg >> 8);
    out[3] = (uint8_t)(start_reg & 0xFFu);
    out[4] = (uint8_t)(count >> 8);
    out[5] = (uint8_t)(count & 0xFFu);
    out[6] = (uint8_t)(count * 2);
    for (uint16_t i = 0; i < count; i++) {
        out[7 + i*2]     = (uint8_t)(regs[i] >> 8);
        out[7 + i*2 + 1] = (uint8_t)(regs[i] & 0xFFu);
    }
    uint16_t pdu_len = 7 + count * 2;
    uint16_t crc = modbus_crc16(out, pdu_len);
    out[pdu_len]     = (uint8_t)(crc & 0xFFu);
    out[pdu_len + 1] = (uint8_t)(crc >> 8);
    return (int)(pdu_len + 2);
}

/* ============================================================
 * CAN BUS IMPLEMENTATION
 * ============================================================ */

typedef struct {
    uint32_t id;       /* 11 位（标准）或 29 位（扩展）*/
    uint8_t  dlc;      /* 数据长度代码（0-8）*/
    uint8_t  data[8];
    uint8_t  is_extended;   /* 0=标准 11 位，1=扩展 29 位*/
    uint8_t  is_rtr;        /* 1 = 远程传输请求（无数据）*/
} CanFrame;

/* ============================================================
 * 任务 4 — CAN 帧验证和信号提取
 * ============================================================ */

int can_frame_validate(const CanFrame *f)
{
    /* TODO：DLC 必须为 0-8
     * TODO：如果is_extended：ID 必须 <= 0x1FFFFFFF（29 位）
     *       否则：ID 必须 <= 0x7FF（11 位）
     * TODO：如果is_rtr：数据无关（RTR 帧不携带数据）
     * TODO: 返回 有效则为 1，无效则为 0*/
    if (f->dlc > 8) return 0;
    if (f->is_extended && f->id > 0x1FFFFFFFu) return 0;
    if (!f->is_extended && f->id > 0x7FFu) return 0;
    return 1;
}

/* 从 CAN 有效负载中提取信号（Intel 字节序、unsigned）*/
uint64_t can_signal_extract(const uint8_t *data, uint8_t start_bit, uint8_t length)
{
    /* Intel 字节序：start_bit = LSB 在 64 位字中的位置
     * 从 8 个字节（小端序）构建uint64_t，然后移位+掩码*/
    uint64_t raw = 0;
    for (int i = 0; i < 8; i++)
        raw |= ((uint64_t)data[i] << (i * 8));
    uint64_t mask = (length == 64) ? 0xFFFFFFFFFFFFFFFFULL : ((1ULL << length) - 1);
    return (raw >> start_bit) & mask;
}

/* ============================================================
 * 任务 5 — CAN 错误处理状态
 * ============================================================ */

typedef enum {
    CAN_ERROR_ACTIVE,
    CAN_ERROR_PASSIVE,
    CAN_BUS_OFF
} CanErrorState;

typedef struct {
    uint16_t tec;   /* 传输错误计数器*/
    uint16_t rec;   /* 接收错误计数器*/
} CanErrorCounters;

CanErrorState can_get_error_state(const CanErrorCounters *ec)
{
    /* TODO：如果 tec > 255 ||记录> 255：返回CAN_BUS_OFF
     * TODO：如果 tec > 127 ||记录 > 127: 返回 CAN_ERROR_PASSIVE
     * TODO：返回CAN_ERROR_ACTIVE*/
    if (ec->tec > 255 || ec->rec > 255) return CAN_BUS_OFF;
    if (ec->tec > 127 || ec->rec > 127) return CAN_ERROR_PASSIVE;
    return CAN_ERROR_ACTIVE;
}

/* ============================================================
 * 任务 6 — 找错题：Modbus 请求构建器
 *
 * 下面的函数构建一个 Modbus FC=03 请求。
 * 发现 3 个错误。
 * ============================================================ */

int modbus_fc03_BUGGY(uint8_t addr, uint16_t reg, uint16_t qty, uint8_t *out)
{
    out[0] = addr;
    out[1] = 0x03;

    /* Bug 1：字节序错误 - 应该是大端序（高字节在前）*/
    out[2] = (uint8_t)(reg & 0xFFu);    /* 应该是 reg >> 8*/
    out[3] = (uint8_t)(reg >> 8);       /* 应该是reg＆0xFF*/

    out[4] = (uint8_t)(qty >> 8);
    out[5] = (uint8_t)(qty & 0xFFu);

    uint16_t crc = modbus_crc16(out, 6);

    /* Bug 2：CRC 已存储 大端序 — Modbus RTU 规范需要 小端序（LO 首先）*/
    out[6] = (uint8_t)(crc >> 8);    /* 应该是 crc & 0xFF*/
    out[7] = (uint8_t)(crc & 0xFFu); /* 应该是 crc >> 8*/

    /* 错误 3：返回 7 而不是 8*/
    return 7;
}

/* ============================================================
 * SELF-TEST
 * ============================================================ */

int main(void)
{
    uint8_t req[8];
    int len = modbus_build_fc03_request(0x01, 0x0000, 0x000A, req);
    assert(len == 8);
    assert(req[0] == 0x01);
    assert(req[1] == 0x03);
    assert(req[2] == 0x00 && req[3] == 0x00);
    assert(req[4] == 0x00 && req[5] == 0x0A);

    /* 已知 CRC 此 Modbus 消息：0xC50E*/
    uint16_t crc_check = (uint16_t)(req[6] | ((uint16_t)req[7] << 8));
    assert(crc_check == 0xC50Eu);

    /* CAN 信号测试*/
    uint8_t can_data[] = {0xE8, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint64_t sig = can_signal_extract(can_data, 0, 16);
    /* 0x03E8 = 1000 */
    assert(sig == 0x03E8u);

    printf("All Modbus/CAN tests PASSED.\n");
    return 0;
}

/* ============================================================
 * INTERVIEW QUESTIONS
 * ============================================================
 *
 * Q1: Modbus RTU 和 Modbus TCP 有什么区别？
 *     答案：TODO
 *
 * Q2：Modbus为主/从。 CAN 是多主机。解释CAN仲裁。
 *     当两个节点同时传输相同的 ID 时会发生什么？
 *     答案：TODO
 *
 * Q3：CAN节点什么时候进入"bus-off"状态？
 *     恢复得怎么样了？
 *     答案：TODO
 *
 * Q4：你收到 Modbus FC=03 回复。 CRC 检查失败。
 *     按可能性的顺序列出可能的原因。
 *     答案：TODO
 *
 * Q5: 什么是CAN FD？它与古典CAN有何不同？
 *     答案：TODO
 */

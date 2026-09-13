# 个人项目面试答案

真实的answers取自你自己的项目经验。逐字使用这些内容或适应行为/技术深入问题。


---

## "Describe a communication protocol you designed."

**Q (EN):** "Describe the UART protocol you designed — the framing and CRC16." **Q (IT):** "Descrivi il protocollo UART che hai progettato — il framing e il CRC16."


### CN 答案

> “我们需要 PIC32 和 TI MCU 之间的结构化二进制UART 协议。
> 我设计了 ASCII-hex 框架：
>
> - **起始字节：** `0x3A`（`':'`，如 Intel HEX 格式）
> - **命令字节**、有效负载长度、编码为 ASCII-十六进制对的有效负载字节
>   (`0xAB`→`'A','B'`)
> - 有效负载的 **CRC16**（两个字节，也是 ASCII 十六进制），使用 Modbus 多项式 `0x8005`
> - **由 CRLF 终止**
>
> ASCII-hex 使流在终端中易于阅读 - 当你
> 在调试期间不使用逻辑分析仪嗅探UART线。
> CRC16 捕获物理链路上的位错误。
> 为了同步，UARTISR使用双缓冲乒乓方案：
> ISR 填充一个缓冲区，而应用程序任务处理另一个缓冲区，
> 避免任何共享状态竞态条件。”

### 信息技术解答

> “Avevo bisogno di un 协议 UART 结构 tra il PIC32 和 il MCU TI。
> Ho progettato un framing ASCII-hex：
>
> - **起始字节：** `0x3A`（`':'`，采用 Intel HEX 格式）
> - **字节comando**，lunghezza有效负载，字节有效负载来自复制ASCII-十六进制
>   (`0xAB`→`'A','B'`)
> - **CRC16** del 有效负载（应字节，ASCII-十六进制），con il polinomio Modbus `0x8005`
> - **CRLF 终止**
>
> L'ASCII-hex rende ilflussoleggibile in unterminale — 每次票价嗅探的实用程序
> sulla linea UART 在调试逻辑分析器期间。
> Il CRC16 会导致链接错误。
> 根据 la sincronizzazione，l'ISR UART 美国 lo 模式双缓冲乒乓球：
> l'ISR riempie un buffer mentre il task applicativo processa l'altro，
> evitando 竞态条件 sullo stato condiviso。”

### 可能的后续问题

| 跟进 | 击中关键点 |
|---|---|
| "Why ASCII-hex instead of raw binary?" | 无需逻辑分析仪即可调试可见性；在我们的波特率下，轻微的开销（2×字节）是可以接受的 |
| "Why `0x3A` as start byte?" | 借自 Intel HEX — 熟悉的惯例，随机噪声中罕见 |
| "Why CRC16 and not a simple checksum?" | CRC-16 检测所有 1 位、所有 2 位以及所有≤ 16 位的突发错误；校验和遗漏了许多多位模式 |
| "What polynomial — 0x8005 or 0xA001?" | 相同的多项式，不同的形式：`0x8005`是正规的（MSB-第一个）； `0xA001` 被反射（LSB-first），用于位循环实现 |
| "What happens if CRC fails?" | 接收方丢弃该帧并发回 NACK 命令字节；发送方最多重传 3 次 |
| "How did the ping-pong work exactly?" | 两个固定缓冲区 A 和 B。ISR 指针在帧完成标志上切换。应用程序任务读取当前未写入的缓冲区ISR。 volatile 标志表示缓冲区就绪 |

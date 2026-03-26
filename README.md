# STM32-Custom-Protocol

A lightweight serial communication protocol implemented on STM32F446RE (NUCLEO board) using bare metal register-level programming — no HAL dependency.

Designed to demonstrate packet framing, XOR checksum validation, and explicit error detection across three failure cases.

---

## Packet Structure

```
┌───────────┬────────────┬──────────────┬──────────────┐
│   0xAA    │   LENGTH   │     DATA     │   CHECKSUM   │
│  (1 byte) │  (1 byte)  │  (N bytes)   │   (1 byte)   │
└───────────┴────────────┴──────────────┴──────────────┘
```

| Field      | Size   | Description                            |
|------------|--------|----------------------------------------|
| Start Byte | 1 byte | Always `0xAA` — marks packet beginning |
| Length     | 1 byte | Number of data bytes (max 32)          |
| Data       | N bytes| Payload                                |
| Checksum   | 1 byte | XOR of Length + all Data bytes         |

### Checksum Calculation

```c
uint8_t chk = length;
for (int i = 0; i < length; i++)
    chk ^= data[i];
```

Example — sending `"HELLO"`:
```
0xAA  0x05  0x48 0x45 0x4C 0x4C 0x4F  0x47
Start  Len   H    E    L    L    O    Checksum
```

---

## Error Handling

| Error              | Condition                         | Response        |
|--------------------|-----------------------------------|-----------------|
| `ERR:START`        | First byte is not `0xAA`          | Packet rejected |
| `ERR:CHECKSUM`     | Received checksum != calculated   | NAK sent to host|
| `ERR:TIMEOUT`      | Byte not received within timeout  | Timeout returned|
| `ACK:OK`           | Packet valid                      | Echo back sent  |

---

## Hardware

| Component | Details              |
|-----------|----------------------|
| MCU       | STM32F446RE          |
| Board     | NUCLEO-F446RE        |
| UART      | USART2 (PA2=TX, PA3=RX) |
| Baud Rate | 115200               |
| Interface | USB via ST-Link      |

---

## Project Structure

```
STM32-Custom-Protocol/
├── Core/
│   ├── Inc/
│   │   └── main.h
│   └── Src/
│       └── main.c          ← Protocol implementation
├── host/
│   └── protocol.py         ← Python test script (PC side)
├── README.md
└── LICENSE
```

---

## Full Source Code

### `Core/Src/main.c`

```c
#include "stm32f4xx.h"
#include <string.h>

#define START_BYTE    0xAA
#define MAX_DATA_LEN  32
#define TIMEOUT_VAL   100000

// ─── Status Codes ─────────────────────────────────────────
#define PKT_OK            0
#define PKT_ERR_START     1
#define PKT_ERR_CHECKSUM  2
#define PKT_ERR_TIMEOUT   3

// ─── UART Init ───────────────────────────────────────────
void UART2_Init(void)
{
    // Clock enable — GPIOA and USART2
    RCC->AHB1ENR  |= (1 << 0);   // GPIOA clock ON
    RCC->APB1ENR  |= (1 << 17);  // USART2 clock ON

    // PA2 = TX, PA3 = RX → Alternate Function Mode
    GPIOA->MODER  &= ~((3 << 4) | (3 << 6));
    GPIOA->MODER  |=  ((2 << 4) | (2 << 6));

    // Alternate Function 7 = USART2
    GPIOA->AFR[0] &= ~((0xF << 8) | (0xF << 12));
    GPIOA->AFR[0] |=  ((7   << 8) | (7   << 12));

    // Baud Rate = 115200 @ 16MHz → BRR = 0x008B
    USART2->BRR = 0x008B;

    // TX + RX enable, USART ON
    USART2->CR1 = (1 << 3) |   // TE — Transmit Enable
                  (1 << 2) |   // RE — Receive Enable
                  (1 << 13);   // UE — USART Enable
}

// ─── Send Byte ───────────────────────────────────────────
void UART2_SendByte(uint8_t data)
{
    while (!(USART2->SR & (1 << 7)));  // Wait TXE
    USART2->DR = data;
}

// ─── Receive with Timeout ────────────────────────────────
uint8_t UART2_ReceiveByte_Timeout(uint8_t *data)
{
    uint32_t timeout = TIMEOUT_VAL;
    while (!(USART2->SR & (1 << 5)))
    {
        if (--timeout == 0) return PKT_ERR_TIMEOUT;
    }
    *data = (uint8_t)USART2->DR;
    return PKT_OK;
}

// ─── Send String ─────────────────────────────────────────
void UART2_SendString(const char *str)
{
    while (*str)
        UART2_SendByte((uint8_t)*str++);
}

// ─── Checksum ────────────────────────────────────────────
uint8_t Calculate_Checksum(uint8_t length, uint8_t *data)
{
    uint8_t chk = length;
    for (int i = 0; i < length; i++)
        chk ^= data[i];
    return chk;
}

// ─── Send Packet ─────────────────────────────────────────
void Send_Packet(uint8_t *data, uint8_t length)
{
    uint8_t chk = Calculate_Checksum(length, data);
    UART2_SendByte(START_BYTE);
    UART2_SendByte(length);
    for (int i = 0; i < length; i++)
        UART2_SendByte(data[i]);
    UART2_SendByte(chk);
}

// ─── Receive + Validate Packet ───────────────────────────
uint8_t Receive_Packet(uint8_t *buf, uint8_t *out_len)
{
    uint8_t byte;

    // 1. Start byte check
    if (UART2_ReceiveByte_Timeout(&byte) != PKT_OK)
    {
        UART2_SendString("ERR:TIMEOUT\n");
        return PKT_ERR_TIMEOUT;
    }
    if (byte != START_BYTE)
    {
        UART2_SendString("ERR:START\n");
        return PKT_ERR_START;
    }

    // 2. Read length
    uint8_t length;
    if (UART2_ReceiveByte_Timeout(&length) != PKT_OK)
    {
        UART2_SendString("ERR:TIMEOUT\n");
        return PKT_ERR_TIMEOUT;
    }

    // 3. Read data bytes
    for (int i = 0; i < length; i++)
    {
        if (UART2_ReceiveByte_Timeout(&buf[i]) != PKT_OK)
        {
            UART2_SendString("ERR:TIMEOUT\n");
            return PKT_ERR_TIMEOUT;
        }
    }

    // 4. Read and verify checksum
    uint8_t received_chk;
    if (UART2_ReceiveByte_Timeout(&received_chk) != PKT_OK)
    {
        UART2_SendString("ERR:TIMEOUT\n");
        return PKT_ERR_TIMEOUT;
    }

    uint8_t expected_chk = Calculate_Checksum(length, buf);
    if (received_chk != expected_chk)
    {
        UART2_SendString("ERR:CHECKSUM\n");
        return PKT_ERR_CHECKSUM;
    }

    // 5. Valid packet
    *out_len = length;
    UART2_SendString("ACK:OK\n");
    return PKT_OK;
}

// ─── Main ─────────────────────────────────────────────────
int main(void)
{
    UART2_Init();

    // Send startup packet
    uint8_t msg[] = "HELLO";
    Send_Packet(msg, strlen((char*)msg));

    uint8_t rxBuf[MAX_DATA_LEN];
    uint8_t rxLen = 0;

    while (1)
    {
        // Only process when data is available
        if (USART2->SR & (1 << 5))
        {
            uint8_t status = Receive_Packet(rxBuf, &rxLen);

            if (status == PKT_OK)
            {
                Send_Packet(rxBuf, rxLen);  // Echo back
            }
        }
    }
}
```

---

### `host/protocol.py`

```python
import serial
import time

# ─── Config ───────────────────────────────────────────────
PORT     = 'COM3'      # Change to your COM port
BAUDRATE = 115200
START    = 0xAA

# ─── Serial Open ──────────────────────────────────────────
ser = serial.Serial(PORT, BAUDRATE, timeout=2)
time.sleep(2)

# ─── Checksum ─────────────────────────────────────────────
def checksum(length, data):
    chk = length
    for b in data:
        chk ^= b
    return chk

# ─── Send Packet ──────────────────────────────────────────
def send_packet(message):
    data   = message.encode()
    length = len(data)
    chk    = checksum(length, data)
    packet = bytes([START, length]) + data + bytes([chk])
    ser.write(packet)
    print(f"Sent   : {message}  |  Bytes: {packet.hex(' ').upper()}")

# ─── Read Response ────────────────────────────────────────
def read_response():
    time.sleep(0.1)
    resp = ser.read_all().decode(errors='ignore').strip()
    print(f"STM32  : {resp}")
    return resp

# ─── Tests ────────────────────────────────────────────────
print("=== Custom Protocol Test ===\n")

# Test 1 — Valid packet
print("--- Test 1: Valid Packet ---")
send_packet("HELLO")
read_response()
time.sleep(1)

# Test 2 — Corrupt checksum
print("\n--- Test 2: Corrupt Checksum ---")
data   = "HELLO".encode()
packet = bytes([START, len(data)]) + data + bytes([0xFF])
ser.write(packet)
print(f"Sent   : (corrupt)  |  Bytes: {packet.hex(' ').upper()}")
read_response()
time.sleep(1)

# Test 3 — Wrong start byte
print("\n--- Test 3: Wrong Start Byte ---")
data   = "HELLO".encode()
packet = bytes([0x00, len(data)]) + data
ser.write(packet)
print(f"Sent   : (bad start)  |  Bytes: {packet.hex(' ').upper()}")
read_response()

ser.close()
print("\n=== Test Complete ===")
```

---

## Setup & Flash

### Requirements
- STM32CubeIDE
- NUCLEO-F446RE board
- Python 3.x + pyserial

### Flash
```
1. Open STM32CubeIDE
2. Import project
3. Build → Ctrl + B
4. Flash → Green Play Button ▶
```

### Run Python Script
```bash
pip install pyserial
python host/protocol.py
```

### Expected Output
```
=== Custom Protocol Test ===

--- Test 1: Valid Packet ---
Sent   : HELLO  |  Bytes: AA 05 48 45 4C 4C 4F 47
STM32  : ACK:OK

--- Test 2: Corrupt Checksum ---
Sent   : (corrupt)  |  Bytes: AA 05 48 45 4C 4C 4F FF
STM32  : ERR:CHECKSUM

--- Test 3: Wrong Start Byte ---
Sent   : (bad start)  |  Bytes: 00 05 48 45 4C 4C 4F
STM32  : ERR:START

=== Test Complete ===
```

---

## Why Bare Metal?

HAL hides what actually happens inside the peripheral. Writing directly to `USART2->BRR`, `USART2->CR1`, and `USART2->DR` means every register write has a reason — baud rate calculation, flag polling, data transmission — nothing is abstracted away.

```c
// HAL — works but opaque
HAL_UART_Transmit(&huart2, data, len, 100);

// Bare Metal — exactly what is happening
USART2->DR = data;
while (!(USART2->SR & USART_SR_TXE));
```

---

## License

MIT

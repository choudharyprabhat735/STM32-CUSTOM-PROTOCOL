/*
 * stageC.c
 *
 *  Created on: Mar 27, 2026
 *      Author: PRABHAT
 */
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
    RCC->AHB1ENR  |= (1 << 0);
    RCC->APB1ENR  |= (1 << 17);

    GPIOA->MODER  &= ~((3 << 4) | (3 << 6));
    GPIOA->MODER  |=  ((2 << 4) | (2 << 6));

    GPIOA->AFR[0] &= ~((0xF << 8) | (0xF << 12));
    GPIOA->AFR[0] |=  ((7   << 8) | (7   << 12));

    USART2->BRR = 0x008B;
    USART2->CR1 = (1 << 3) | (1 << 2) | (1 << 13);
}

// ─── Send Byte ───────────────────────────────────────────
void UART2_SendByte(uint8_t data)
{
    while (!(USART2->SR & (1 << 7)));
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

// ─── Packet Bhejo ────────────────────────────────────────
void Send_Packet(uint8_t *data, uint8_t length)
{
    uint8_t chk = Calculate_Checksum(length, data);
    UART2_SendByte(START_BYTE);
    UART2_SendByte(length);
    for (int i = 0; i < length; i++)
        UART2_SendByte(data[i]);
    UART2_SendByte(chk);
}

// ─── Packet Receive + Validate ───────────────────────────
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

    // 2. Length lo
    uint8_t length;
    if (UART2_ReceiveByte_Timeout(&length) != PKT_OK)
    {
        UART2_SendString("ERR:TIMEOUT\n");
        return PKT_ERR_TIMEOUT;
    }

    // 3. Data lo
    for (int i = 0; i < length; i++)
    {
        if (UART2_ReceiveByte_Timeout(&buf[i]) != PKT_OK)
        {
            UART2_SendString("ERR:TIMEOUT\n");
            return PKT_ERR_TIMEOUT;
        }
    }

    // 4. Checksum lo aur verify karo
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

    // 5. Sab sahi!
    *out_len = length;
    UART2_SendString("ACK:OK\n");
    return PKT_OK;
}

// ─── Main ─────────────────────────────────────────────────
int main(void)
{
    UART2_Init();

    // STM32 se ek valid packet bhejo
    uint8_t msg[] = "HELLO";
    Send_Packet(msg, strlen((char*)msg));

    // Ab PC se packet receive karne ka wait karo
    uint8_t rxBuf[MAX_DATA_LEN];
    uint8_t rxLen = 0;

    while (1)
    {
        uint8_t status = Receive_Packet(rxBuf, &rxLen);

        if (status == PKT_OK)
        {
            // Echo wapas bhejo
            Send_Packet(rxBuf, rxLen);
        }
    }
}


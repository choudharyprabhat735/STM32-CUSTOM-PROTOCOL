#include "stm32f4xx.h"
#include <string.h>

#define START_BYTE   0xAA
#define MAX_DATA_LEN 32

// ─── Packet Struct ────────────────────────────────────────
typedef struct {
    uint8_t start;
    uint8_t length;
    uint8_t data[MAX_DATA_LEN];
    uint8_t checksum;
} Packet;

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

// ─── Send / Receive ──────────────────────────────────────
void UART2_SendByte(uint8_t data)
{
    while (!(USART2->SR & (1 << 7)));
    USART2->DR = data;
}

uint8_t UART2_ReceiveByte(void)
{
    while (!(USART2->SR & (1 << 5)));
    return (uint8_t)USART2->DR;
}

// ─── Checksum Calculate ──────────────────────────────────
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

    UART2_SendByte(START_BYTE);   // 0xAA
    UART2_SendByte(length);       // Length
    for (int i = 0; i < length; i++)
        UART2_SendByte(data[i]);  // Data bytes
    UART2_SendByte(chk);          // Checksum
}

// ─── Main ─────────────────────────────────────────────────
int main(void)
{
    UART2_Init();

    // Test packet — "HELLO" bhejo
    uint8_t msg[] = "HELLO";
    Send_Packet(msg, strlen((char*)msg));

    while (1) { }
}

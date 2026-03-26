#include "stm32f4xx.h"

// ─── UART Init ───────────────────────────────────────────
void UART2_Init(void)
{
    // 1. Clock enable — GPIOA aur USART2
    RCC->AHB1ENR  |= (1 << 0);   // GPIOA clock ON
    RCC->APB1ENR  |= (1 << 17);  // USART2 clock ON

    // 2. PA2 = TX, PA3 = RX → Alternate Function Mode
    GPIOA->MODER  &= ~((3 << 4) | (3 << 6));   // clear
    GPIOA->MODER  |=  ((2 << 4) | (2 << 6));   // AF mode

    // 3. Alternate Function 7 = USART2
    GPIOA->AFR[0] &= ~((0xF << 8) | (0xF << 12));
    GPIOA->AFR[0] |=  ((7   << 8) | (7   << 12));

    // 4. Baud Rate = 115200 @ 16MHz
    // BRR = 16000000 / 115200 = 138.88 → 0x008B
    USART2->BRR = 0x008B;

    // 5. TX + RX enable, USART ON
    USART2->CR1 = (1 << 3) |   // TE — Transmit Enable
                  (1 << 2) |   // RE — Receive Enable
                  (1 << 13);   // UE — USART Enable
}

// ─── 1 Byte Bhejo ────────────────────────────────────────
void UART2_SendByte(uint8_t data)
{
    while (!(USART2->SR & (1 << 7)));  // TXE flag wait
    USART2->DR = data;
}

// ─── 1 Byte Lo ───────────────────────────────────────────
uint8_t UART2_ReceiveByte(void)
{
    while (!(USART2->SR & (1 << 5)));  // RXNE flag wait
    return (uint8_t)USART2->DR;
}

// ─── Main ─────────────────────────────────────────────────
int main(void)
{
    UART2_Init();

    // Test — "OK" bhejo PC ko
    UART2_SendByte('O');
    UART2_SendByte('K');
    UART2_SendByte('\n');

    while (1) { }
}

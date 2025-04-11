#include "Master_Modbus.h"
#include "stm32h7xx_hal.h"
#include <stdio.h>
#include "main.h"

/* Define the index and flags */
unsigned short ModbusMaster_Tx_index = 0;
unsigned short ModbusMaster_Rx_index = 0;
unsigned short ModbusMaster_TimeoutFlag = 0;
unsigned short ModbusMaster_FrameComplete_Flag = 0;
unsigned char ModbusMaster_outbox[MODBUS_MASTER_OUTBOX_LENGTH];
/* Define the Modbus Frame structure */
Modbus_MasterFrame ModbusFrame;
extern UART_HandleTypeDef huart2;
unsigned char ModbusMaster_inbox[MODBUS_MASTER_INBOX_LENGTH];

/* Initialize Modbus USART and GPIO */
void ModbusMaster_Init(void)
{
    /* Initialize GPIO for RS-485 */
	 GPIO_InitTypeDef GPIO_InitStruct = {0};

	    GPIO_InitStruct.Pin = GPIO_PIN_4;
	    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	    GPIO_InitStruct.Pull = GPIO_NOPULL;
	    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    /* Initialize UART1 for Modbus communication */
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        printf("UART2 initialization failed!\n");
    }
}

/* Calculate CRC for Modbus frame */
unsigned short ModbusMaster_CalculateCRC(unsigned char *buffer, unsigned short length)
{
    unsigned short crc = 0xFFFF;
    for (unsigned short pos = 0; pos < length; pos++)
    {
        crc ^= buffer[pos];
        for (int i = 0; i < 8; i++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

/* Send a Modbus request */
void ModbusMaster_SendRequest(unsigned char slave_id, unsigned char function_code, unsigned short start_address, unsigned short data_length, unsigned short *data)
{
    ModbusMaster_Tx_index = 0;

    /* Construct the Modbus frame */
    ModbusMaster_outbox[ModbusMaster_Tx_index++] = slave_id;
    ModbusMaster_outbox[ModbusMaster_Tx_index++] = function_code;
    ModbusMaster_outbox[ModbusMaster_Tx_index++] = (start_address >> 8) & 0xFF;
    ModbusMaster_outbox[ModbusMaster_Tx_index++] = start_address & 0xFF;
    ModbusMaster_outbox[ModbusMaster_Tx_index++] = (data_length >> 8) & 0xFF;
    ModbusMaster_outbox[ModbusMaster_Tx_index++] = data_length & 0xFF;

    /* Calculate CRC */
    unsigned short crc = ModbusMaster_CalculateCRC(ModbusMaster_outbox, ModbusMaster_Tx_index);
    ModbusMaster_outbox[ModbusMaster_Tx_index++] = crc & 0xFF;
    ModbusMaster_outbox[ModbusMaster_Tx_index++] = (crc >> 8) & 0xFF;

    // Debug
    printf("Modbus Request: ");
    for (int i = 0; i < ModbusMaster_Tx_index; i++)
    {
        printf("%02X ", ModbusMaster_outbox[i]);
    }
    printf("\n");

    /* Enable TX and send the frame */
    RS485_TX_ENABLE();
    HAL_UART_Transmit(&huart2, ModbusMaster_outbox, ModbusMaster_Tx_index, HAL_MAX_DELAY);
    RS485_RX_ENABLE();
}

unsigned short ModbusMaster_ReceiveResponse(unsigned char *buffer, unsigned short length)
{
    if (HAL_UART_Receive(&huart2, buffer, length, MODBUS_MASTER_TIMEOUT_MS) == HAL_OK)
    {
        // Debug: Print the received buffer
        printf("Received Buffer: ");
        for (int i = 0; i < length; i++) {
            printf("%02X ", buffer[i]);
        }
        printf("\n");

        // Validate CRC
        unsigned short crc_received = (buffer[length - 1] << 8) | buffer[length - 2];
        unsigned short crc_calculated = ModbusMaster_CalculateCRC(buffer, length - 2);

        // Debug: Print the CRC values
        printf("CRC Received: 0x%04X, CRC Calculated: 0x%04X\n", crc_received, crc_calculated);

        if (crc_received == crc_calculated)
        {
            ModbusMaster_FrameComplete_Flag = 1;
            return length;
        }
    }

    ModbusMaster_TimeoutFlag = 1;
    return 0;
}

/* Read Input Registers */
void ModbusMaster_ReadInputRegisters(unsigned char slave_id, unsigned short start_address, unsigned short data_length)
{
     ModbusMaster_SendRequest(slave_id, MODBUS_FC_READ_INPUT_REGISTERS, start_address, data_length, NULL);
}

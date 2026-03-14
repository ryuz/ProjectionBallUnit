/*
 *  	ext_ctrl.c
 *
 *  External coordinate input via UART0 (binary frame protocol)
 *
 *  Copyright (c) 2023
 *  K.Watanabe, Crescentt
 *  Released under the MIT license
 *  http://opensource.org/licenses/mit-license.php
 *
 */
#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "ProjectionBall.h"
#include "ext_ctrl.h"

typedef struct {
    int16_t cmd0;
    int16_t cmd1;
    bool    laser;
} ExtCtrlFrame_t;

/* Ping-pong buffer: ISR writes to one slot, Core1 reads from the other */
static volatile ExtCtrlFrame_t ext_buf[2];
static volatile uint8_t ext_buf_latest = 0;
static volatile bool    ext_has_data = false;

/* ISR-local receive state */
static uint8_t rx_frame[EXT_CTRL_FRAME_LEN];
static uint8_t rx_idx = 0;

void ExtCtrlInit(void)
{
    uart_set_baudrate(UART_ID, EXT_CTRL_UART_BAUD);
}

void ExtCtrlOnUartRx(void)
{
    while (uart_is_readable(UART_ID))
    {
        uint8_t byte = uart_getc(UART_ID);

        if (rx_idx == 0)
        {
            /* Wait for sync byte */
            if (byte == EXT_CTRL_SYNC_BYTE)
                rx_frame[rx_idx++] = byte;
        }
        else
        {
            rx_frame[rx_idx++] = byte;

            if (rx_idx >= EXT_CTRL_FRAME_LEN)
            {
                /* Verify XOR checksum (bytes 1..5) */
                uint8_t chk = rx_frame[1] ^ rx_frame[2] ^ rx_frame[3]
                            ^ rx_frame[4] ^ rx_frame[5];

                if (chk == rx_frame[6])
                {
                    /* Write to the inactive buffer slot */
                    uint8_t wr = 1 - ext_buf_latest;
                    ext_buf[wr].cmd0  = (int16_t)((rx_frame[1] << 8) | rx_frame[2]);
                    ext_buf[wr].cmd1  = (int16_t)((rx_frame[3] << 8) | rx_frame[4]);
                    ext_buf[wr].laser = (rx_frame[5] & 0x01) ? true : false;
                    /* Flip — reader now sees this slot */
                    ext_buf_latest = wr;
                    ext_has_data = true;
                }
                rx_idx = 0;
            }
        }
    }
}

bool ExtCtrlGetLatest(int16_t *cmd0, int16_t *cmd1, bool *laser)
{
    if (!ext_has_data)
        return false;

    uint8_t idx = ext_buf_latest;
    *cmd0  = ext_buf[idx].cmd0;
    *cmd1  = ext_buf[idx].cmd1;
    *laser = ext_buf[idx].laser;
    return true;
}

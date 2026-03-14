/*
 *  	ext_ctrl.h
 *
 *  External coordinate input via UART0 (binary frame protocol)
 *
 *  Protocol: 7-byte binary frame @ 1Mbaud
 *    [SYNC:0xA5] [CMD0_H] [CMD0_L] [CMD1_H] [CMD1_L] [FLAGS] [XOR_CHK]
 *    CMD0/CMD1: signed int16 offset from center (same as internal path coords)
 *    FLAGS bit0: laser on/off
 *
 *  Copyright (c) 2023
 *  K.Watanabe, Crescentt
 *  Released under the MIT license
 *  http://opensource.org/licenses/mit-license.php
 *
 */
#ifndef EXT_CTRL_H
#define EXT_CTRL_H

#include <stdint.h>
#include <stdbool.h>

#define EXT_CTRL_UART_BAUD  1000000
#define EXT_CTRL_SYNC_BYTE  0xA5
#define EXT_CTRL_FRAME_LEN  7

/* Call once after ioInit() to reconfigure UART for external control mode */
void ExtCtrlInit(void);

/* UART RX interrupt handler — call from OnUartRx() */
void ExtCtrlOnUartRx(void);

/* Get latest received coordinate (called from Core1 via GetPathCmd)
 * Returns true if at least one valid frame has been received */
bool ExtCtrlGetLatest(int16_t *cmd0, int16_t *cmd1, bool *laser);

#endif

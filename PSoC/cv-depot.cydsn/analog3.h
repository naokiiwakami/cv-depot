/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/

#pragma once

#include <stdint.h>

#include "project.h"

// ID assignments /////////////////////////////////
#define A3_ID_MIDI_TIMING_CLOCK 0x100
#define A3_ID_MIDI_VOICE_BASE   0x101
#define A3_ID_MIDI_REAL_TIME    0x140
#define A3_ID_MIDI_CHANNEL_BASE 0x141

// Message types //////////////////////////////////

/* MIDI Channel Voice message */
#define A3_VOICE_MSG_SET_NOTE          0x07
#define A3_VOICE_MSG_GATE_OFF          0x08
#define A3_VOICE_MSG_GATE_ON           0x09
#define A3_VOICE_MSG_POLY_KEY_PRESSURE 0x0A
#define A3_VOICE_MSG_CONTROL_CHANGE    0x0B
#define A3_VOICE_MSG_PROGRAM_CHANGE    0x0C
#define A3_VOICE_MSG_CHANNEL_PRESSURE  0x0D
#define A3_VOICE_MSG_PITCH_BEND        0x0E

#define A3_DATA_LENGTH 8

extern void A3SendDataStandard(uint32_t id, uint8_t dlc, CAN_DATA_BYTES_MSG *data);

/* [] END OF FILE */

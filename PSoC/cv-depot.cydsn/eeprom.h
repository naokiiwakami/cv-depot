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

#include "project.h"

#include "stdint.h"
    
#define ADDR_MIDI_CH 0x00
#define ADDR_BEND_OFFSET 0x01
#define ADDR_BEND_OCTAVE_WIDTH 0x04
#define ADDR_NOTE_1_WIPER 0x06
#define ADDR_NOTE_2_WIPER 0x07
#define ADDR_MIDI_CH_1 0x08
#define ADDR_MIDI_CH_2 0x09
#define ADDR_KEY_ASSIGNMENT_MODE 0x0a
#define ADDR_KEY_PRIORITY 0x0b
#define ADDR_MODULE_UID 0x1c

extern uint8_t ReadEepromWithValueCheck(uint16 address, uint8_t max);

extern cystatus Save16(uint16_t data, uint16_t address);
extern uint16_t Load16(uint16_t address);
    
extern cystatus Save32(uint32_t data, uint16_t address);
extern uint32_t Load32(uint16_t address);

/* [] END OF FILE */

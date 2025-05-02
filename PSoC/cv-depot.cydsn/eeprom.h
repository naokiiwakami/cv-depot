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
    
#define ADDR_MIDI_CH 0x0
#define ADDR_BEND_OFFSET 0x1
#define ADDR_BEND_OCTAVE_WIDTH 0x4
#define ADDR_NOTE_1_WIPER 0x6
#define ADDR_NOTE_2_WIPER 0x7
#define ADDR_MIDI_CH_1 0x8
#define ADDR_MIDI_CH_2 0x9
#define ADDR_KEY_ASSIGNMENT_MODE 0xa
#define ADDR_KEY_PRIORITY 0xb

extern uint8_t ReadEepromWithValueCheck(uint16 address, uint8_t max);
extern cystatus Save16(uint16 data, uint16_t address);
extern uint16_t Load16(uint16 address);
    
/* [] END OF FILE */

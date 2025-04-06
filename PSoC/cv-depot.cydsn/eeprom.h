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

#ifndef EEPROM_ADDR_H
#define EEPROM_ADDR_H

#include "project.h"

#include "stdint.h"
    
#define ADDR_MIDI_CH 0
#define ADDR_BEND_OFFSET 1
#define ADDR_BEND_OCTAVE_WIDTH 4

extern cystatus Save16(uint16_t data, uint16_t address);
extern uint16_t Load16(uint16_t address);
    
#endif  // EEPROM_ADDR_H

/* [] END OF FILE */

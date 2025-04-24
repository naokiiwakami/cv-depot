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

#include <stdint.h>

#include "key_assigner.h"

#pragma once
    
#define NUM_MIDI_CHANNELS 16

/**
 * Global MIDI configuration
 */
typedef struct midi_config {
    enum KeyAssignmentMode key_assignment_mode;
    uint8_t midi_channel_1;  // MIDI channel for note 1
    uint8_t midi_channel_2;  // MIDI channel for note 2
    enum KeyPriority key_priority;
} midi_config_t;

extern void InitMidiControllers();

/**
 * Handles a byte from the MIDI UART module.
 */
extern void ConsumeMidiByte(uint8_t rx_byte);

/* [] END OF FILE */

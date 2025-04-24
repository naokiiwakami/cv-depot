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

#if 0
/**
 * MIDI channel configuration and state
 */
typedef struct midi_channel {
    // configuration
    key_assigner_t *key_assigner;
    
    // state
    uint8_t message;
    
    uint8_t data[2];        // MIDI data buffer
    uint8_t data_position;  // MIDI data buffer pointer
    uint8_t data_length;    // Expected MIDI data length
} midi_channel_t;
#endif

extern void InitMidiControllers();
// extern void InitializeMidiDecoder();

/**
 * Handles a byte from the MIDI UART module.
 */
extern void ConsumeMidiByte(uint8_t rx_byte);

/* [] END OF FILE */

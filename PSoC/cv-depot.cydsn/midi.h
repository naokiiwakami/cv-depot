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

/**
 * Global MIDI configuration
 */
typedef struct midi_config {
    uint8_t num_channels;  // Number of MIDI channels to handle
    midi_channel_t *channels[NUM_MIDI_CHANNELS]; // MIDI channel mapping
} midi_config_t;

extern void InitializeMidiDecoder();

/**
 * Handles a byte from the MIDI UART module.
 */
extern void ConsumeMidiByte(uint8_t rx_byte);

/* [] END OF FILE */

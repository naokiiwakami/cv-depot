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
    uint8_t midi_channels[NUM_VOICES];  // MIDI channels for notes
    uint8_t selected_voice;  // Referred by the user interface
    enum KeyPriority key_priority;
} midi_config_t;

extern midi_config_t midi_config;

extern void InitMidiControllers();
extern void InitializeMidiDecoder();

/**
 * Changes MIDI channels.
 *
 * The method saves the new channels to EEPROM and resets the MIDI decoder.
 *
 * This method assumes that the new channels are in the midi_config already,
 * so does not take arguments.
 */
extern void CommitMidiChannelChange();

/**
 * Changes key assigment mode.
 *
 * The method saves the new assigment mode to EEPROM and resets the MIDI decoder.
 *
 * This method assumes that the new assignment mode is in the midi_config already,
 * so does not take argument for the new mode.
 */
extern void CommitKeyAssignmentModeChange();

/**
 * Handles a byte from the MIDI UART module.
 */
extern void ConsumeMidiByte(uint8_t rx_byte);

/* [] END OF FILE */

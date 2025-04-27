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
    uint8_t channels[NUM_VOICES];  // MIDI channels for notes
    enum KeyPriority key_priority;
} midi_config_t;

extern void InitializeMidiControllers();
extern void InitializeMidiDecoder();

/**
 * Accesses to the master MIDI config are done through these methods.
 */
extern const midi_config_t *GetMidiConfig();
extern void CommitMidiConfigChange(const midi_config_t *new_config);

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

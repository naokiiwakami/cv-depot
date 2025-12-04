/*
 * MIT License
 *
 * Copyright (c) 2025 Naoki Iwakami
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
    uint8_t expression_or_breath; // 0: expression, 1: breath
} midi_config_t;

// The master MIDI config
extern midi_config_t midi_config;

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

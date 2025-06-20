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

#include <string.h>

#include "project.h"

#include "eeprom.h"
#include "midi.h"
#include "key_assigner.h"

/*---------------------------------------------------------*/
/* The master MIDI config                                  */
/*---------------------------------------------------------*/
midi_config_t midi_config;

/*---------------------------------------------------------*/
/* MIDI constants                                          */
/*---------------------------------------------------------*/

/* MIDI Channel Voice message */
#define MSG_NOTE_OFF          0x80
#define MSG_NOTE_ON           0x90
#define MSG_POLY_KEY_PRESSURE 0xA0
#define MSG_CONTROL_CHANGE    0xB0
#define MSG_PROGRAM_CHANGE    0xC0
#define MSG_CHANNEL_PRESSURE  0xD0
#define MSG_PITCH_BEND        0xE0

/* MIDI Control Changes */
#define CC_BREATH                0x02
#define CC_EXPRESSION            0x0B
#define CC_DAMPER_PEDAL          0x40

/* MIDI Channel Mode Messages */
#define CC_ALL_SOUND_OFF         0x78
#define CC_RESET_ALL_CONTROLLERS 0x79
#define CC_LOCAL_CONTROL         0x7A
#define CC_ALL_NOTES_OFF         0x7B
#define CC_OMNI_MODE_OFF         0x7C
#define CC_OMNI_MODE_ON          0x7D
#define CC_MONO_MODE_ON          0x7E
#define CC_POLY_MODE_ON          0x7F

/* MIDI System Common Messages */
#define SYSEX_IN  0xF0
#define SYSEX_OUT 0xF7

/* MIDI System Real-Time Messages */
#define TIMINIG_CLOCK  0xf8  // The lowest number in the real-time messages

#define MAX_DATA_VALUE 0x7F

/* Notes */
#define C0 0x0B
#define C4 0x3C
#define A4 0x45

/*---------------------------------------------------------*/
/* Macros                                                  */
/*---------------------------------------------------------*/
#define IsInSystemExclusiveMode(status)    ((status) == SYSEX_IN)
#define IsChannelStatus(status)            ((status) < SYSEX_IN)
#define RetrieveMessageFromStatus(status)  ((status) & 0xF0)
#define RetrieveChannelFromStatus(status)  ((status) & 0x0F)

/*---------------------------------------------------------*/
/* Decoder states                                          */
/*---------------------------------------------------------*/
static uint8_t midi_status;
static uint8_t midi_channel;
static uint8_t midi_message;

// data buffer
static uint8_t midi_data[2];        // MIDI data buffer
static uint8_t midi_data_position;  // MIDI data buffer pointer
static uint8_t midi_data_length;    // Expected MIDI data length

static key_assigner_t key_assigner_instances[2];
static key_assigner_t *key_assigners[NUM_MIDI_CHANNELS];

static void HandleMidiChannelMessage();

void InitializeMidiControllers()
{
    memset(&midi_config, 0, sizeof(midi_config));  // is memset safe to use?

    // Set Basic MIDI channels
    midi_config.channels[0] = ReadEepromWithValueCheck(ADDR_MIDI_CH_1, 16);
    midi_config.channels[1] = ReadEepromWithValueCheck(ADDR_MIDI_CH_2, 16);
    midi_config.key_assignment_mode = ReadEepromWithValueCheck(ADDR_KEY_ASSIGNMENT_MODE, KEY_ASSIGN_END);
    midi_config.key_priority =
        ReadEepromWithValueCheck(ADDR_KEY_PRIORITY, KEY_PRIORITY_END);

    // set A4 to all voices and turn off gates
    for (int i = 0; i < NUM_VOICES; ++i) {
        all_voices[i].gate_off();
        all_voices[i].set_note(A4);
    }

    InitializeMidiDecoder();
}

void InitializeMidiDecoder()
{
    midi_status = 0;
    midi_channel = 0;
    midi_message = 0;
    midi_data_position = 0;
    midi_data_length = 0;

    memset(key_assigners, 0, sizeof(key_assigners));

    // TODO: Generalize the implementation for N number of voices
    key_assigners[midi_config.channels[0]] =
        InitializeKeyAssigner(&key_assigner_instances[0], midi_config.key_priority);
    AddVoice(&key_assigner_instances[0], &all_voices[0], midi_config.key_assignment_mode);

    key_assigner_t *note_2_assigner = key_assigners[midi_config.channels[1]];
    if (note_2_assigner == NULL) {
        note_2_assigner = key_assigners[midi_config.channels[1]] =
            InitializeKeyAssigner(&key_assigner_instances[1], midi_config.key_priority);
    }
    AddVoice(note_2_assigner, &all_voices[1], midi_config.key_assignment_mode);
}

const midi_config_t *GetMidiConfig()
{
    return &midi_config;
}

void CommitMidiConfigChange(const midi_config_t *new_config)
{
    // Save changes
    EEPROM_UpdateTemperature();
    for (int voice = 0; voice < NUM_VOICES; ++voice) {
        if (new_config->channels[voice] != midi_config.channels[voice]) {
            EEPROM_WriteByte(new_config->channels[voice], ADDR_MIDI_CH_1 + voice);
        }
    }
    if (new_config->key_assignment_mode != midi_config.key_assignment_mode) {
        EEPROM_WriteByte(new_config->key_assignment_mode, ADDR_KEY_ASSIGNMENT_MODE);
    }
    if (new_config->key_priority != midi_config.key_priority) {
        EEPROM_WriteByte(new_config->key_priority, ADDR_KEY_PRIORITY);
    }

    // Reflect changes
    midi_config = *new_config;
    InitializeMidiDecoder();
}

void ConsumeMidiByte(uint8_t rx_byte)
{
    if (rx_byte >= TIMINIG_CLOCK) {
      // Ignore system real-time messages (yet)
      return;
    }

    // Ignore system messages
    if (IsInSystemExclusiveMode(midi_status)) {
        if (rx_byte == SYSEX_OUT) {
            midi_status = SYSEX_OUT;
        }
        return;
    }

    if (rx_byte >= SYSEX_IN) {
        midi_status = rx_byte;
        return;
    }

    if (rx_byte > MAX_DATA_VALUE) {
        // This is a status byte of a channel message
        midi_status = rx_byte;
        midi_channel = RetrieveChannelFromStatus(midi_status);
        midi_message = RetrieveMessageFromStatus(midi_status);

        // set the data length according to the message type
        switch(midi_message) {
        case MSG_NOTE_OFF:
        case MSG_NOTE_ON:
        case MSG_POLY_KEY_PRESSURE:
        case MSG_CONTROL_CHANGE:
        case MSG_PITCH_BEND:
            midi_data_length = 2;
            break;
        case MSG_PROGRAM_CHANGE:
        case MSG_CHANNEL_PRESSURE:
            midi_data_length = 1;
            break;
        }

        // Reset the data index
        midi_data_position = 0;
        return;
    }

    // The byte is data if we reach here
    midi_data[midi_data_position++] = rx_byte;
    if (midi_data_position ==  midi_data_length) {
        midi_data_position = 0;

        // We handle the data
        if (IsChannelStatus(midi_status)) {
            HandleMidiChannelMessage();
        }
    }
}


void HandleMidiChannelMessage()
{
    // do nothing for a channel that is out of scope
    key_assigner_t *key_assigner = key_assigners[midi_channel];
    if (key_assigner == NULL) {
        return;
    }

    switch(midi_message) {
    case MSG_NOTE_OFF:
        NoteOff(key_assigner, midi_data[0]);
        break;
    case MSG_NOTE_ON:
        NoteOn(key_assigner, midi_data[0], midi_data[1]);
        break;
#if 0
    case MSG_CONTROL_CHANGE:
        ControlChange(midi_data[0], midi_data[1]);
        break;
    case MSG_PITCH_BEND:
        BendPitch(midi_data[0], midi_data[1]);
        break;
#endif
    default:
        // do nothing for unsupported channel messages
        break;
    }
}

/* [] END OF FILE */

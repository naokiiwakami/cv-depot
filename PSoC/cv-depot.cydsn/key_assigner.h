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

#pragma once

#include <stdint.h>

// Maximum number of notes to track history in a voice
#define MAX_NOTES 32
#define MAX_TRACK_HISTORY 8
#define ALL_NOTES 128
#define NUM_VOICES 2

enum KeyAssignmentMode {
    KEY_ASSIGN_DUOPHONIC = 0,
    KEY_ASSIGN_UNISON,
    KEY_ASSIGN_PARALLEL,
    KEY_ASSIGN_END,
};

enum KeyPriority {
    KEY_PRIORITY_LATER = 0,
    KEY_PRIORITY_HIGH,
    KEY_PRIORITY_LOW,
    KEY_PRIORITY_END,
};

typedef struct voice {
    uint8_t id;
    uint8_t notes[MAX_NOTES];
    int num_notes;
    uint8_t velocity;
    uint8_t gate;
    uint8_t in_use[ALL_NOTES];
    uint32_t gate_on_time;
    uint32_t gate_off_time;
    void (*set_note)(uint8_t note_number);
    void (*gate_on)(uint8_t velocity);
    void (*gate_off)();
    struct voice *next_voice;
} voice_t;

extern voice_t all_voices[NUM_VOICES];

typedef struct key_assigner {
    voice_t *voices[NUM_VOICES];  // pre-allocate memory for the longest array
    int num_voices;
    int index_next_voice;
    enum KeyPriority key_priority;
} key_assigner_t;

/**
 * Initializes all voices.
 */
extern void KeyAssigner_ConnectVoices();

/**
 * Clears a key assigner.
 */
extern key_assigner_t *InitializeKeyAssigner(key_assigner_t *, enum KeyPriority);

/**
 * Adds a voice to a key assigner.
 */
// TODO: Consider moving the param key_assignment_mode to
//   InitializeKeyAssigner as the mode must be consistent in an assigner.
extern void AddVoice(key_assigner_t *, voice_t *, enum KeyAssignmentMode);

// Requests for performance actions
extern void NoteOn(key_assigner_t *key_assigner, uint8_t note_number, uint8_t velocity);
extern void NoteOff(key_assigner_t *key_assigner, uint8_t note_number);

/* [] END OF FILE */

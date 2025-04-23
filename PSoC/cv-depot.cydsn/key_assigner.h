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

#pragma once

// Maximum number of notes to track history in a voice
#define MAX_NOTES 32
#define MAX_TRACK_HISTORY 8
#define ALL_NOTES 128

typedef struct voice {
    uint8_t notes[MAX_NOTES];
    int num_notes;
    uint8_t velocity;
    uint8_t gate;
    uint8_t in_use[ALL_NOTES];
    void (*set_note)(uint8_t note_number);
    void (*gate_on)(uint8_t velocity);
    void (*gate_off)();
} voice_t;

typedef struct key_assigner {
    voice_t **voices;
    int num_voices;
    int index_next_voice;
} key_assigner_t;

extern void SetUpDuophonic();
extern void SetUpUnison();
extern void NoteOn(key_assigner_t *key_assigner, uint8_t note_number, uint8_t velocity);
extern void NoteOff(key_assigner_t *key_assigner, uint8_t note_number);

/* [] END OF FILE */

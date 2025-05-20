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

#include <stddef.h>
#include <stdint.h>

#include "project.h"

#include "analog3.h"
#include "key_assigner.h"
#include "voice.h"

voice_t all_voices[NUM_VOICES];

void VoiceNoteOn(voice_t *voice, uint8_t note_number, uint8_t velocity)
{
    if (voice->num_notes == MAX_NOTES) {
        voice->in_use[voice->notes[MAX_NOTES-1]] = 0;
    } else {
        ++voice->num_notes;
    }    
    for (int i = voice->num_notes; --i > 0;) {
        voice->notes[i] = voice->notes[i - 1];
    }
    voice->notes[0] = note_number;
    
    // Update the hardware
    CAN_DATA_BYTES_MSG data;
    for (voice_t *current = voice; current != NULL; current = current->next_voice) {
        current->set_note(note_number);
        current->gate_on(velocity);
        data.byte[0] = A3_VOICE_MSG_SET_NOTE;
        data.byte[1] = note_number;
        data.byte[2] = A3_VOICE_MSG_GATE_ON;
        data.byte[3] = velocity;
        A3SendDataStandard(A3_ID_MIDI_VOICE_BASE + current->id, 4, &data);
    }
    // LED_Driver_PutChar7Seg('N', 0);
    // LED_Driver_Write7SegNumberHex(note_number, 1, 2, LED_Driver_RIGHT_ALIGN);

    voice->gate = 1;
    voice->in_use[note_number] = 1;
    voice->velocity = velocity;
}

void VoiceReactivateNote(voice_t *voice, uint8_t note_number, uint8_t velocity)
{
    // no idea for now, do nothing
}

void VoiceNoteOff(voice_t *voice, uint8_t note_number)
{
    if (!voice->in_use[note_number]) {
        return;
    }
    int to_drop;
    for (to_drop = 0; to_drop < voice->num_notes; ++to_drop) {
        if (voice->notes[to_drop] == note_number) {
            break;
        }
    }
    for (int i = to_drop; i < voice->num_notes - 1; ++i) {
        voice->notes[i] = voice->notes[i + 1];
    }
    
    voice->in_use[note_number] = 0;
    --voice->num_notes;
    
    if (to_drop > 0) {
        // hidden note, do nothing
        return;
    }

    // update the hardware
    CAN_DATA_BYTES_MSG data;
    if (voice->num_notes == 0) {
        voice->gate = 0;
        for (voice_t *current = voice; current != NULL; current = current->next_voice) {
            current->gate_off();
            data.byte[0] = A3_VOICE_MSG_GATE_OFF;
            A3SendDataStandard(A3_ID_MIDI_VOICE_BASE + current->id, 1, &data);
        }
    } else {
        for (voice_t *current = voice; current != NULL; current = current->next_voice) {
            current->set_note(voice->notes[0]);
            data.byte[0] = A3_VOICE_MSG_SET_NOTE;
            data.byte[1] = voice->notes[0];
            A3SendDataStandard(A3_ID_MIDI_VOICE_BASE + current->id, 2, &data);
        }
        // retrigger?
    }
}

static void InitializeVoice(voice_t *voice, uint8_t id,
    void (*set_note)(uint8_t), void (*gate_on)(uint8_t), void (*gate_off)())
{
    voice->id = id;
    voice->num_notes = 0;
    voice->velocity = 0;
    voice->gate = 0;
    for (int i = 0; i < ALL_NOTES; ++i) {
        voice->in_use[i] = 0;
    }
    voice->set_note = set_note;
    voice->gate_on = gate_on;
    voice->gate_off = gate_off;
    voice->next_voice = NULL;
}

void KeyAssigner_ConnectVoices()
{
    voice_config_t configs[NUM_VOICES];
    GetVoiceConfigs(configs, NUM_VOICES);
    for (uint8_t i = 0; i < NUM_VOICES; ++i) {
        InitializeVoice(&all_voices[i], i,
            configs[i].set_note, configs[i].gate_on, configs[i].gate_off);
    }
}

key_assigner_t *InitializeKeyAssigner(key_assigner_t *key_assigner, enum KeyPriority key_priority)
{
    key_assigner->num_voices = 0;
    key_assigner->index_next_voice = 0;
    key_assigner->key_priority = key_priority;
    return key_assigner;
}

void AddVoice(key_assigner_t *assigner, voice_t *voice, enum KeyAssignmentMode key_assignment_mode)
{
    if (key_assignment_mode == KEY_ASSIGN_UNISON) {
        if (assigner->num_voices == 0) {
            assigner->voices[assigner->num_voices++] = voice;
        } else {
            assigner->voices[0]->next_voice = voice;
        }
    } else { // duophonic or parallel
        assigner->voices[assigner->num_voices++] = voice;
    }
    voice->next_voice = NULL;
}

void NoteOn(key_assigner_t *assigner, uint8_t note_number, uint8_t velocity)
{
    // note on with zero velocity means note off
    if (velocity == 0) {
        NoteOff(assigner, note_number);
        return;
    }
    
    for (int i = 0; i < assigner->num_voices; ++i) {
        if (assigner->voices[i]->in_use[note_number]) {
            VoiceReactivateNote(assigner->voices[i], note_number, velocity);
            return;
        }
    }

    // Find an available voice
    for (int i = 0; i < assigner->num_voices; ++i) {
        int index = (assigner->index_next_voice + i) % assigner->num_voices;
        voice_t *voice = assigner->voices[index];
        if (!voice->gate) {
            VoiceNoteOn(voice, note_number, velocity);
            assigner->index_next_voice = (index + 1) % assigner->num_voices;
            return;
        }
    }

    // All voices are occupied, replace the oldest voice
    voice_t *next_voice = assigner->voices[assigner->index_next_voice];
    assigner->index_next_voice = (assigner->index_next_voice + 1) % assigner->num_voices;
    VoiceNoteOn(next_voice, note_number, velocity);
}

void NoteOff(key_assigner_t *assigner, uint8_t note_number)
{
    for (int i = 0; i < assigner->num_voices; ++i) {
        VoiceNoteOff(assigner->voices[i], note_number);
    }
}

/* [] END OF FILE */

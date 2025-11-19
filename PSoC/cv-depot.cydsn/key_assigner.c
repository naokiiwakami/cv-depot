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

#include <stddef.h>
#include <stdint.h>

#include "project.h"

#include "analog3.h"
#include "key_assigner.h"
#include "main.h"
#include "voice.h"

// We borrow PWM_Bend to count delay time, which has 21.354 usec cycle.
// Gate should delay by about 10ms to avoid making sound during CV transition.
#define GATE_DELAY 450

voice_t all_voices[NUM_VOICES];

static void SendGateOn(void *arg)
{
    voice_t *voice =(voice_t *)arg;
    uint32_t target_time;
    if (voice->gate_on_time > TIMER_COUNTER_WRAP) {
        CyGlobalIntDisable;
        target_time = (timer_counter & (TIMER_COUNTER_WRAP - (TIMER_COUNTER_WRAP >> 1))) ? timer_counter : timer_counter + TIMER_COUNTER_WRAP + 1;
        CyGlobalIntEnable;
    } else {
        target_time = timer_counter;
    }
    if (voice->gate_on_time > target_time) {
        task_t task = { .run = SendGateOn, .arg = voice};
        ScheduleTask(task);
        return;
    }
    voice->gate_on(voice->velocity);
    // TODO: Split set note and get on
    CAN_DATA_BYTES_MSG data;
    data.byte[0] = A3_VOICE_MSG_SET_NOTE;
    data.byte[1] = voice->notes[0];
    data.byte[2] = A3_VOICE_MSG_GATE_ON;
    data.byte[3] = voice->velocity << 1;
    data.byte[4] = 0;
    A3SendDataStandard(A3_ID_MIDI_VOICE_BASE + voice->id, 5, &data);
}

static void SendGateOff(void *arg)
{
    voice_t *voice =(voice_t *)arg;
    uint32_t target_time;
    if (voice->gate_off_time > TIMER_COUNTER_WRAP) {
        CyGlobalIntDisable;
        target_time = (timer_counter & (TIMER_COUNTER_WRAP - (TIMER_COUNTER_WRAP >> 1))) ? timer_counter : timer_counter + TIMER_COUNTER_WRAP + 1;
        CyGlobalIntEnable;
    } else {
        target_time = timer_counter;
    }
    if (voice->gate_off_time > target_time) {
        task_t task = { .run = SendGateOff, .arg = voice};
        ScheduleTask(task);
        return;
    }
    voice->gate_off();
}

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
    voice->velocity = velocity;

    // Update the hardware
    for (voice_t *current = voice; current != NULL; current = current->next_voice) {
        current->set_note(note_number);
        // CAN set note message should be forwarded here, but splitting the message
        // confuses the envelope generator for some reason. We send a bundled message
        // later to detour the issue.
        current->gate_on_time = timer_counter + GATE_DELAY;
        task_t task = { .run = SendGateOn, .arg = current};
        ScheduleTask(task);
    }
    // LED_Driver_PutChar7Seg('N', 0);
    // LED_Driver_Write7SegNumberHex(note_number, 1, 2, LED_Driver_RIGHT_ALIGN);

    voice->gate = 1;
    voice->in_use[note_number] = 1;
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
            current->gate_off_time = timer_counter + GATE_DELAY;
            task_t task = { .run = SendGateOff, .arg = current};
            ScheduleTask(task);
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

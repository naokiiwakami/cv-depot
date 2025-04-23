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

#include "key_assigner.h"

voice_t voices[2];
voice_t *duo_voices[2] = { &voices[0], &voices[1] };

#define VELOCITY_DAC_VALUE(velocity) (((velocity) * (velocity)) >> 3)
#define INDICATOR_VALUE(velocity) ((velocity) >= 64 ? (((velocity) - 63)  * ((velocity) - 63)) / 32 - 1 : 0)
#define NOTE_PWM_MAX_VALUE 120

#define SetNote1 PWM_Notes_WriteCompare1
#define SetNote2 PWM_Notes_WriteCompare2

static void Gate1On(uint8_t velocity)
{
    DVDAC_Velocity_1_SetValue(VELOCITY_DAC_VALUE(velocity));
    PWM_Indicators_WriteCompare1(INDICATOR_VALUE(velocity));
    Pin_Gate_1_Write(1);
}

static void Gate1Off()
{
    Pin_Portament_En_Write(0);
    DVDAC_Velocity_1_SetValue(0);
    Pin_Gate_1_Write(0);
}

static void Gate2On(uint8_t velocity)
{
    DVDAC_Velocity_2_SetValue(VELOCITY_DAC_VALUE(velocity));
    PWM_Indicators_WriteCompare2(INDICATOR_VALUE(velocity));
    Pin_Gate_2_Write(1);
}

static void Gate2Off()
{
    DVDAC_Velocity_2_SetValue(0);
    Pin_Gate_2_Write(0);
}

static void SetBothNotes(uint8_t note_number)
{
    SetNote1(note_number);
    SetNote2(note_number);
}

static void BothGateOn(uint8_t velocity)
{
    Gate1On(velocity);
    Gate2On(velocity);
}

static void BothGateOff()
{
    Gate1Off();
    Gate2Off();
}

void InitializeVoice(voice_t *voice, void (*set_note)(uint8_t), void (*gate_on)(uint8_t), void (*gate_off)())
{
    voice->num_notes = 0;
    voice->velocity = 0;
    voice->gate = 0;
    for (int i = 0; i < ALL_NOTES; ++i) {
        voice->in_use[i] = 0;
    }
    voice->set_note = set_note;
    voice->gate_on = gate_on;
    voice->gate_off = gate_off;
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
    
    // Update the hardware
    voice->set_note(note_number);
    voice->gate_on(velocity);
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

    if (voice->num_notes == 0) {
        voice->gate = 0;
        voice->gate_off();
        // LED_Driver_PutChar7Seg('F', 0);
        // LED_Driver_Write7SegNumberHex(note_number, 1, 2, LED_Driver_RIGHT_ALIGN);
    } else {
        voice->set_note(voice->notes[0]);
        // retrigger?
    }
}

void InitializeKeyAssigner(key_assigner_t *key_assigner, voice_t *voices[], int num_voices)
{
    key_assigner->voices = voices;
    key_assigner->num_voices = num_voices;
    key_assigner->index_next_voice = 0;
}

void SetUpDuophonic(key_assigner_t *assigner)
{
    InitializeVoice(duo_voices[0], SetNote1, Gate1On, Gate1Off);
    InitializeVoice(duo_voices[1], SetNote2, Gate2On, Gate2Off);
    InitializeKeyAssigner(assigner, duo_voices, 2);
}

void SetUpUnison(key_assigner_t *assigner)
{
    InitializeVoice(&voices[0], SetBothNotes, BothGateOn, BothGateOff);
    InitializeKeyAssigner(assigner, duo_voices, 1);
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

    for (int i = 0; i < assigner->num_voices; ++i) {
        int index = (assigner->index_next_voice + i) % assigner->num_voices;
        voice_t *voice = assigner->voices[index];
        if (!voice->gate) {
            VoiceNoteOn(voice, note_number, velocity);
            assigner->index_next_voice = (index + 1) % assigner->num_voices;
            return;
        }
    }
    voice_t *next_voice = assigner->voices[assigner->index_next_voice];
    assigner->index_next_voice = (assigner->index_next_voice + 1) % assigner->num_voices;
    VoiceNoteOn(next_voice, note_number, velocity);
}

void NoteOff(key_assigner_t *assigner, uint8_t note_number)
{
    // voice_t *next_voice = NULL;
    for (int i = 0; i < assigner->num_voices; ++i) {
        VoiceNoteOff(assigner->voices[i], note_number);
    }
}

/* [] END OF FILE */

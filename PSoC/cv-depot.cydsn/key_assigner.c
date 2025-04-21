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


static void Gate1On(uint8_t velocity)
{
    if (Pin_Gate_1_Read()) {
        Pin_Portament_En_Write(1);
    } else {
        DVDAC_Velocity_1_SetValue(VELOCITY_DAC_VALUE(velocity));
        PWM_Indicators_WriteCompare1(INDICATOR_VALUE(velocity));
        Pin_Gate_1_Write(1);
    }
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

void InitializeVoice(voice_t *voice, void (*gate_on)(uint8_t), void (*gate_off)())
{
    voice->num_notes = 0;
    voice->velocity = 0;
    voice->gate = 0;
    for (int i = 0; i < ALL_NOTES; ++i) {
        voice->in_use[i] = 0;
    }
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
    for (int i = 1; i < voice->num_notes; ++i) {
        voice->notes[i] = voice->notes[i - 1];
    }
    voice->notes[0] = note_number;
    voice->gate = 1;
    voice->in_use[note_number] = 1;
    voice->velocity = velocity;
    
    // Update the hardware
    voice->gate_on(velocity);
    LED_Driver_PutChar7Seg('N', 0);
    LED_Driver_Write7SegNumberHex(note_number, 1, 2, LED_Driver_RIGHT_ALIGN);
}

void VoiceReactivateNote(voice_t *voice, uint8_t note_number, uint8_t velocity)
{
    for (int i = 0; i < voice->num_notes; ++i) {
        if (voice->notes[i] == note_number) {
            for (int j = i; j > 0; --j) {
                voice->notes[i] = voice->notes[i-1];
            }
            voice->notes[0] = note_number;
            voice->velocity = velocity;    
            return;            
        }
    }
}

void VoiceNoteOff(voice_t *voice, uint8_t note_number)
{
    if (!voice->in_use[note_number]) {
        return;
    }
    int i;
    for (i = 0; i < voice->num_notes; ++i) {
        if (voice->notes[i] == note_number) {
            break;
        }
    }
    for (; i < voice->num_notes - 1; ++i) {
        voice->notes[i] = voice->notes[i + 1];
    }
    --voice->num_notes;
    voice->gate = 0;
    voice->in_use[note_number] = 0;

    // Update the hardware
    voice->gate_off();
    LED_Driver_PutChar7Seg('F', 0);
    LED_Driver_Write7SegNumberHex(note_number, 1, 2, LED_Driver_RIGHT_ALIGN);    
}

void InitializeKeyAssigner(key_assigner_t *key_assigner, voice_t *voices[], int num_voices)
{
    key_assigner->voices = voices;
    key_assigner->num_voices = num_voices;
    key_assigner->index_next_voice = 0;
}

void SetUpDuophonic(key_assigner_t *assigner)
{
    InitializeVoice(duo_voices[0], Gate1On, Gate1Off);
    InitializeVoice(duo_voices[1], Gate2On, Gate2Off);
    InitializeKeyAssigner(assigner, duo_voices, 2);
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

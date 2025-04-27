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

/*
 * Generic voice configuration.
 */

#pragma once

#include <stdint.h>

enum Voice {
    VOICE_1 = 0,
    VOICE_2,
};
#define NUM_VOICES 2

typedef struct voice_config {
    void (*set_note)(uint8_t note_number);
    void (*gate_on)(uint8_t velocity);
    void (*gate_off)();
} voice_config_t;

extern void GetVoiceConfigs(voice_config_t voice_configs[], unsigned size);  // implemented in hardware.c

/* [] END OF FILE */

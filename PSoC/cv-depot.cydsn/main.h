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

#pragma once

#include <stdint.h>

// Firmware runtime modes
enum ProgramMode {
    MODE_NORMAL = 0,
    MODE_MENU_SELECTING,
    MODE_MENU_SELECTED,
    MODE_MIDI_CHANNEL_SETUP,
    MODE_MIDI_CHANNEL_CONFIRMED,
    MODE_KEY_ASSIGNMENT_SETUP,
    MODE_KEY_ASSIGNMENT_CONFIRMED,
    MODE_CALIBRATION_INIT,
    MODE_CALIBRATION_BEND_WIDTH,
    MODE_CALIBRATION_BEND_CONFIRMED,
};

enum Voice {
    VOICE_1 = 0,
    VOICE_2,
};

#define BEND_STEPS 1024

// extern volatile int8_t menu_item;
extern volatile uint8_t mode;
extern uint16_t bend_offset;
extern void Calibrate();

// macros
#define GREEN_ENCODER_LED_ON(x) Pin_Encoder_LED_1_Write(1)
#define GREEN_ENCODER_LED_OFF(x) Pin_Encoder_LED_1_Write(0)
#define RED_ENCODER_LED_ON(x) Pin_Encoder_LED_2_Write(1)
#define RED_ENCODER_LED_OFF(x) Pin_Encoder_LED_2_Write(0)

/* [] END OF FILE */

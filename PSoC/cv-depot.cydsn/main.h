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
    MODE_MENU_INVOKING,
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

extern volatile uint8_t mode;
extern void Calibrate();  // implemented in calibration.c
extern void Diagnose();   // implemented in diagnosis.c

/* [] END OF FILE */

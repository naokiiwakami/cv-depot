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

#ifndef MAIN_H
#define MAIN_H

#include "stdint.h"

// Firmware runtime modes
enum ProgramMode {
    MODE_NORMAL = 0,
    MODE_MENU_SELECTING,
    MODE_MENU_SELECTED,
    MODE_MIDI_CHANNEL_SETUP,
    MODE_MIDI_CHANNEL_CONFIRMED,
    MODE_CALIBRATION_INIT,
    MODE_CALIBRATION_BEND_WIDTH,
    MODE_CALIBRATION_BEND_CONFIRMED,
};

enum Voice {
    VOICE_1,
    VOICE_2,
};

extern volatile uint8_t mode;
extern uint16_t bend_offset;
extern void Calibrate();

#endif  // MAIN_H
/* [] END OF FILE */

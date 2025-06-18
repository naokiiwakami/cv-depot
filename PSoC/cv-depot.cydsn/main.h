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

// Task management
typedef struct task {
    void (*run)(void *);
    void *arg;
} task_t;

extern void ScheduleTask(task_t task);

/* [] END OF FILE */

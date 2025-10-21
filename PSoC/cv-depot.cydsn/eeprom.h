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

#include "project.h"

#include "stdint.h"

#define ADDR_BEND_OFFSET 0x01
#define ADDR_BEND_OCTAVE_WIDTH 0x04
#define ADDR_NOTE_1_WIPER 0x06
#define ADDR_NOTE_2_WIPER 0x07
#define ADDR_MIDI_CH_1 0x08
#define ADDR_MIDI_CH_2 0x09
#define ADDR_KEY_ASSIGNMENT_MODE 0x0a
#define ADDR_KEY_PRIORITY 0x0b
#define ADDR_MODULE_UID 0x1c
#define ADDR_NAME 0x20 /* - 0x60 */
#define ADDR_GATE_TYPE 0x60

#define ADDR_UNSET 0xffff

extern uint8_t ReadEepromWithValueCheck(uint16 address, uint8_t max);

extern cystatus Save16(uint16_t data, uint16_t address);
extern uint16_t Load16(uint16_t address);

extern cystatus Save32(uint32_t data, uint16_t address);
extern uint32_t Load32(uint16_t address);

extern void SaveString(const char *string, size_t max_length, uint16_t address);
extern void LoadString(char *string, size_t max_length, uint16_t address);

/* [] END OF FILE */

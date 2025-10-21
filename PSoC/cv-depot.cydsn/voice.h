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

enum GateType {
    GATE_TYPE_VELOCITY = 0,  // gate includes velocity info
    GATE_TYPE_LEGACY,        // legacy 8V gate
    GATE_TYPE_END,
};

typedef struct voice_config {
    void (*set_note)(uint8_t note_number);
    void (*gate_on)(uint8_t velocity);
    void (*gate_off)();
} voice_config_t;

extern enum GateType gate_type;
extern void GetVoiceConfigs(voice_config_t voice_configs[], unsigned size);  // implemented in hardware.c
/**
 * Updates the gate_type to a new value.
 *
 * @param new_gate_type - New gate type
 * @returns 1 when the value has changed, 0 otherwise
 */
extern int8_t UpdateGateType(enum GateType new_gate_type);

/* [] END OF FILE */

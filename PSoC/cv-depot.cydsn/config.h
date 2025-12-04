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

#include "analog3.h"

// type of this module
#define MODULE_TYPE_CV_DEPOT 1

// Property IDs
#define PROP_NUM_VOICES 3
#define PROP_KEY_ASSIGNMENT_MODE 4
#define PROP_KEY_PRIORITY 5
#define PROP_MIDI_CHANNELS 6
#define PROP_GATE_TYPE 7
#define PROP_BEND_DEPTH 8
#define PROP_EXPRESSION_OR_BREATH 9
#define NUM_PROPS 10
/*
TBD
#define PROP_RETRIGGER 7
#define TYPE_RETRIGGER A3_U8
#define PROP_PORTAMENT_MODE 8
#define TYPE_PORTAMENT_MODE A3_U8
#define PROP_PORTAMENT_DIRECTION 9
#define TYPE_PORTAMENT_DIRECTION A3_U8
#define PROP_PORTAMENT_TIME 10
#define TYPE_PORTAMENT_TYPE A3_U8
*/

extern char module_name[A3_MAX_CONFIG_DATA_LENGTH];

extern a3_property_t config[NUM_PROPS];

/* [] END OF FILE */

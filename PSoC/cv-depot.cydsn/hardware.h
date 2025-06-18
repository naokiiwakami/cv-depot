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
 * Place hardware control materials here.
 */

#pragma once

#include <stdint.h>

#define BEND_STEPS 1024

extern uint16_t bend_offset;

extern void InitializeVoiceControl();

extern void BlinkGreen(uint16_t interval_ms, uint16_t times);
extern void BlinkRed(uint16_t interval_ms, uint16_t times);

// macros
#define GET_GREEN_ENCODER_LED(x) Pin_Encoder_LED_1_Read()
#define SET_GREEN_ENCODER_LED(x) Pin_Encoder_LED_1_Write(x)
#define GREEN_ENCODER_LED_ON(x) SET_GREEN_ENCODER_LED(1)
#define GREEN_ENCODER_LED_OFF(x) SET_GREEN_ENCODER_LED(0)
#define GREEN_ENCODER_LED_TOGGLE(x) SET_GREEN_ENCODER_LED(!GET_GREEN_ENCODER_LED())

#define GET_RED_ENCODER_LED(x) Pin_Encoder_LED_2_Read()
#define SET_RED_ENCODER_LED(x) Pin_Encoder_LED_2_Write(x)
#define RED_ENCODER_LED_ON(x) SET_RED_ENCODER_LED(1)
#define RED_ENCODER_LED_OFF(x) SET_RED_ENCODER_LED(0)
#define RED_ENCODER_LED_TOGGLE(x) SET_RED_ENCODER_LED(!GET_RED_ENCODER_LED())

/* [] END OF FILE */

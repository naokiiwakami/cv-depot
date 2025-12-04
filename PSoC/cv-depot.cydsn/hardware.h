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

// CAN
#define MAILBOX_MC 0
#define MAILBOX_OTHERS 15
typedef struct _can_message {
    uint32_t id;
    uint8_t extended;
    uint8_t dlc;
    uint8_t data[8];
} can_message_t;

#define MESSAGE_QUEUE_SIZE 8
extern can_message_t message_queue[MESSAGE_QUEUE_SIZE];
extern uint32_t q_head;
extern uint32_t q_tail;
extern uint8_t q_full;

#define BEND_STEPS 1024

extern uint16_t bend_offset;
extern uint16_t bend_octave_width;
extern uint32_t bend_halftone_width; // Q26.6
extern uint8_t bend_depth;

extern void UpdateBendDepth(uint8_t new_bend_depth);
extern void BendPitch(int16_t bend_amount);

extern void SetExpression(uint8_t value);
extern void SetModulation(uint8_t value);

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

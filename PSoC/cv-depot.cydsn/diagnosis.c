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

#include "project.h"

#include "main.h"

void Diagnose() {
    Pin_Gate_1_Write(1);
    Pin_Gate_2_Write(1);
    const int32_t weight = 1 << 20;
    for (int32_t counter = 0; counter < 16 * weight; ++counter) {
        if (counter % weight != 0) {
            continue;
        }
        int8_t value = counter / weight;
        LED_Driver_Write7SegDigitHex(value, 0);
        LED_Driver_Write7SegDigitHex(value, 1);
        LED_Driver_Write7SegDigitHex(value, 2);
        Pin_Encoder_LED_1_Write(1 - value / 8);
        Pin_Encoder_LED_2_Write(value / 8);
        PWM_Indicators_WriteCompare1(value * 8 - 1);
        PWM_Indicators_WriteCompare2(value * 8 - 1);
        Pin_Gate_1_Write(1);
        Pin_Gate_2_Write(1);
    }
    Pin_Gate_1_Write(0);
    Pin_Gate_2_Write(0);
    Pin_Encoder_LED_1_Write(0);
    Pin_Encoder_LED_2_Write(0);
    LED_Driver_ClearDisplayAll();
    mode = MODE_NORMAL;
}

/* [] END OF FILE */

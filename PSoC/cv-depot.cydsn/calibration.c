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

#include "eeprom.h"
#include "main.h"
#include "pot_change.h"
#include "voice.h"
#include "hardware.h"

/**
 * Find bend width for a octave (i.e., 1V) manually.
 *
 * Strategy:
 * The user already know current CV output by a tester reading. The module runs binary search
 * targeting 1V above the current voltage. The module changes the voltage, then the user answers
 * "raise" or "lower" by turning the encoder. User would press button when the voltage reaches
 * the target, then this method exits.
 *
 */
static uint16_t FindBendOctaveWidth()
{
    QuadDec_SetCounter(0);
    int16_t counter_last_value = 0;
    uint16_t bend_lower = bend_offset;  // we know the target is 1V higher than the starting point.
    uint16_t bend_upper = BEND_STEPS;

    int16_t bend = bend_offset;
    bend += Load16(ADDR_BEND_OCTAVE_WIDTH);;
    if (bend < bend_lower || bend > bend_upper) {
        // in case the stored data is coruppted and the bend gets out of range
        bend = (bend_lower + bend_upper) / 2;
    }

    PWM_Bend_WriteCompare(bend);

    // Run binary search. User would decide to go higher or lower.
    while (mode == MODE_CALIBRATION_BEND_WIDTH) {
        int16_t counter_value = QuadDec_GetCounter();
        if (counter_value == counter_last_value) {
            continue;
        }
        if (counter_value < counter_last_value) {
            bend_upper = bend;
            LED_Driver_WriteString7Seg("dwn", 0);
        } else if (counter_value > counter_last_value) {
            bend_lower = bend;
            LED_Driver_WriteString7Seg("up ", 0);
        }
        counter_last_value = counter_value;
        bend = (bend_lower + bend_upper) / 2;
        PWM_Bend_WriteCompare(bend);
    }
    LED_Driver_ClearDisplayAll();
    return bend - bend_offset;
}

#define MEASUREMENT_WAIT_MS 256
#define WAIT(x) CyDelay(MEASUREMENT_WAIT_MS)

uint16_t BendToReference(int16_t initial_bend)
{
    uint16_t bend_lower = 0;
    uint16_t bend_upper = BEND_STEPS;
    uint16_t bend = initial_bend >= 0 ? initial_bend : (bend_lower + bend_upper) / 2;
    PWM_Bend_WriteCompare(bend);
    WAIT();
    uint8_t reading = Pin_Adjustment_In_Read();
    Pin_LED_Write(reading);
    do {
        bend = (bend_lower + bend_upper) / 2;
        PWM_Bend_WriteCompare(bend);
        WAIT();
        reading = Pin_Adjustment_In_Read();
        Pin_LED_Write(reading);
        if (reading) {
            bend_lower = bend;
        } else {
            bend_upper = bend;
        }
    } while (bend_upper - bend_lower > 1);
    return bend;
}

void ChangeWiper(pot_t *pot, uint16_t wiper)
{
    LED_Driver_Write7SegNumberDec(wiper, 1, 2, LED_Driver_RIGHT_ALIGN);
    PotSetTargetPosition(pot, wiper);
    while (!PotUpdate(pot)) {}
}

struct calib_config {
    enum Voice voice;
    uint8_t voice_ram;
    pot_t *pot;
    uint8_t comparator_switch;
    uint16_t save_address;
};

inline static void SetNote(enum Voice voice, uint8_t note_number)
{
    if (voice == VOICE_1) {
        PWM_Notes_WriteCompare1(note_number);
    } else {
        PWM_Notes_WriteCompare2(note_number);
    }
}

void SetUpOctaveMeasurement(struct calib_config *config, uint16_t wiper, uint16_t bend_octave_width)
{
    ChangeWiper(config->pot, wiper);
    SetNote(config->voice, 36);
    bend_offset = BendToReference(-1);
    PWM_Bend_WriteCompare(bend_offset - bend_octave_width);
    SetNote(config->voice, 48);
}

inline static uint8_t min(uint8_t x, uint8_t y)
{
    return x < y ? x : y;
}

inline static uint8_t max(uint8_t x, uint8_t y)
{
    return x > y ? x : y;
}

void CalibrateNoteCV(struct calib_config *config, uint16_t bend_octave_width)
{
    RED_ENCODER_LED_ON();

    LED_Driver_SetDisplayRAM(config->voice_ram, 0);
    Pin_Adj_S0_Write(config->comparator_switch);

    uint8_t lowest = 0;
    uint8_t highest = 63;
    uint8_t midpoint = highest / 2;
    uint8_t step = 1;
    uint8_t low = midpoint - step;
    uint8_t high = midpoint + step;
    uint8_t wiper;
    uint8_t reading;
    do {
        wiper = (low + high) / 2;
        SetUpOctaveMeasurement(config, wiper, bend_octave_width);
        WAIT();
        reading = Pin_Adjustment_In_Read();
        Pin_LED_Write(reading);
        if (reading) {
            low = wiper + 1;
            lowest = low;
            if (low == high && high < highest) {
                step *= 2;
                high = min(midpoint + step, highest);
            }
        } else {
            high = wiper -1;
            highest = high;
            if (low == high && low > lowest) {
                step *= 2;
                low = max(midpoint - step, lowest);
            }
        }
    } while (high - low > 0);

    BlinkGreen(100, 9);

    ChangeWiper(config->pot, low);

    // save the wiper position
    EEPROM_WriteByte(config->pot->current, config->save_address);

    CyDelay(950);
}

#if 0
void CalibrateNoteCV(struct calib_config *config, uint16_t bend_octave_width)
{
    RED_ENCODER_LED_ON();

    LED_Driver_SetDisplayRAM(config->voice_ram, 0);
    Pin_Adj_S0_Write(config->comparator_switch);

    uint8_t wiper_lowest = 0;
    uint8_t wiper_highest = 63;
    uint8_t wiper;
    uint8_t reading;
    do {
        wiper = (wiper_lowest + wiper_highest) / 2;
        SetUpOctaveMeasurement(config, wiper, bend_octave_width);
        WAIT();
        reading = Pin_Adjustment_In_Read();
        Pin_LED_Write(reading);
        if (reading) {
            wiper_lowest = wiper + 1;
        } else {
            wiper_highest = wiper -1;
        }
    } while (wiper_highest - wiper_lowest > 0);

    BlinkGreen(100, 5);

    ChangeWiper(config->pot, wiper_highest);

    // save the wiper position
    EEPROM_WriteByte(config->pot->current, config->save_address);

    CyDelay(950);
}
#endif

void Calibrate()
{
    // turn off portament
    Pin_Portament_En_Write(0);

    mode = MODE_CALIBRATION_INIT;
    RED_ENCODER_LED_ON();

    LED_Driver_WriteString7Seg("___", 0);
    // set note C2
    PWM_Notes_WriteCompare1(12);
    PWM_Notes_WriteCompare2(12);

    // move the bender to the center position
    PWM_Bend_WriteCompare(bend_offset);

    // Halt while reading current value. User will proceed the mode when ready
    while (mode == MODE_CALIBRATION_INIT) {}

    LED_Driver_SetDisplayRAM(1, 0);
    LED_Driver_SetDisplayRAM(1, 1);
    LED_Driver_SetDisplayRAM(1, 2);

    // Find the bend width for a octave manually
    uint16_t bend_octave_width = FindBendOctaveWidth();
    Save16(bend_octave_width, ADDR_BEND_OCTAVE_WIDTH);

    // Adjust the note CV ranges now
    Pin_Adj_En_Write(1);

    // note 1
    struct calib_config config_1 = {
        .voice = VOICE_1,
        .voice_ram = 0x1 << 5,
        .pot = &pot_note_1,
        .comparator_switch = 0,
        .save_address = ADDR_NOTE_1_WIPER,
    };
    CalibrateNoteCV(&config_1, bend_octave_width);

    // note 2
    struct calib_config config_2 = {
        .voice = VOICE_2,
        .voice_ram = 0x1 << 2,
        .pot = &pot_note_2,
        .comparator_switch = 1,
        .save_address = ADDR_NOTE_2_WIPER,
    };
    CalibrateNoteCV(&config_2, bend_octave_width);

    // wrap up
    bend_offset = BEND_STEPS / 2;
    PWM_Bend_WriteCompare(bend_offset);
    Pin_Adj_En_Write(0);
    Pin_Encoder_LED_1_Write(0);
    Pin_Encoder_LED_2_Write(0);
    LED_Driver_ClearDisplayAll();
}

/* [] END OF FILE */

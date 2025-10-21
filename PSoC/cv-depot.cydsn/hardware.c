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
#include "hardware.h"
#include "pot.h"
#include "pot_change.h"
#include "voice.h"

uint16_t bend_offset;

enum GateType gate_type;

// CAN message queue
can_message_t message_queue[MESSAGE_QUEUE_SIZE];
uint32_t q_head = 0;
uint32_t q_tail = 0;
uint8_t q_full = 0;

// macros
#define SetNote1 PWM_Notes_WriteCompare1
#define SetNote2 PWM_Notes_WriteCompare2

// TODO: Make this value adjustable
#define GATE_ON_POINT 680
// #define GATE_ON_POINT 639
#define GATE_DAC_STEPS 2040
#define FIXED_POINT_BITSHIFT 18
#define MAX_VELOCITY 127
#define VELOCITY_FACTOR \
    (uint32_t) (((GATE_DAC_STEPS - GATE_ON_POINT) << FIXED_POINT_BITSHIFT) / MAX_VELOCITY / MAX_VELOCITY)
#define VELOCITY_DAC_VALUE(velocity) ((((velocity) * (velocity) * (VELOCITY_FACTOR)) >> FIXED_POINT_BITSHIFT) + GATE_ON_POINT)
#define INDICATOR_VALUE(velocity) ((velocity) >= 64 ? (((velocity) - 63)  * ((velocity) - 63)) / 32 - 1 : 0)
#define NOTE_PWM_MAX_VALUE 120

static void Gate1On(uint8_t velocity)
{
    uint16_t gate_level = VELOCITY_DAC_VALUE(velocity);
    DVDAC_Velocity_1_SetValue(gate_level);
    PWM_Indicators_WriteCompare1(INDICATOR_VALUE(velocity));
    Pin_Gate_1_Write(1);
}

static void Gate1OnLegacy(uint8_t velocity)
{
    (void)velocity;
    Gate1On(127);
}

static void Gate1Off()
{
    Pin_Portament_En_Write(0);
    DVDAC_Velocity_1_SetValue(0);
    Pin_Gate_1_Write(0);
}

static void Gate2On(uint8_t velocity)
{
    DVDAC_Velocity_2_SetValue(VELOCITY_DAC_VALUE(velocity));
    PWM_Indicators_WriteCompare2(INDICATOR_VALUE(velocity));
    Pin_Gate_2_Write(1);
}

static void Gate2OnLegacy(uint8_t velocity)
{
    (void)velocity;
    Gate2On(127);
}

static void Gate2Off()
{
    DVDAC_Velocity_2_SetValue(0);
    Pin_Gate_2_Write(0);
}

void GetVoiceConfigs(voice_config_t voice_configs[], unsigned size)
{
    if (size < 1) {
        return;
    }
    voice_configs[0].set_note = SetNote1;
    voice_configs[0].gate_on = gate_type == GATE_TYPE_VELOCITY ? Gate1On : Gate1OnLegacy;
    voice_configs[0].gate_off = Gate1Off;

    if (size < 2) {
        return;
    }
    voice_configs[1].set_note = SetNote2;
    voice_configs[1].gate_on = gate_type == GATE_TYPE_VELOCITY ? Gate2On : Gate2OnLegacy;
    voice_configs[1].gate_off = Gate2Off;
}

int8_t UpdateGateType(enum GateType new_gate_type)
{
    if (new_gate_type == gate_type) {
        // no change, do nothing
        return 0;
    }
    EEPROM_WriteByte(new_gate_type, ADDR_GATE_TYPE);
    gate_type = new_gate_type;
    return 1;
}

void InitializeVoiceControl()
{
    // setup hardware
    Pin_Portament_En_Write(0);
    Pin_Adj_En_Write(0);
    Pin_Adj_S0_Write(0);
    PWM_Notes_Start();
    PWM_Bend_Start();
    PWM_Indicators_Start();
    DVDAC_Velocity_1_Start();
    DVDAC_Velocity_2_Start();
    DVDAC_Expression_Start();
    DVDAC_Modulation_Start();

    // Note CV
    uint8_t wiper = EEPROM_ReadByte(ADDR_NOTE_1_WIPER);
    PotChangePlaceRequest(&pot_note_1, -1);  // move to termianl B to ensure the starting position
    PotChangePlaceRequest(&pot_note_1, wiper);

    wiper = EEPROM_ReadByte(ADDR_NOTE_2_WIPER);
    PotChangePlaceRequest(&pot_note_2, -1);  // move to termianl B to ensure the starting position
    PotChangePlaceRequest(&pot_note_2, wiper);

    // Gate type
    gate_type = EEPROM_ReadByte(ADDR_GATE_TYPE);
    if (gate_type > GATE_TYPE_LEGACY) {
        gate_type = GATE_TYPE_VELOCITY;
    }

    /*
    uint8_t temp = EEPROM_ReadByte(ADDR_BEND_OFFSET);
    if (temp == 0xd1) {
        temp = EEPROM_ReadByte(ADDR_BEND_OFFSET + 1);
        bend_offset = temp << 8;
        temp = EEPROM_ReadByte(ADDR_BEND_OFFSET + 2);
        bend_offset += temp;
    } else { // initial value
        bend_offset = BEND_STEPS / 2;
    }
    */
    // bend_offset = BEND_STEPS / 2;

    // Gates
    // Gate1Off();
    // Gate2Off();

    // Bend
    // BendPitch(0x00, PITCH_BEND_CENTER);  // set neutral

    // Portament
    Pin_Portament_En_Write(0);
    // move pot terminals to B to ensure the starting positions
    PotChangePlaceRequest(&pot_portament_1, -1);
    PotChangePlaceRequest(&pot_portament_2, -1);
    // then set the pot values
    PotChangePlaceRequest(&pot_portament_1, 2);
    PotChangePlaceRequest(&pot_portament_2, 2);

}

static void InitSysTimer(uint16_t interval_ms)
{
    CySysTickInit();
    CySysTickSetClockSource(CY_SYS_SYST_CSR_CLK_SRC_LFCLK);
    CySysTickSetReload(100000 / 1000 * interval_ms);
    CySysTickDisableInterrupt();
    CySysTickEnableInterrupt();
}

static void StartSysTimer()
{
    CySysTickClear();
    CySysTickEnable();
}

static uint16_t blink_count = 0;

static void GreenBlinker()
{
    GREEN_ENCODER_LED_TOGGLE();
    if (--blink_count == 0) {
        CySysTickStop();
    }
}

static void RedBlinker()
{
    RED_ENCODER_LED_TOGGLE();
    if (--blink_count == 0) {
        CySysTickStop();
    }
}

void BlinkGreen(uint16_t interval_ms, uint16_t times)
{
    InitSysTimer(interval_ms);
    CySysTickSetCallback(0, GreenBlinker);
    blink_count = times;
    GREEN_ENCODER_LED_ON();
    RED_ENCODER_LED_OFF();
    StartSysTimer();
}

void BlinkRed(uint16_t interval_ms, uint16_t times)
{
    InitSysTimer(interval_ms);
    CySysTickSetCallback(0, RedBlinker);
    blink_count = times;
    RED_ENCODER_LED_ON();
    GREEN_ENCODER_LED_OFF();
    StartSysTimer();
}

/* [] END OF FILE */

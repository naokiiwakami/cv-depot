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

#include <stdint.h>

#include "project.h"

#include "main.h"
#include "midi.h"
#include "eeprom.h"

typedef struct menu {
    const char *name;
    void (*func)();
} menu_t;

static void SetKeyAssignment();
static void SetMidiChannel1();
static void SetMidiChannel2();
static void Diagnose();

// Menu items would change by the configuration. The menu is built on demand by the switch interrupt
// handler. It should be done quickly, so the menu items are kept in the static space.
static const menu_t kMenuSetChannel = { "ch ", SetMidiChannel1 }; // set MIDI channels for the both voices
static const menu_t kMenuSetChannel1 = { "ch1", SetMidiChannel1 }; // set MIDI channel for voice 1
static const menu_t kMenuSetChannel2 = { "ch2", SetMidiChannel2 }; // set MIDI channel for voice 2
static const menu_t kMenuSetKeyAssignment = { "asn", SetKeyAssignment };
static const menu_t kMenuCalibrate = { "cal", Calibrate }; // calibrate bend center position
static const menu_t kMenuDiagnose = { "dgn", Diagnose }; // diagnose hardware

#define MAX_MENU_SIZE 8
static volatile menu_t menu[MAX_MENU_SIZE];
static volatile uint8_t menu_size = 0;

static volatile int16_t prev_counter_value = 0;

static volatile int8_t menu_item = -1;

static void BuildMenu()
{
    uint8_t i = 0;
    if (midi_config.key_assignment_mode != KEY_ASSIGN_PARALLEL) {
        menu[i++] = kMenuSetChannel;
    } else {
        menu[i++] = kMenuSetChannel1;
        menu[i++] = kMenuSetChannel2;
    }
    menu[i++] = kMenuSetKeyAssignment;
    menu[i++] = kMenuCalibrate;
    menu[i++] = kMenuDiagnose;
    menu_size = i;
}

static volatile uint8_t blink_count = 0;

static void RedBlink()
{
    RED_ENCODER_LED_TOGGLE();
    if (--blink_count == 0) {
        CySysTickStop();
    }
}

static void StartRedBlinking() {
    CySysTickInit();
    CySysTickSetClockSource(CY_SYS_SYST_CSR_CLK_SRC_LFCLK);
    CySysTickSetReload(100000 / 1000 * 100); // 100ms
    CySysTickDisableInterrupt();
    CySysTickSetCallback(0, RedBlink);
    CySysTickEnableInterrupt();
    CySysTickClear();
    blink_count = 10;
    RED_ENCODER_LED_ON();
    GREEN_ENCODER_LED_OFF();
    CySysTickEnable();
}

static void SetMidiChannel(enum Voice selected_voice)
{
    mode = MODE_MIDI_CHANNEL_SETUP;
    midi_config.selected_voice = selected_voice;
    GREEN_ENCODER_LED_ON();
    RED_ENCODER_LED_ON();
    int16_t encoder_value = midi_config.midi_channels[selected_voice];
    QuadDec_SetCounter(encoder_value);
    prev_counter_value = -1;
}

void SetMidiChannel1()
{
    SetMidiChannel(VOICE_1);
}

void SetMidiChannel2()
{
    SetMidiChannel(VOICE_2);
}

static void SetKeyAssignment()
{
    mode = MODE_KEY_ASSIGNMENT_SETUP;
    GREEN_ENCODER_LED_ON();
    RED_ENCODER_LED_ON();
    QuadDec_SetCounter(midi_config.key_assignment_mode);
    prev_counter_value = -1;
}

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

static int8_t PickUpChangedEncoderValue(uint8_t range)
{
    int16_t counter_value = QuadDec_GetCounter();
    if (counter_value == prev_counter_value) {
        return -1;
    }
    prev_counter_value = counter_value;
    if (counter_value >= 0) {
        return counter_value % range;
    }
    return (counter_value + 1) % range + range - 1; 
}

// Setting mode handlers ///////////////////////////////////////////////

static void HandleMenuSelection()
{
    int8_t index = PickUpChangedEncoderValue(menu_size);
    if (index >= 0) {
        LED_Driver_WriteString7Seg(menu[index].name, 0);
    }
}

static void ConfirmMenuSelection()
{
    if (menu_item >= 0) {
        uint8_t temp = menu_item;
        menu_item = -1;
        menu[temp].func();
    }
}

static void HandleMidiChannelSetup()
{
    int8_t value = PickUpChangedEncoderValue(NUM_MIDI_CHANNELS);
    if (value >= 0) {
        midi_config.midi_channels[midi_config.selected_voice] = value;
        LED_Driver_Write7SegNumberDec(value + 1, 1, 2, LED_Driver_RIGHT_ALIGN);
        if (midi_config.key_assignment_mode == KEY_ASSIGN_PARALLEL) {
            if (midi_config.midi_channels[VOICE_1] == midi_config.midi_channels[VOICE_2]) {
                GREEN_ENCODER_LED_OFF();
            } else {
                GREEN_ENCODER_LED_ON();
            }
        }
    }
}

static void ConfirmMidiChannelSetup()
{
    if (midi_config.key_assignment_mode == KEY_ASSIGN_PARALLEL
        && midi_config.midi_channels[VOICE_1] == midi_config.midi_channels[VOICE_2]) {
        // Error, go back to setup mode
        mode = MODE_MIDI_CHANNEL_SETUP;
        StartRedBlinking();
        return;
    }
    GREEN_ENCODER_LED_OFF();
    RED_ENCODER_LED_OFF();
    CommitMidiChannelChange();
    LED_Driver_ClearDisplayAll();
    mode = MODE_NORMAL;
}

static void HandleKeyAssignmentModeSetup()
{
    int8_t value = PickUpChangedEncoderValue(KEY_ASSIGN_END);
    if (value >= 0) {
        LED_Driver_WriteString7Seg(kKeyAssignmentModeName[value], 0);
        midi_config.key_assignment_mode = value;
    }
}

static void ConfirmKeyAssignmentMode()
{
    GREEN_ENCODER_LED_OFF();
    RED_ENCODER_LED_OFF();
    if (midi_config.key_assignment_mode == KEY_ASSIGN_PARALLEL) {
        if (midi_config.midi_channels[VOICE_1] == midi_config.midi_channels[VOICE_2]) {
          SetMidiChannel(VOICE_2);
          return;
        }
    } else if (midi_config.midi_channels[VOICE_1] != midi_config.midi_channels[VOICE_2]) {
        SetMidiChannel1();
        return;
    }
    CommitKeyAssignmentModeChange();
    LED_Driver_ClearDisplayAll();
    mode = MODE_NORMAL;
}

void HandleSettingModes()
{
    switch (mode) {
    case MODE_MENU_SELECTING:
        HandleMenuSelection();
        break;
    case MODE_MENU_SELECTED:
        ConfirmMenuSelection();
        break;
    case MODE_MIDI_CHANNEL_SETUP:
        HandleMidiChannelSetup();
        break;
    case MODE_MIDI_CHANNEL_CONFIRMED:
        ConfirmMidiChannelSetup();
        break;
    case MODE_KEY_ASSIGNMENT_SETUP:
        HandleKeyAssignmentModeSetup();
        break;
    case MODE_KEY_ASSIGNMENT_CONFIRMED:
        ConfirmKeyAssignmentMode();
        break;
    }
}

void HandleSwitchEvent()
{
    switch (mode) {
    case MODE_NORMAL: {
        BuildMenu();
        QuadDec_SetCounter(0);
        prev_counter_value = - 1;
        mode = MODE_MENU_SELECTING;
        GREEN_ENCODER_LED_ON();
        break;
    }
    case MODE_MENU_SELECTING: {
        int16_t counter_value = QuadDec_GetCounter();
        menu_item = counter_value % menu_size;
        mode = MODE_MENU_SELECTED;
        GREEN_ENCODER_LED_OFF();
        QuadDec_SetCounter(0);
        prev_counter_value = -1;
        break;
    }
    case MODE_KEY_ASSIGNMENT_SETUP:
        mode = MODE_KEY_ASSIGNMENT_CONFIRMED;
        break;
    case MODE_MIDI_CHANNEL_SETUP:
        mode = MODE_MIDI_CHANNEL_CONFIRMED;
        break;
    case MODE_CALIBRATION_INIT:
        mode = MODE_CALIBRATION_BEND_WIDTH;
        break;
    case MODE_CALIBRATION_BEND_WIDTH:
        mode = MODE_NORMAL;
        break;
    }
}

/* [] END OF FILE */

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

#include <stdint.h>

#include "project.h"

#include "hardware.h"
#include "main.h"
#include "midi.h"
#include "eeprom.h"
#include "voice.h"

typedef struct menu {
    const char *name;
    void (*func)();
} menu_t;

static int8_t PickUpChangedEncoderValue(uint8_t range);
static void StartFinalization();

static void InitiateKeyAssignSetup();
static void InitiateMidiChannelSetup1();
static void InitiateMidiChannelSetup2();
static void InitiateGateTypeSetup();

// Menu items would change by the configuration. The menu is built on demand by the switch interrupt
// handler. It should be done quickly, so the menu items are kept in the static space.
static const menu_t kMenuSetChannel = { "ch ", InitiateMidiChannelSetup1 }; // for the both voices
static const menu_t kMenuSetChannel1 = { "ch1", InitiateMidiChannelSetup1 }; // for voice 1
static const menu_t kMenuSetChannel2 = { "ch2", InitiateMidiChannelSetup2 }; // for voice 2
static const menu_t kMenuSetKeyAssignment = { "asn", InitiateKeyAssignSetup };
static const menu_t kMenuSetGateType = { "gat", InitiateGateTypeSetup };
static const menu_t kMenuCalibrate = { "cal", Calibrate }; // calibrate the bend center position
static const menu_t kMenuDiagnose = { "dgn", Diagnose }; // diagnose the hardware

const char *kKeyAssignmentModeName[KEY_ASSIGN_END] = { "duo", "uni", "par" };
const char *kGateTypeName[GATE_TYPE_END] = { "a3 ", "leg" };

// Setup operation states ///////////////////////

#define MAX_MENU_SIZE 8

struct menu_selection {
    const menu_t *menu[MAX_MENU_SIZE];
    uint8_t menu_size;
    uint8_t menu_item;
};

struct midi_setup {
    enum Voice selected_voice;
    midi_config_t config;
    uint8_t blink_count; // used for error indication
};

// Structure to keep track of the setup state
struct setup_state {
    int16_t prev_counter_value;
    union mode_specific {
        struct menu_selection menu;
        struct midi_setup midi;
        enum GateType gate_type;
    } mode;
};

static struct setup_state setup_state = {};

static void BuildMenu()
{
    uint8_t i = 0;
    if (GetMidiConfig()->key_assignment_mode != KEY_ASSIGN_PARALLEL) {
        setup_state.mode.menu.menu[i++] = &kMenuSetChannel;
    } else {
        setup_state.mode.menu.menu[i++] = &kMenuSetChannel1;
        setup_state.mode.menu.menu[i++] = &kMenuSetChannel2;
    }
    setup_state.mode.menu.menu[i++] = &kMenuSetKeyAssignment;
    setup_state.mode.menu.menu[i++] = &kMenuSetGateType;
    setup_state.mode.menu.menu[i++] = &kMenuCalibrate;
    setup_state.mode.menu.menu[i++] = &kMenuDiagnose;
    setup_state.mode.menu.menu_size = i;
}

static void InitiateMidiChannelSetup(const midi_config_t *midi_config, enum Voice selected_voice)
{
    mode = MODE_MIDI_CHANNEL_SETUP;
    GREEN_ENCODER_LED_ON();
    RED_ENCODER_LED_ON();
    int16_t encoder_value = midi_config->channels[selected_voice];
    QuadDec_SetCounter(encoder_value);
    setup_state.prev_counter_value = -1;
    setup_state.mode.midi.config = *midi_config;
    setup_state.mode.midi.selected_voice = selected_voice;
    setup_state.mode.midi.blink_count = 0;

    uint8_t value = selected_voice == VOICE_1 ? 0x1 << 5 : 0x1 << 2;
    if (midi_config->key_assignment_mode != KEY_ASSIGN_PARALLEL) {
        value |= 0x1 << 2;
    }
    LED_Driver_SetDisplayRAM(value, 0);
}

void InitiateMidiChannelSetup1()
{
    InitiateMidiChannelSetup(GetMidiConfig(), VOICE_1);
}

void InitiateMidiChannelSetup2()
{
    InitiateMidiChannelSetup(GetMidiConfig(), VOICE_2);
}

void InitiateKeyAssignSetup()
{
    mode = MODE_KEY_ASSIGNMENT_SETUP;
    GREEN_ENCODER_LED_ON();
    RED_ENCODER_LED_ON();
    const midi_config_t *midi_config = GetMidiConfig();
    QuadDec_SetCounter(midi_config->key_assignment_mode);
    setup_state.prev_counter_value = -1;
    setup_state.mode.midi.config = *midi_config;
    setup_state.mode.midi.blink_count = 0;
}

void InitiateGateTypeSetup()
{
    mode = MODE_GATE_TYPE_SETUP;
    GREEN_ENCODER_LED_ON();
    RED_ENCODER_LED_ON();
    setup_state.mode.gate_type = gate_type;
    QuadDec_SetCounter(gate_type);
    setup_state.prev_counter_value = -1;
}

// Settings event handlers ///////////////////////////////////////////////////

static void InvokeMenu()
{
    BuildMenu();
    QuadDec_SetCounter(0);
    setup_state.prev_counter_value = - 1;
    mode = MODE_MENU_SELECTING;
    GREEN_ENCODER_LED_ON();
}

static void HandleMenuSelection()
{
    int8_t index = PickUpChangedEncoderValue(setup_state.mode.menu.menu_size);
    if (index >= 0) {
        LED_Driver_WriteString7Seg(setup_state.mode.menu.menu[index]->name, 0);
        setup_state.mode.menu.menu_item = index;
    }
}

static void ConfirmMenuSelection()
{
    GREEN_ENCODER_LED_OFF();
    if (setup_state.mode.menu.menu_item >= 0) {
        uint8_t temp = setup_state.mode.menu.menu_item;
        setup_state.mode.menu.menu_item = -1;
        setup_state.mode.menu.menu[temp]->func();
    }
}

static void HandleMidiChannelSetup()
{
    int8_t value = PickUpChangedEncoderValue(NUM_MIDI_CHANNELS);
    if (value >= 0) {
        midi_config_t *midi_config = &setup_state.mode.midi.config;
        midi_config->channels[setup_state.mode.midi.selected_voice] = value;
        LED_Driver_Write7SegNumberDec(value + 1, 1, 2, LED_Driver_RIGHT_ALIGN);
        if (midi_config->key_assignment_mode == KEY_ASSIGN_PARALLEL) {
            if (midi_config->channels[VOICE_1] == midi_config->channels[VOICE_2]) {
                GREEN_ENCODER_LED_OFF();
            } else {
                GREEN_ENCODER_LED_ON();
            }
        }
    }
}

static void ConfirmMidiChannelSetup()
{
    midi_config_t *midi_config = &setup_state.mode.midi.config;
    if (midi_config->key_assignment_mode == KEY_ASSIGN_PARALLEL) {
        if (midi_config->channels[VOICE_1] == midi_config->channels[VOICE_2]) {
            // Error, go back to the setup mode
            mode = MODE_MIDI_CHANNEL_SETUP;
            BlinkRed(100, 10);
            return;
        }
    } else {
        // channels of the both voices must be the same.
        enum Voice selected_voice = setup_state.mode.midi.selected_voice;
        midi_config->channels[(selected_voice + 1) % NUM_VOICES] = midi_config->channels[selected_voice];
    }

    CommitMidiConfigChange(midi_config);
    StartFinalization();
    mode = MODE_NORMAL;
}

static void HandleKeyAssignmentModeSetup()
{
    int8_t value = PickUpChangedEncoderValue(KEY_ASSIGN_END);
    if (value >= 0) {
        LED_Driver_WriteString7Seg(kKeyAssignmentModeName[value], 0);
        setup_state.mode.midi.config.key_assignment_mode = value;
    }
}

static void ConfirmKeyAssignmentMode()
{
    GREEN_ENCODER_LED_OFF();
    RED_ENCODER_LED_OFF();
    const midi_config_t *midi_config = &setup_state.mode.midi.config;
    if (midi_config->key_assignment_mode == KEY_ASSIGN_PARALLEL) {
        if (midi_config->channels[VOICE_1] == midi_config->channels[VOICE_2]) {
          InitiateMidiChannelSetup(midi_config, VOICE_2);
          return;
        }
    } else if (midi_config->channels[VOICE_1] != midi_config->channels[VOICE_2]) {
        InitiateMidiChannelSetup(midi_config, VOICE_1);
        return;
    }
    CommitMidiConfigChange(midi_config);
    StartFinalization();
    mode = MODE_NORMAL;
}

static void HandleGateTypeSetup()
{
    int8_t value = PickUpChangedEncoderValue(GATE_TYPE_END);
    if (value >= 0) {
        LED_Driver_WriteString7Seg(kGateTypeName[value], 0);
        setup_state.mode.gate_type = value;
    }
}

static void ConfirmGateType()
{
    GREEN_ENCODER_LED_OFF();
    RED_ENCODER_LED_OFF();
    if (UpdateGateType(setup_state.mode.gate_type)) {
        KeyAssigner_ConnectVoices();
    }
    StartFinalization();
    mode = MODE_NORMAL;
}

// Entry points ///////////////////////////////////////////////////////////////////////////////

/**
 * This method is called by the main loop when the program mode is not normal
 * to handle setup events. Every method should behave asynchronously.
 */
void HandleSettingModes()
{
    switch (mode) {
    case MODE_MENU_INVOKING:
        InvokeMenu();
        break;
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
    case MODE_GATE_TYPE_SETUP:
        HandleGateTypeSetup();
        break;
    case MODE_GATE_TYPE_CONFIRMED:
        ConfirmGateType();
        break;
    }
}

/**
 * This method is called by the switch input interrupt handler.
 */
void HandleSwitchEvent()
{
    switch (mode) {
    case MODE_NORMAL: {
        mode = MODE_MENU_INVOKING;
        break;
    }
    case MODE_MENU_SELECTING: {
        mode = MODE_MENU_SELECTED;
        break;
    }
    case MODE_KEY_ASSIGNMENT_SETUP:
        mode = MODE_KEY_ASSIGNMENT_CONFIRMED;
        break;
    case MODE_MIDI_CHANNEL_SETUP:
        mode = MODE_MIDI_CHANNEL_CONFIRMED;
        break;
    case MODE_GATE_TYPE_SETUP:
        mode = MODE_GATE_TYPE_CONFIRMED;
        break;
    case MODE_CALIBRATION_INIT:
        mode = MODE_CALIBRATION_BEND_WIDTH;
        break;
    case MODE_CALIBRATION_BEND_WIDTH:
        mode = MODE_NORMAL;
        break;
    }
}

// Utilities //////////////////////////////////////////////////////////////////////////////

int8_t PickUpChangedEncoderValue(uint8_t range)
{
    int16_t value = QuadDec_GetCounter();
    if (value == setup_state.prev_counter_value) {
        return -1;
    }
    setup_state.prev_counter_value = value;
    return value >= 0 ? value % range : (value + 1) % range + range - 1;
}

static void FinalizationBlinker()
{
    GREEN_ENCODER_LED_TOGGLE();
    if (--setup_state.mode.midi.blink_count == 0) {
        CySysTickStop();
        GREEN_ENCODER_LED_OFF();
        LED_Driver_ClearDisplayAll();
    }
}

static void InitSysTimer()
{
    CySysTickInit();
    CySysTickSetClockSource(CY_SYS_SYST_CSR_CLK_SRC_LFCLK);
    CySysTickSetReload(100000 / 1000 * 100); // 100ms
    CySysTickDisableInterrupt();
    CySysTickEnableInterrupt();
}

static void StartSysTimer()
{
    CySysTickClear();
    CySysTickEnable();
}

void StartFinalization()
{
    InitSysTimer();
    CySysTickSetCallback(0, FinalizationBlinker);
    setup_state.mode.midi.blink_count = 10;
    GREEN_ENCODER_LED_ON();
    RED_ENCODER_LED_OFF();
    StartSysTimer();
}

/* [] END OF FILE */

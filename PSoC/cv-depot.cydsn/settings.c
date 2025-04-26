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

static void InitiateKeyAssignSetup();
static void InitiateMidiChannelSetup1();
static void InitiateMidiChannelSetup2();
static void Diagnose();

// Menu items would change by the configuration. The menu is built on demand by the switch interrupt
// handler. It should be done quickly, so the menu items are kept in the static space.
static const menu_t kMenuSetChannel = { "ch ", InitiateMidiChannelSetup1 }; // for the both voices
static const menu_t kMenuSetChannel1 = { "ch1", InitiateMidiChannelSetup1 }; // for voice 1
static const menu_t kMenuSetChannel2 = { "ch2", InitiateMidiChannelSetup2 }; // for voice 2
static const menu_t kMenuSetKeyAssignment = { "asn", InitiateKeyAssignSetup };
static const menu_t kMenuCalibrate = { "cal", Calibrate }; // calibrate the bend center position
static const menu_t kMenuDiagnose = { "dgn", Diagnose }; // diagnose the hardware

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
    } mode;
};

static volatile struct setup_state setup_state = {};

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
    if (counter_value == setup_state.prev_counter_value) {
        return -1;
    }
    setup_state.prev_counter_value = counter_value;
    if (counter_value >= 0) {
        return counter_value % range;
    }
    return (counter_value + 1) % range + range - 1; 
}

// Settings event handlers ///////////////////////////////////////////////////

static void HandleMenuSelection()
{
    int8_t index = PickUpChangedEncoderValue(setup_state.mode.menu.menu_size);
    if (index >= 0) {
        LED_Driver_WriteString7Seg(setup_state.mode.menu.menu[index]->name, 0);
    }
}

static void ConfirmMenuSelection()
{
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
        volatile midi_config_t *midi_config = &setup_state.mode.midi.config;
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

static void StartRedBlinking();

static void ConfirmMidiChannelSetup()
{
    volatile midi_config_t *midi_config = &setup_state.mode.midi.config;
    if (midi_config->key_assignment_mode == KEY_ASSIGN_PARALLEL) {
        if (midi_config->channels[VOICE_1] == midi_config->channels[VOICE_2]) {
            // Error, go back to the setup mode
            mode = MODE_MIDI_CHANNEL_SETUP;
            StartRedBlinking();
            return;
        }
    } else {
        // channels of the both voices must be the same.
        enum Voice selected_voice = setup_state.mode.midi.selected_voice;
        midi_config->channels[(selected_voice + 1) % NUM_VOICES] = midi_config->channels[selected_voice];
    }
    GREEN_ENCODER_LED_OFF();
    RED_ENCODER_LED_OFF();

    CommitMidiConfigChange((const midi_config_t*) midi_config);
    LED_Driver_ClearDisplayAll();
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
    const midi_config_t *midi_config = (const midi_config_t*) &setup_state.mode.midi.config;
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
    LED_Driver_ClearDisplayAll();
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

/**
 * This method is called by the switch input interrupt handler.
 */
void HandleSwitchEvent()
{
    switch (mode) {
    case MODE_NORMAL: {
        BuildMenu();
        QuadDec_SetCounter(0);
        setup_state.prev_counter_value = - 1;
        mode = MODE_MENU_SELECTING;
        GREEN_ENCODER_LED_ON();
        break;
    }
    case MODE_MENU_SELECTING: {
        int16_t counter_value = QuadDec_GetCounter();
        setup_state.mode.menu.menu_item = counter_value % setup_state.mode.menu.menu_size;
        mode = MODE_MENU_SELECTED;
        GREEN_ENCODER_LED_OFF();
        QuadDec_SetCounter(0);
        setup_state.prev_counter_value = -1;
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

// Utilities //////////////////////////////////////////////////////////////////////////////

static void RedBlink()
{
    RED_ENCODER_LED_TOGGLE();
    if (--setup_state.mode.midi.blink_count == 0) {
        CySysTickStop();
    }
}

void StartRedBlinking() {
    CySysTickInit();
    CySysTickSetClockSource(CY_SYS_SYST_CSR_CLK_SRC_LFCLK);
    CySysTickSetReload(100000 / 1000 * 100); // 100ms
    CySysTickDisableInterrupt();
    CySysTickSetCallback(0, RedBlink);
    CySysTickEnableInterrupt();
    CySysTickClear();
    setup_state.mode.midi.blink_count = 10;
    RED_ENCODER_LED_ON();
    GREEN_ENCODER_LED_OFF();
    CySysTickEnable();
}

/* [] END OF FILE */

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
#include "project.h"

#include "eeprom.h"
#include "key_assigner.h"
#include "main.h"
#include "midi.h"
#include "pot.h"
#include "pot_change.h"
#include "settings.h"

// Interrupt handler declarations
CY_ISR_PROTO(SwitchHandler);

// Misc setup parameters
#define LED_Driver_BRIGHTNESS 70

volatile uint8_t mode = MODE_NORMAL;

uint16_t bend_offset;

#if 0
typedef struct menu {
    const char *name;
    void (*func)();
} menu_t;

static void SetKeyAssignment();
static void SetMidiChannel1();
static void SetMidiChannel2();
static void Diagnose();

volatile int8_t menu_item = -1;

// Menu items would change by the configuration. The menu is built on demand by the switch interrupt
// handler. It should be done quickly, so the menu items are kept in the static space.
static const menu_t kMenuSetKeyAssignment = { "asn", SetKeyAssignment };
static const menu_t kMenuSetChannel = { "ch ", SetMidiChannel1 }; // set MIDI channels for the both voices
static const menu_t kMenuSetChannel1 = { "ch1", SetMidiChannel1 }; // set MIDI channel for voice 1
static const menu_t kMenuSetChannel2 = { "ch2", SetMidiChannel2 }; // set MIDI channel for voice 2
static const menu_t kMenuCalibrate = { "cal", Calibrate }; // calibrate bend center position
static const menu_t kMenuDiagnose = { "dgn", Diagnose }; // diagnose hardware

#define MAX_MENU_SIZE 8
static volatile menu_t menu[MAX_MENU_SIZE];
static volatile uint8_t menu_size = 0;

static void BuildMenu()
{
    uint8_t i = 0;
    menu[i++] = kMenuSetKeyAssignment;
    if (midi_config.key_assignment_mode != KEY_ASSIGN_PARALLEL) {
        menu[i++] = kMenuSetChannel;
    } else {
        menu[i++] = kMenuSetChannel1;
        menu[i++] = kMenuSetChannel2;
    }
    menu[i++] = kMenuCalibrate;
    menu[i++] = kMenuDiagnose;
    menu_size = i;
}

// static uint8_t basic_midi_channel; // The decoder picks up only this channel

// Voice controller
// static uint8_t  voice_CurrentNote;
// static uint8_t  voices_NotesCount;
// static uint8_t  voices_Notes[MAX_NOTE_COUNT];

// other controller values
// Note that these values are not always equivalent to actual
// CV values since we apply anti-click mechanism to the CV values in
// transition to make the pitch change smooth.
// static int16_t ctrl_PitchBend;
// static uint8_t ctrl_PitchBend_updating;
uint16_t bend_offset;

#define VELOCITY_DAC_VALUE(velocity) (((velocity) * (velocity)) >> 3)
#define INDICATOR_VALUE(velocity) ((velocity) >= 64 ? (((velocity) - 63)  * ((velocity) - 63)) / 32 - 1 : 0)
#define NOTE_PWM_MAX_VALUE 120

static void SetKeyAssignment()
{
    mode = MODE_KEY_ASSIGNMENT_SETUP;
    GREEN_ENCODER_LED_ON();
    RED_ENCODER_LED_ON();
    QuadDec_SetCounter(midi_config.key_assignment_mode);
}

static void SetMidiChannel(uint8_t selected_voice)
{
    mode = MODE_MIDI_CHANNEL_SETUP;
    midi_config.selected_voice = selected_voice;
    GREEN_ENCODER_LED_ON();
    RED_ENCODER_LED_ON();
    int16_t encoder_value = midi_config.midi_channels[selected_voice];
    // QuadDec_SetCounter(encoder_value + 16384);
    QuadDec_SetCounter(encoder_value);
}

void SetMidiChannel1()
{
    SetMidiChannel(0);
}

void SetMidiChannel2()
{
    SetMidiChannel(1);
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

#endif

int main(void)
{
    CyGlobalIntEnable; /* Enable global interrupts. */

    // Initialization ////////////////////////////////////
    EEPROM_Start();
    PotGlobalInit();
    
    UART_Midi_Start();
    LED_Driver_Start();
    LED_Driver_SetBrightness(LED_Driver_BRIGHTNESS, 0);
    LED_Driver_SetBrightness(LED_Driver_BRIGHTNESS, 1);
    LED_Driver_SetBrightness(LED_Driver_BRIGHTNESS, 2);
    Pin_Portament_En_Write(0);
    Pin_Adj_En_Write(0);
    Pin_Adj_S0_Write(0);
    
    isr_SW_StartEx(SwitchHandler);
    QuadDec_Start();
    PWM_Notes_Start();
    PWM_Bend_Start();
    PWM_Indicators_Start();
    DVDAC_Velocity_1_Start();
    DVDAC_Velocity_2_Start();
    DVDAC_Expression_Start();
    DVDAC_Modulation_Start();
    
    InitMidiControllers();

    for (;;) {
        uint8_t status = UART_Midi_ReadRxStatus();
        if (status & UART_Midi_RX_STS_FIFO_NOTEMPTY) {
            uint8_t rx_byte = UART_Midi_ReadRxData();
            ConsumeMidiByte(rx_byte);
        }
        if (mode != MODE_NORMAL) {
            HandleSettingModes();
        }        
        // Consume pot change requests if not empty
        PotChangeHandleRequests();
    }
}

CY_ISR(SwitchHandler)
{
    HandleSwitchEvent();
}

/* [] END OF FILE */

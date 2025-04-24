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

// Interrupt handler declarations
CY_ISR_PROTO(SwitchHandler);

// Misc setup parameters
#define LED_Driver_BRIGHTNESS 70

volatile uint8_t mode = MODE_NORMAL;

typedef struct menu {
    const char *name;
    void (*func)();
} menu_t;

static void SetMidiChannel();
// static void Calibrate();
static void Diagnose();

static int8_t menu_item = -1;

static menu_t menus[] = {
    { "ch ", SetMidiChannel }, // set MIDI channel
    { "cal", Calibrate }, // calibrate bend center position
    { "dgn", Diagnose }, // diagnose hardware
    { NULL, NULL },
};

static uint8_t basic_midi_channel; // The decoder picks up only this channel

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

void SetMidiChannel()
{
    mode = MODE_MIDI_CHANNEL_SETUP;
    Pin_Encoder_LED_2_Write(1);
    int16_t encoder_value = basic_midi_channel;
    QuadDec_SetCounter(encoder_value + 16384);
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
        switch (mode) {
        case MODE_MENU_SELECTING: {
            int16_t counter_value = QuadDec_GetCounter();
            uint8_t temp = counter_value % (sizeof(menus) / sizeof(menu_t) - 1);
            LED_Driver_WriteString7Seg(menus[temp].name, 0);
            break;
        }
        case MODE_MENU_SELECTED: {
            if (menu_item >= 0) {
                uint8_t temp = menu_item;
                menu_item = -1;
                menus[temp].func();
            }
            break;
        }
        case MODE_MIDI_CHANNEL_SETUP: {
            int16_t counter_value = QuadDec_GetCounter();
            basic_midi_channel = counter_value % 16;
            LED_Driver_Write7SegNumberDec(basic_midi_channel + 1, 1, 2, LED_Driver_RIGHT_ALIGN);
            break;
        }
        case MODE_MIDI_CHANNEL_CONFIRMED:
            Pin_Encoder_LED_2_Write(0);
            EEPROM_UpdateTemperature();
            EEPROM_WriteByte(basic_midi_channel, ADDR_MIDI_CH);
            LED_Driver_ClearDisplayAll();
            mode = MODE_NORMAL;
            break;
        }
        
        // Consume pot change requests if not empty
        PotChangeHandleRequests();
    }
}

CY_ISR(SwitchHandler)
{
    switch (mode) {
    case MODE_NORMAL:
        QuadDec_SetCounter(0);
        mode = MODE_MENU_SELECTING;
        Pin_Encoder_LED_1_Write(1);
        break;
    case MODE_MENU_SELECTING: {
        int16_t counter_value = QuadDec_GetCounter();
        menu_item = counter_value % (sizeof(menus) / sizeof(menu_t) - 1);
        mode = MODE_MENU_SELECTED;
        Pin_Encoder_LED_1_Write(0);
        break;
    }
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

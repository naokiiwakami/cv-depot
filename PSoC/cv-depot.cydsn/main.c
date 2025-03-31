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

// Interrupt handler declarations
CY_ISR_PROTO(SwitchHandler);

#define MODE_NORMAL 0
#define MODE_MENU_SELECTING 1
#define MODE_MENU_SELECTED 2
#define MODE_MIDI_CHANNEL_SETUP 3
#define MODE_MIDI_CHANNEL_CONFIRMED 4

static uint8_t mode = MODE_NORMAL;

typedef struct menu {
    const char *name;
    void (*func)();
} menu_t;

static void Diagnose();
static void SetMidiChannel();

static int8_t menu_item = -1;

static menu_t menus[] = {
    { "ch ", SetMidiChannel },
    { "dgn", Diagnose },
    { NULL, NULL },
};

/*---------------------------------------------------------*/
/* MIDI decoder                                            */
/*---------------------------------------------------------*/

/* MIDI Channel Voice message */
#define MSG_NOTE_OFF          0x80
#define MSG_NOTE_ON           0x90
#define MSG_POLY_KEY_PRESSURE 0xA0
#define MSG_CONTROL_CHANGE    0xB0
#define MSG_PROGRAM_CHANGE    0xC0
#define MSG_CHANNEL_PRESSURE  0xD0
#define MSG_PITCH_BEND        0xE0

/* MIDI Control Changes */
#define CC_BREATH                0x02
#define CC_EXPRESSION            0x0B
#define CC_DAMPER_PEDAL          0x40

/* MIDI Channel Mode Messages */
#define CC_ALL_SOUND_OFF         0x78
#define CC_RESET_ALL_CONTROLLERS 0x79
#define CC_LOCAL_CONTROL         0x7A
#define CC_ALL_NOTES_OFF         0x7B
#define CC_OMNI_MODE_OFF         0x7C
#define CC_OMNI_MODE_ON          0x7D
#define CC_MONO_MODE_ON          0x7E
#define CC_POLY_MODE_ON          0x7F

/* MIDI System Messages */
#define SYSEX_IN  0xF0
#define SYSEX_OUT 0xF7

/* MIDI System Real-Time Messages */
#define TIMINIG_CLOCK  0xf8  // The lowest number in the real-time messages

#define DATA_MAX_VALUE 0x7F

/* Notes */
#define C0 0x0B

/* Macros used by MIDI decoder */
#define IsSystemExclusive(status)     ((status) == SYSEX_IN)
#define IsChannelStatus(status)       ((status) < SYSEX_IN)
#define RetrieveMessageFromStatus(status)  ((status) & 0xF0)
#define RetrieveChannelFromStatus(status)  ((status) & 0x0F)

volatile uint8_t midi_status;      // MIDI status
volatile uint8_t midi_message;     // MIDI message (= status & 0xF0)
volatile uint8_t midi_channel;     // MIDI channel (= status & 0x0F)

volatile uint8_t target_midi_channel; // The decoder picks up only this channel

volatile uint8_t midi_data[2];     // MIDI data buffer
volatile uint8_t midi_data_position;    // MIDI data buffer pointer
volatile uint8_t midi_data_length; // Expected MIDI data length

/*---------------------------------------------------------*/
/* Controllers                                             */
/*---------------------------------------------------------*/
#define MAX_NOTE_COUNT 8

// Voice controller
volatile uint8_t  voice_CurrentNote;
volatile uint8_t  voices_NotesCount;
volatile uint8_t  voices_Notes[MAX_NOTE_COUNT];

// other controller values
// Note that these values are not always equivalent to actual
// CV values since we apply anti-click mechanism to the CV values in
// transition to make the pitch change smooth.
volatile uint16_t ctrl_PitchBend;
volatile uint8_t ctrl_PitchBend_updating;

void InitMidiControllers()
{
    // ctrl_PitchBend = 8192 >> 4; // 0x20 0x00
    ctrl_PitchBend_updating = 0;
    // pitch_bend(0x00, 0x40);  // set neutral
    
    voices_NotesCount = 0;
    voice_CurrentNote = 68;  // A4
    // set_cv();

    target_midi_channel = 0;

    // Gates
    Pin_Gate_1_Write(0);
    Pin_Gate_2_Write(0);
    
    // Portament
    Pin_Portament_En_Write(0);
}

void NoteOff(uint8_t note_number)
{
    LED_Driver_1_PutChar7Seg('F', 0);
    LED_Driver_1_Write7SegNumberHex(note_number, 1, 2, LED_Driver_1_RIGHT_ALIGN);
    DVDAC_Velocity_1_SetValue(2040);
    Pin_Gate_1_Write(0);
}

void NoteOn(uint8_t note_number, uint8_t velocity)
{
    // note on with zero velocity means note off
    if (velocity == 0) {
        NoteOff(note_number);
        return;
    }
    // LED_Driver_1_Write7SegNumberDec(velocity, 0, 3, LED_Driver_1_RIGHT_ALIGN);
    LED_Driver_1_PutChar7Seg('N', 0);
    LED_Driver_1_Write7SegNumberHex(note_number, 1, 2, LED_Driver_1_RIGHT_ALIGN);
    PWM_Notes_WriteCompare1(note_number);
    PWM_Indicators_WriteCompare1(velocity);
    DVDAC_Velocity_1_SetValue(2040 - velocity * 16);
    // PWM_Notes_WriteCompare2(255 - velocity * 2);
    Pin_Gate_1_Write(1);
}

void ControlChange(uint8_t control_number, uint8_t value)
{
}

void PitchBend(uint8_t lsb, uint8_t msb)
{
}

void HandleMidiChannelMessage()
{
    // do nothing for channels that are out of scope
    if (midi_channel != target_midi_channel) {
        return;
    }

    switch(midi_message) {
    case MSG_NOTE_OFF:
        NoteOff(midi_data[0]);
        break;
    case MSG_NOTE_ON:
        NoteOn(midi_data[0], midi_data[1]);
        break;
    case MSG_CONTROL_CHANGE:
        ControlChange(midi_data[0], midi_data[1]);
        break;
    case MSG_PITCH_BEND:
        PitchBend(midi_data[0], midi_data[1]);
        break;
    default:
        // do nothing for unsupported channel messages
        break;
    }
}

static void ConsumeMidiByte(uint16_t rx_byte)
{
    if (rx_byte >= TIMINIG_CLOCK) {
      // Ignore system real-time messages (yet)
      return;
    }
  
    // Ignore system messages
    if (IsSystemExclusive(midi_status)) {
        if (rx_byte == SYSEX_OUT) {
            midi_status = SYSEX_OUT;
        }
        return;
    }
    
    if (rx_byte > SYSEX_IN) {
        midi_status = rx_byte;
        midi_data_length = 1; // Just read and trash data byte
        midi_data_position = 0;
        return;
    }

    if (rx_byte == SYSEX_IN) {
        midi_status = SYSEX_IN;
        return;
    }

    if (rx_byte > DATA_MAX_VALUE) {
        // This is a status byte of a channel message
        midi_status = rx_byte;
        midi_channel = RetrieveChannelFromStatus(midi_status);
        midi_message = RetrieveMessageFromStatus(midi_status);

        // set the data length according to the message type
        switch(midi_message) {
        case MSG_NOTE_OFF:
        case MSG_NOTE_ON:
        case MSG_POLY_KEY_PRESSURE:
        case MSG_CONTROL_CHANGE:
        case MSG_PITCH_BEND:
            midi_data_length = 2;
            break;
        case MSG_PROGRAM_CHANGE:
        case MSG_CHANNEL_PRESSURE:
            midi_data_length = 1;
            break;
        }
        
        // Reset the data index
        midi_data_position = 0;
        return;
    }

    // The byte is data if we reach here
    midi_data[midi_data_position++] = rx_byte;
    if (midi_data_position ==  midi_data_length) {
        midi_data_position = 0;

        // We handle the data
        if (IsChannelStatus(midi_status)) {
            HandleMidiChannelMessage();
        }
    }
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
        LED_Driver_1_Write7SegDigitHex(value, 0);
        LED_Driver_1_Write7SegDigitHex(value, 1);
        LED_Driver_1_Write7SegDigitHex(value, 2);
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
    LED_Driver_1_ClearDisplayAll();
}

void SetMidiChannel() {
    Pin_Encoder_LED_2_Write(1);
    int16_t encoder_value = target_midi_channel;
    QuadDec_SetCounter(encoder_value + 16384);
    mode = MODE_MIDI_CHANNEL_SETUP;
}

int main(void)
{
    CyGlobalIntEnable; /* Enable global interrupts. */

    // Initialization ////////////////////////////////////
    InitMidiControllers();
    UART_Midi_Start();
    LED_Driver_1_Start();
    LED_Driver_1_SetBrightness(70, 0);
    LED_Driver_1_SetBrightness(70, 1);
    LED_Driver_1_SetBrightness(70, 2);
    Pin_Portament_En_Write(0);
    Pin_LED_Write(1);
    Pin_Resistor_Select_Note_1_Write(1);
    Pin_Resistor_Select_Note_2_Write(1);
    Pin_Resistor_Select_Portament_1_Write(1);
    Pin_Resistor_Select_Portament_2_Write(1);
    
    isr_SW_StartEx(SwitchHandler);
    QuadDec_Start();
    PWM_Notes_Start();
    PWM_Bend_Start();
    PWM_Indicators_Start();
    DVDAC_Velocity_1_Start();
    DVDAC_Velocity_2_Start();
    DVDAC_Expression_Start();
    DVDAC_Modulation_Start();

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
            LED_Driver_1_WriteString7Seg(menus[temp].name, 0);
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
            target_midi_channel = counter_value % 16;
            LED_Driver_1_Write7SegNumberDec(target_midi_channel + 1, 1, 2, LED_Driver_1_RIGHT_ALIGN);
            break;
        }
        case MODE_MIDI_CHANNEL_CONFIRMED:
            LED_Driver_1_ClearDisplayAll();
            mode = MODE_NORMAL;
            break;
        }
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
       Pin_Encoder_LED_2_Write(0);
       mode = MODE_MIDI_CHANNEL_CONFIRMED;
       break;
    }
}

/* [] END OF FILE */

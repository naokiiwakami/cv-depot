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

static int32_t current = 101;
static uint8_t encoder_value = 0;

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
volatile uint8_t midi_data_ptr;    // MIDI data buffer pointer
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

    // Set target MIDI channel.  If the button is on, we read the channel from the octave switch.
    // Otherwise the channel is 1 (but the internal value is zero based).
    target_midi_channel = 0;

    // GATE1_OFF();
    Pin_Gate_1_Write(1);
}

void NoteOff(uint8_t note_number)
{
    LED_Driver_1_PutChar7Seg('F', 0);
    LED_Driver_1_Write7SegNumberHex(note_number, 1, 2, LED_Driver_1_RIGHT_ALIGN);
    DVDAC_Velocity_1_SetValue(2040);
    Pin_Gate_1_Write(1);
}

void NoteOn(uint8_t note_number, uint8_t velocity)
{
    // note on with zero velocity means note off
    if (velocity == 0) {
        NoteOff(note_number);
        return;
    }
    LED_Driver_1_Write7SegNumberDec(velocity, 0, 3, LED_Driver_1_RIGHT_ALIGN);
    /*
    LED_Driver_1_PutChar7Seg('N', 0);
    LED_Driver_1_Write7SegNumberHex(note_number, 1, 2, LED_Driver_1_RIGHT_ALIGN);
    */
    PWM_Notes_WriteCompare1(note_number);
    PWM_Indicators_WriteCompare1(velocity);
    DVDAC_Velocity_1_SetValue(2040 - velocity * 16);
    // PWM_Notes_WriteCompare2(255 - velocity * 2);
    Pin_Gate_1_Write(0);
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
      // we break here to avoid the system real-time message affects the midi_status.
      return;
    }
  
    if (IsSystemExclusive(midi_status)) {
        if (rx_byte == SYSEX_OUT) {
            midi_status = SYSEX_OUT;
        }
        return; // ignore any sysex data
    }

    if (rx_byte > SYSEX_IN) {
        midi_status = rx_byte;
        midi_data_length = 1; // We don't really parse system, skip every byte
        midi_data_ptr = 0;
        return;
    }

    if (rx_byte == SYSEX_IN) {
        midi_status = SYSEX_IN;
        return;
    }

    if (rx_byte > DATA_MAX_VALUE) {
        // it's a new message
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
        midi_data_ptr = 0;
        return;
    }

    // The byte is data if we reach here
    midi_data[midi_data_ptr++] = rx_byte;
    if (midi_data_ptr ==  midi_data_length) {
        midi_data_ptr = 0;

        // We handle the data
        if (IsChannelStatus(midi_status)) {
            HandleMidiChannelMessage();
        }
    }
}

int main(void)
{
    CyGlobalIntEnable; /* Enable global interrupts. */

    // Initialization ////////////////////////////////////
    InitMidiControllers();
    UART_Midi_Start();
    LED_Driver_1_Start();
    LED_Driver_1_Write7SegNumberDec(current, 0, 3, LED_Driver_1_RIGHT_ALIGN);
    Pin_Portament_En_Write(0);
    isr_SW_StartEx(SwitchHandler);
    QuadDec_Start();
    Pin_LED_Write(1);
    PWM_Notes_Start();
    PWM_Bend_Start();
    PWM_Indicators_Start();
    DVDAC_Velocity_1_Start();
    DVDAC_Velocity_2_Start();
    DVDAC_Expression_Start();
    DVDAC_Modulation_Start();

    for(;;)
    {
        uint8_t status = UART_Midi_ReadRxStatus();
        if (status & UART_Midi_RX_STS_FIFO_NOTEMPTY) {
            uint8_t rx_byte = UART_Midi_ReadRxData();
            ConsumeMidiByte(rx_byte);
        }
        int16_t counter_value = QuadDec_GetCounter();
        if (counter_value <= 0) {
            QuadDec_SetCounter(1);
            counter_value = 1;
        }
        /*
        if (counter_value > 60) {
            counter_value = 0;
            QuadDec_SetCounter(counter_value);
        }
        */
        if (counter_value >= 120) {
            counter_value = 120;
            QuadDec_SetCounter(counter_value);
        }
        status = counter_value;
        if (status != encoder_value) {
            LED_Driver_1_Write7SegNumberDec(status, 0, 3, LED_Driver_1_RIGHT_ALIGN);
            PWM_Notes_WriteCompare1(status);
            // PWM_Notes_WriteCompare2(120 - status);
            // PWM_Indicators_WriteCompare1(status);
            encoder_value = status;
            DVDAC_Velocity_1_SetValue(status * 17);
        }
        
        // Pin_LED_Write(Pin_Adjustment_In_Read() ^ 0x1);
    }
}

CY_ISR(SwitchHandler)
{
    // current += 101;
    // LED_Driver_1_Write7SegNumberDec(current, 0, 3, LED_Driver_1_RIGHT_ALIGN);
    uint8_t gate_level = Pin_LED_Read() ^ 0x1;
    Pin_LED_Write(gate_level);
    // Gate 1 adjustment
    Pin_Gate_1_Write(gate_level);
    int16_t velocity;
    if (gate_level) {
        velocity = QuadDec_GetCounter() * 17;
    } else {
        velocity = 0;
    }
    DVDAC_Velocity_1_SetValue(velocity);
    LED_Driver_1_Write7SegNumberHex(velocity, 0, 3, LED_Driver_1_RIGHT_ALIGN);
    // Pin_Portament_En_Write(Pin_Portament_En_Read() ^ 1);
/*
    int16_t counter_value = QuadDec_GetCounter();
    int8_t status = counter_value + 24;
    PWM_Notes_WriteCompare1(status);
    LED_Driver_1_Write7SegNumberDec(status, 0, 3, LED_Driver_1_RIGHT_ALIGN);
*/
    // Pin_Gate_2_Write(0);
}

/* [] END OF FILE */

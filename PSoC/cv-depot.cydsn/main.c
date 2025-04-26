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

#define VELOCITY_DAC_VALUE(velocity) (((velocity) * (velocity)) >> 3)
#define INDICATOR_VALUE(velocity) ((velocity) >= 64 ? (((velocity) - 63)  * ((velocity) - 63)) / 32 - 1 : 0)
#define NOTE_PWM_MAX_VALUE 120

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

    // The main loop ////////////////////////////////////
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

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
#include "hardware.h"

// Interrupt handler declarations
CY_ISR_PROTO(SwitchHandler);

// Misc setup parameters
#define LED_Driver_BRIGHTNESS 70

volatile uint8_t mode = MODE_NORMAL;

int main(void)
{
    // Initialization ////////////////////////////////////
    EEPROM_Start();
    PotGlobalInit();
    
    CAN_Start();
    UART_Midi_Start();
    LED_Driver_Start();
    LED_Driver_SetBrightness(LED_Driver_BRIGHTNESS, 0);
    LED_Driver_SetBrightness(LED_Driver_BRIGHTNESS, 1);
    LED_Driver_SetBrightness(LED_Driver_BRIGHTNESS, 2);
    isr_SW_StartEx(SwitchHandler);
    QuadDec_Start();
    
    InitializeVoiceControl();
    KeyAssigner_ConnectVoices();
    InitializeMidiControllers();

    CyGlobalIntEnable; /* Enable global interrupts. */

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

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
#include <stdlib.h>

#include "project.h"

#include "analog3.h"
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
uint32_t module_uid;
uint16_t module_id = 0xffff;
volatile uint8_t has_control_center_message = 0;

static void InitializeModule()
{
    module_uid = Load32(ADDR_MODULE_UID);
    if (module_uid == 0) {
        module_uid = rand() & 0x1fffffff;
        Save32(module_uid, ADDR_MODULE_UID);
    }
}

static void RequestForId()
{
    CAN_DATA_BYTES_MSG data;
    data.byte[0] = A3_ADMIN_REQUEST_ID;
    A3SendDataExtended(module_uid, 1, &data);
}

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
    
    InitializeModule();
    InitializeVoiceControl();
    KeyAssigner_ConnectVoices();
    InitializeMidiControllers();

    CyGlobalIntEnable; /* Enable global interrupts. */
    
    Pin_LED_Write(1);
    RequestForId();
    
    // TODO: set timeout
    while (module_id > 0x7ff) {
        while (!has_control_center_message) {}
        has_control_center_message = 0;
        uint8_t opcode = CAN_RX_DATA_BYTE(0, 0);
        if (opcode == A3_MC_ASSIGN_MODULE_ID) {
            uint32_t target_module
                = CAN_RX_DATA_BYTE(0, 1) << 24
                | CAN_RX_DATA_BYTE(0, 2) << 16
                | CAN_RX_DATA_BYTE(0, 3) << 8
                | CAN_RX_DATA_BYTE(0, 4);
            if (target_module == module_uid) {
                module_id = A3_ID_INDIVIDUAL_MODULE_BASE + CAN_RX_DATA_BYTE(0, 5);
            }
        }
    }
    Pin_LED_Write(0);

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

// Interrupt handlers ////////////////////////////////

CY_ISR(SwitchHandler)
{
    HandleSwitchEvent();
}

#ifdef CAN_MSG_RX_ISR_CALLBACK
void CAN_MsgRXIsr_Callback()
{
    Pin_LED_Write(~Pin_LED_Read());
}
#endif

void CAN_ReceiveMsg_0_Callback()
{
    has_control_center_message = 1;
}

/* [] END OF FILE */

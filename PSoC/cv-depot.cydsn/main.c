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
#include <malloc.h>

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

// Task management
#define MAX_TASKS 8
task_t pending_tasks[MAX_TASKS];
volatile uint8_t task_first;
volatile uint8_t task_last;
volatile uint8_t tasks_overflow;

static void ClearTasks()
{
    task_first = 0;
    task_last = 0;
    tasks_overflow = 0;
}

void ScheduleTask(task_t task)
{
    if (tasks_overflow) {
        // no way to notify, silently ignore
        if (task.arg != NULL) {
            free(task.arg);
        }
        return;
    }
    pending_tasks[task_last] = task;
    task_last = (task_last + 1) % MAX_TASKS;
    if (task_last == task_first) {
        tasks_overflow = 1;
    }
}

void ConsumeTask()
{
    CyGlobalIntDisable;  // enter critical section
    if (task_last == task_first && !tasks_overflow) {
        // nothing to pick up
        CyGlobalIntEnable; // exit critical section
        return;
    }
    task_t task = pending_tasks[task_first];
    task_first = (task_first + 1) % MAX_TASKS;
    tasks_overflow = 0;
    CyGlobalIntEnable; // exit critical section
    task.run(task.arg);
}

int main(void)
{
    // Initialization ////////////////////////////////////
    ClearTasks();
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
    
    InitializeA3Module();
    InitializeVoiceControl();
    KeyAssigner_ConnectVoices();
    InitializeMidiControllers();

    CyGlobalIntEnable; /* Enable global interrupts. */
    
    Pin_LED_Write(1);
    SignIn();
    
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
        
        // Consume task if any, one at a time
        ConsumeTask();
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
    task_t task = {
        .run = HandleMissionControlMessage,
        .arg = NULL,
    };
    ScheduleTask(task);
}

/* [] END OF FILE */

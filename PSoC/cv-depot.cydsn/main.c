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
#include <malloc.h>
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

    RED_ENCODER_LED_ON();
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
    if (q_full) {
        // we can't do anything
        return;
    }
    can_message_t *message = &message_queue[q_head];
    q_head = (q_head + 1) % MESSAGE_QUEUE_SIZE;
    message->id = CAN_GET_RX_ID(MAILBOX_MC);
    message->extended = CAN_GET_RX_IDE(MAILBOX_MC);
    message->dlc = CAN_GET_DLC(MAILBOX_MC);
    for (uint32_t i = 0; i < message->dlc; ++i) {
        message->data[i] = CAN_RX_DATA_BYTE(MAILBOX_MC, i);
    }
    task_t task = {
        .run = HandleMissionControlMessage,
        .arg = message,
    };
    ScheduleTask(task);
}

void CAN_ReceiveMsg_Callback()
{
    if (q_full) {
        // we can't do anything
        return;
    }
    can_message_t *message = &message_queue[q_head];
    q_head = (q_head + 1) % MESSAGE_QUEUE_SIZE;
    message->id = CAN_GET_RX_ID(MAILBOX_OTHERS);
    message->extended = CAN_GET_RX_IDE(MAILBOX_OTHERS);
    message->dlc = CAN_GET_DLC(MAILBOX_OTHERS);
    for (uint32_t i = 0; i < message->dlc; ++i) {
        message->data[i] = CAN_RX_DATA_BYTE(MAILBOX_OTHERS, i);
    }
    task_t task = {
        .run = HandleGeneralMessage,
        .arg = message,
    };
    ScheduleTask(task);
}
/* [] END OF FILE */

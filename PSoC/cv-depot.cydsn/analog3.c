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
#include <stdlib.h>

#include "analog3.h"
#include "eeprom.h"
#include "hardware.h"

#define A3_ID_UNASSIGNED 0

uint32_t a3_module_uid;
uint16_t a3_module_id;

void A3SendDataStandard(uint32_t id, uint8_t dlc, CAN_DATA_BYTES_MSG *data)
{
    CAN_TX_MSG message = {
        .id = id,
        .rtr = 0,
        .ide = 0,
        .dlc = dlc,
        .irq = 0,
        .msg = data,
    };
    CAN_SendMsg(&message);
}

void A3SendDataExtended(uint32_t id, uint8_t dlc, CAN_DATA_BYTES_MSG *data)
{
    CAN_TX_MSG message = {
        .id = id,
        .rtr = 0,
        .ide = 1,
        .dlc = dlc,
        .irq = 0,
        .msg = data,
    };
    CAN_SendMsg(&message);
}

void InitializeA3Module()
{
    a3_module_uid = Load32(ADDR_MODULE_UID);
    if (a3_module_uid == 0) {
        a3_module_uid = rand() & 0x1fffffff;
        Save32(a3_module_uid, ADDR_MODULE_UID);
    }
    a3_module_id = A3_ID_UNASSIGNED;
}

static void CancelUid()
{
    a3_module_uid = rand() & 0x1fffffff;
    Save32(a3_module_uid, ADDR_MODULE_UID);
    a3_module_id = A3_ID_UNASSIGNED;
    SignIn();
}

void SignIn()
{
    CAN_DATA_BYTES_MSG data;
    data.byte[0] = A3_ADMIN_SIGN_IN;
    A3SendDataExtended(a3_module_uid, 1, &data);
}

static void NotifyId()
{
    CAN_DATA_BYTES_MSG data;
    data.byte[0] = A3_ADMIN_NOTIFY_ID;
    data.byte[1] = a3_module_id - A3_ID_IM_BASE;
    A3SendDataExtended(a3_module_uid, 2, &data);
}

static void RequestUidCancel(uint32_t id)
{
    CAN_DATA_BYTES_MSG data;
    data.byte[0] = A3_ADMIN_REQ_UID_CANCEL;
    A3SendDataExtended(id, 1, &data);
}

static const char *module_name = "cv-depot; a long time ago in a galaxy far, far away...";
static int data_position = 0;

static void HandleRequestName()
{
    data_position = 0;
    int payload_index = 0;
    CAN_DATA_BYTES_MSG data;
    data.byte[payload_index++] = A3_IM_NAME_REPLY;
    data.byte[payload_index++] = A3_ATTR_NAME;
    uint8_t name_length = strlen(module_name);
    data.byte[payload_index++] = name_length;
    for (; payload_index < A3_DATA_LENGTH && data_position < name_length; ++payload_index, ++data_position) {
        data.byte[payload_index] = module_name[data_position];
    }
    CyDelay(10);
    A3SendDataStandard(a3_module_id, payload_index, &data);
}

static void HandleContinueName()
{
    int payload_index = 0;
    CAN_DATA_BYTES_MSG data;
    data.byte[payload_index++] = A3_IM_NAME_REPLY;
    uint8_t name_length = strlen(module_name);
    for (; payload_index < A3_DATA_LENGTH && data_position < name_length; ++payload_index, ++data_position) {
        data.byte[payload_index] = module_name[data_position];
    }
    CyDelay(10);
    A3SendDataStandard(a3_module_id, payload_index, &data);
}

void HandleMissionControlMessage(void* arg)
{
    (void)arg;
    uint8_t opcode = CAN_RX_DATA_BYTE(0, 0);
    switch (opcode) {
    case A3_MC_SIGN_IN: {
        if (a3_module_id == A3_ID_UNASSIGNED) {
            SignIn();
        } else {
            NotifyId();
        }
    }
    case  A3_MC_ASSIGN_MODULE_ID: {
        uint32_t target_module
            = CAN_RX_DATA_BYTE(0, 1) << 24
            | CAN_RX_DATA_BYTE(0, 2) << 16
            | CAN_RX_DATA_BYTE(0, 3) << 8
            | CAN_RX_DATA_BYTE(0, 4);
        if (target_module == a3_module_uid) {
            a3_module_id = CAN_RX_DATA_BYTE(0, 5) + A3_ID_IM_BASE;
        }
        Pin_LED_Write(0);
        break;
    }
    case A3_MC_PING: {
        uint16_t target_module = CAN_RX_DATA_BYTE(0, 1) + A3_ID_IM_BASE;
        if (target_module == a3_module_id) {
            CAN_DATA_BYTES_MSG data;
            data.byte[0] = A3_IM_PING_REPLY;
            A3SendDataStandard(a3_module_id, 1, &data);
            if (CAN_GET_DLC(0) >= 3 && CAN_RX_DATA_BYTE(0, 2)) {
                BlinkGreen(70, 3);
            }
        }
        break;
    }
    case A3_MC_REQUEST_NAME: {
        uint16_t target_module = CAN_RX_DATA_BYTE(0, 1) + A3_ID_IM_BASE;
        if (target_module == a3_module_id) {
            HandleRequestName();
        }
        break;
    }
    case A3_MC_CONTINUE_NAME: {
        uint16_t target_module = CAN_RX_DATA_BYTE(0, 1) + A3_ID_IM_BASE;
        if (target_module == a3_module_id) {
            HandleContinueName();
        }
        break;
    }
    }
}

void HandleGeneralMessage(void *arg)
{
    (void)arg;
    uint8_t is_extended = CAN_GET_RX_IDE(1);
    if (!is_extended) {
        // no jobs yet in the standard ID space
        return;
    }
    uint32_t id = CAN_GET_RX_ID(1);
    if (id != a3_module_uid) {
        // it's not about me
        return;
    }
    uint8_t opcode = CAN_RX_DATA_BYTE(1, 0);
    switch (opcode) {
    case A3_ADMIN_SIGN_IN:
    case A3_ADMIN_NOTIFY_ID:
        RequestUidCancel(id);
        // cancel mine, too
        CancelUid();
        break;
    case  A3_ADMIN_REQ_UID_CANCEL:
        CancelUid();
        break;
    }
}

/* [] END OF FILE */

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
#include "config.h"
#include "eeprom.h"
#include "hardware.h"
#include "key_assigner.h"
#include "midi.h"

#define A3_ID_UNASSIGNED 0

// module identifiers
uint32_t a3_module_uid;
uint16_t a3_module_id;

static uint8_t a3_module_type = MODULE_TYPE;
static uint8_t num_voices = NUM_VOICES;

// communication states
static char module_name[64];
static int prop_position = 0;
static int data_position = 0;

static uint32_t wire_id = 0;

static a3_vector_t channels = {
    .size = NUM_VOICES,
    .data = &midi_config.channels,
};

static a3_module_property_t config[NUM_PROPS] = {
    {
        .id = PROP_MODULE_UID,
        .value_type = TYPE_MODULE_UID,
        .data = &a3_module_uid,
    }, {
        .id = PROP_MODULE_TYPE,
        .value_type = TYPE_MODULE_TYPE,
        .data = &a3_module_type,
    }, {
        .id = PROP_MODULE_NAME,
        .value_type = TYPE_MODULE_NAME,
        .data = module_name,
    }, {
        .id = PROP_NUM_VOICES,
        .value_type = TYPE_NUM_VOICES,
        .data = &num_voices,
    }, {
        .id = PROP_KEY_ASSIGNMENT_MODE,
        .value_type = TYPE_KEY_ASSIGNMENT_MODE,
        .data = &midi_config.key_assignment_mode,
    }, {
        .id = PROP_KEY_PRIORITY,
        .value_type = TYPE_KEY_PRIORITY,
        .data = &midi_config.key_priority,
    }, {
        .id = PROP_MIDI_CHANNELS,
        .value_type = TYPE_MIDI_CHANNELS,
        .data = &channels,
    }, 
};

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
    strcpy(module_name, "cv-depot; a long time ago in a galaxy far, far away...");
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

static void HandleRequestName()
{
    wire_id = CAN_RX_DATA_BYTE(0, 2) + A3_ID_ADMIN_WIRES_BASE;
    data_position = 0;
    int payload_index = 0;
    CAN_DATA_BYTES_MSG data;
    data.byte[payload_index++] = A3_ATTR_NAME;
    uint8_t name_length = strlen(module_name);
    data.byte[payload_index++] = name_length;
    for (; payload_index < A3_DATA_LENGTH && data_position < name_length; ++payload_index, ++data_position) {
        data.byte[payload_index] = module_name[data_position];
    }
    A3SendDataStandard(wire_id, payload_index, &data);
}

static void HandleContinueName()
{
    int payload_index = 0;
    CAN_DATA_BYTES_MSG data;
    uint8_t name_length = strlen(module_name);
    for (; payload_index < A3_DATA_LENGTH && data_position < name_length; ++payload_index, ++data_position) {
        data.byte[payload_index] = module_name[data_position];
    }
    A3SendDataStandard(wire_id, payload_index, &data);
}

static int FillU8(a3_module_property_t *prop, CAN_DATA_BYTES_MSG *data, int payload_index)
{
    if (data_position < 2) {
        data->byte[payload_index++] = 1;
        ++data_position;
        if (payload_index == A3_DATA_LENGTH) {
            return payload_index;
        }
    }
    data->byte[payload_index++] = *(uint8_t *)prop->data;
    data_position = 0;
    ++prop_position;
    return payload_index;
}

#define MIN(x, y) ((x) < (y) ? (x) : (y))

static int FillU16(a3_module_property_t *prop, CAN_DATA_BYTES_MSG *data, int payload_index)
{
    if (data_position < 2) {
        data->byte[payload_index++] = 2;
        ++data_position;
        if (payload_index == A3_DATA_LENGTH) {
            return payload_index;
        }
    }
    uint32_t value = *(uint32_t *)prop->data;
    int data_size = MIN(A3_DATA_LENGTH - payload_index, 4 - data_position);
    value >>= (4 - data_position - data_size) * 8; 
    for (int i = 0; i < data_size; ++i) {
        data->byte[payload_index + data_size - i - 1] = value & 0xff;
        value >>= 8;
    }
    data_position += data_size;
    payload_index += data_size;
    if (data_position == 4) {
        ++prop_position;
        data_position = 0;
    }
    return payload_index;
}

static int FillU32(a3_module_property_t *prop, CAN_DATA_BYTES_MSG *data, int payload_index)
{
    if (data_position < 2) {
        data->byte[payload_index++] = 4;
        ++data_position;
        if (payload_index == A3_DATA_LENGTH) {
            return payload_index;
        }
    }
    uint32_t value = *(uint32_t *)prop->data;
    int data_size = MIN(A3_DATA_LENGTH - payload_index, 6 - data_position);
    value >>= (6 - data_position - data_size) * 8; 
    for (int i = 0; i < data_size; ++i) {
        data->byte[payload_index + data_size - i - 1] = value & 0xff;
        value >>= 8;
    }
    data_position += data_size;
    payload_index += data_size;
    if (data_position == 6) {
        ++prop_position;
        data_position = 0;
    }
    return payload_index;
}

static int FillVectorU8(a3_module_property_t *prop, CAN_DATA_BYTES_MSG *data, int payload_index)
{
    a3_vector_t *vector = (a3_vector_t *)prop->data;
    if (data_position < 2) {
        data->byte[payload_index++] = vector->size;
        ++data_position;
        if (payload_index == A3_DATA_LENGTH) {
            return payload_index;
        }
    }
    // This implementation could be faster but is endian free.
    int data_index = data_position - 2;
    int data_size = MIN(A3_DATA_LENGTH - payload_index, vector->size - data_index);
    memcpy(&data->byte[payload_index], &((uint8_t*)vector->data)[data_index], data_size);
    data_position += data_size;
    payload_index += data_size;
    if (data_position == vector->size + 2) {
        ++prop_position;
        data_position = 0;
    }
    return payload_index;
}

static int FillString(a3_module_property_t *prop, CAN_DATA_BYTES_MSG *data, int payload_index)
{
    const char *value = (const char *)prop->data;
    uint8_t length = strlen(value);
    if (data_position < 2) {
        data->byte[payload_index++] = length;
        ++data_position;
        if (payload_index == A3_DATA_LENGTH) {
            return payload_index;
        }
    }
    // This implementation could be faster but is endian free.
    int data_index = data_position - 2;
    int data_size = MIN(A3_DATA_LENGTH - payload_index, length - data_index);
    memcpy(&data->byte[payload_index], &value[data_index], data_size);
    data_position += data_size;
    payload_index += data_size;
    if (data_position == length + 2) {
        ++prop_position;
        data_position = 0;
    }
    return payload_index;
}

static int FillPropertyData(CAN_DATA_BYTES_MSG *data, int payload_index)
{
    if (prop_position >= NUM_PROPS) {
        return A3_DATA_LENGTH;
    }
    a3_module_property_t *current_prop = &config[prop_position];
    if (data_position < 1) {
        data->byte[payload_index++] = current_prop->id;
        ++data_position;
        if (payload_index == A3_DATA_LENGTH) {
            return payload_index;
        }
    }
    switch (current_prop->value_type) {
    case A3_U8:
        return FillU8(current_prop, data, payload_index);
    case A3_U16:
        return FillU16(current_prop, data, payload_index);
    case A3_U32:
        return FillU32(current_prop, data, payload_index);
    case A3_STRING:
        return FillString(current_prop, data, payload_index);
    case A3_VECTOR_U8:
        return FillVectorU8(current_prop, data, payload_index);
    }
    return A3_DATA_LENGTH;
}

static void HandleRequestConfig()
{
    wire_id = CAN_RX_DATA_BYTE(0, 2) + A3_ID_ADMIN_WIRES_BASE;
    prop_position = 0;
    data_position = 0;
    int payload_index = 0;
    CAN_DATA_BYTES_MSG data;
    data.byte[payload_index++] = NUM_PROPS;
    while (payload_index < A3_DATA_LENGTH) {
        payload_index = FillPropertyData(&data, payload_index);
    }
    A3SendDataStandard(wire_id, payload_index, &data);
}

static void HandleContinueConfig()
{
    int payload_index = 0;
    CAN_DATA_BYTES_MSG data;
    while (payload_index < A3_DATA_LENGTH) {
        payload_index = FillPropertyData(&data, payload_index);
    }
    A3SendDataStandard(wire_id, payload_index, &data);
}

static void HandleMissionControlCommand(uint8_t opcode)
{
    switch (opcode) {
    case A3_MC_PING: {
        CAN_DATA_BYTES_MSG data;
        data.byte[0] = A3_IM_PING_REPLY;
        A3SendDataStandard(a3_module_id, 1, &data);
        if (CAN_GET_DLC(0) >= 3 && CAN_RX_DATA_BYTE(0, 2)) {
            BlinkGreen(70, 3);
        }
        break;
    }
    case A3_MC_REQUEST_NAME:
        HandleRequestName();
        break;
    case A3_MC_CONTINUE_NAME:
        HandleContinueName();
        break;
    case A3_MC_REQUEST_CONFIG:
        HandleRequestConfig();
        break;
    case A3_MC_CONTINUE_CONFIG:
        HandleContinueConfig();
        break;
    }
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
    default: {
        uint16_t target_module = CAN_RX_DATA_BYTE(0, 1) + A3_ID_IM_BASE;
        if (target_module == a3_module_id) {
            HandleMissionControlCommand(opcode);
        }
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

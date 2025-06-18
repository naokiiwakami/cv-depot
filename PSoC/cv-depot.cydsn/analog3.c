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

// module properties //////////////////////////////////////////////////

// identifiers
uint32_t a3_module_uid;
uint16_t a3_module_id;

static uint8_t a3_module_type = MODULE_TYPE;
static uint8_t num_voices = NUM_VOICES;

#define MAX_CONFIG_DATA_LENGTH 64
static char module_name[MAX_CONFIG_DATA_LENGTH];
// Temporary buffer to keep config data while streaming.
// We don't use heap since its size is very limited.
static uint8_t stream_buffer[MAX_CONFIG_DATA_LENGTH];

static a3_vector_t channels = {
    .size = NUM_VOICES,
    .data = &midi_config.channels,
};

static void commit_integer(a3_property_t *);
static void commit_string(a3_property_t *);
static void commit_vector_u8(a3_property_t *);

static a3_property_t config[NUM_PROPS] = {
    {
        .id = PROP_MODULE_UID,
        .value_type = TYPE_MODULE_UID,
        .protected = 1,
        .data = &a3_module_uid,
        .commit = NULL,
    }, {
        .id = PROP_MODULE_TYPE,
        .value_type = TYPE_MODULE_TYPE,
        .protected = 1,
        .data = &a3_module_type,
        .commit = NULL,
    }, {
        .id = PROP_MODULE_NAME,
        .value_type = TYPE_MODULE_NAME,
        .protected = 0,
        .data = module_name,
        .commit = commit_string,
    }, {
        .id = PROP_NUM_VOICES,
        .value_type = A3_U8,
        .protected = 0,
        .data = &num_voices,
        .commit = commit_integer,
    }, {
        .id = PROP_KEY_ASSIGNMENT_MODE,
        .value_type = A3_U8,
        .protected = 0,
        .data = &midi_config.key_assignment_mode,
        .commit = commit_integer,
    }, {
        .id = PROP_KEY_PRIORITY,
        .value_type = A3_U8,
        .protected = 0,
        .data = &midi_config.key_priority,
        .commit = commit_integer,
    }, {
        .id = PROP_MIDI_CHANNELS,
        .value_type = A3_VECTOR_U8,
        .protected = 0,
        .data = &channels,
        .commit = commit_vector_u8,
    }, 
};

// CAN tx/rx methods /////////////////////////////////////////////////////

#define MAILBOX_MC 0
#define MAILBOX_OTHERS 1

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

void A3SendRemoteFrameStandard(uint32_t id)
{
    // CAN_DATA_BYTES_MSG msg;
    CAN_TX_MSG message = {
        .id = id,
        .rtr = 1,
        .ide = 0,
        .dlc = 0,
        .irq = 0,
        .msg = NULL,
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

// Stream control ////////////////////////////////////////////////////////////////////////

// communication states
static struct stream_state {
    uint32_t prop_position;
    uint32_t data_position;
    uint32_t wire_addr;

    uint8_t num_remaining_properties;
    uint8_t data_size;
} stream_state = {
    .prop_position = 0,
    .data_position = 0,
    .wire_addr = A3_ID_INVALID,
    .num_remaining_properties = 0,
    .data_size = 0,
};

/**
 * Initializes the admin wire for writes.
 *
 * @param wire_addr - wire ID to start
 * @param prop_start_index - starting index of the properties
 * @param num_props - number of properties to send
 */
static void InitializeWireWrites(uint32_t wire_addr, int prop_start_index, int num_props)
{
    stream_state.wire_addr = wire_addr;
    stream_state.prop_position = prop_start_index;
    stream_state.num_remaining_properties = num_props;
}

#define NOWHERE 0xffffffff

/**
 * Initializes the admin wire for writes.
 *
 * @param wire_addr - wire ID to start
 * @param prop_start_index - starting index of the properties
 * @param num_props - number of properties to send
 */
static void InitializeWireReads(uint32_t wire_addr)
{
    stream_state.wire_addr = wire_addr;
    stream_state.prop_position = NOWHERE;
    stream_state.num_remaining_properties = 0;
    stream_state.data_position = 0;
    stream_state.data_size = 0;
}

/**
 * Check current stream state and terminate property and/or chunk if the positions
 * reach the ends.
 */
static void CheckForTransferTermination(uint32_t property_data_length)
{
    if (stream_state.data_position == property_data_length) {
        // completed transferring the property. proceed to the next.
        stream_state.data_position = 0;
        ++stream_state.prop_position;
        --stream_state.num_remaining_properties;
        if (stream_state.num_remaining_properties == 0) {
            // entire transfer completed. clear the wire ID
            stream_state.wire_addr = A3_ID_INVALID;
        }
    }
}

static uint8_t DoneStream()
{
    return stream_state.num_remaining_properties == 0;
}

static void TerminateStream()
{
    stream_state.wire_addr = A3_ID_INVALID;
    stream_state.prop_position = 0;
    stream_state.data_position = 0;
    stream_state.num_remaining_properties = 0;
}

#define MIN(x, y) ((x) < (y) ? (x) : (y))

static int FillInt(a3_property_t *prop, CAN_DATA_BYTES_MSG *data, uint8_t payload_index, uint8_t num_bytes)
{
    if (stream_state.data_position < 2) {
        data->byte[payload_index++] = num_bytes;
        ++stream_state.data_position;
        if (payload_index == A3_DATA_LENGTH) {
            return payload_index;
        }
    }
    uint32_t value = *(uint32_t *)prop->data;
    uint8_t total_bytes = num_bytes + 2;
    uint8_t bytes_to_send = MIN(A3_DATA_LENGTH - payload_index, total_bytes - stream_state.data_position);
    value >>= (total_bytes - bytes_to_send - stream_state.data_position) * 8; 
    for (int i = 0; i < bytes_to_send; ++i) {
        data->byte[payload_index + bytes_to_send - i - 1] = value & 0xff;
        value >>= 8;
    }
    stream_state.data_position += bytes_to_send;
    payload_index += bytes_to_send;
    CheckForTransferTermination(total_bytes);
    return payload_index;
}

static int FillVectorU8(a3_property_t *prop, CAN_DATA_BYTES_MSG *data, int payload_index)
{
    a3_vector_t *vector = (a3_vector_t *)prop->data;
    if (stream_state.data_position < 2) {
        data->byte[payload_index++] = vector->size;
        ++stream_state.data_position;
        if (payload_index == A3_DATA_LENGTH) {
            return payload_index;
        }
    }
    // This implementation could be faster but is endian free.
    int data_index = stream_state.data_position - 2;
    int data_size = MIN(A3_DATA_LENGTH - payload_index, vector->size - data_index);
    memcpy(&data->byte[payload_index], &((uint8_t*)vector->data)[data_index], data_size);
    stream_state.data_position += data_size;
    payload_index += data_size;
    CheckForTransferTermination(vector->size + 2);
    return payload_index;
}

static int FillString(a3_property_t *prop, CAN_DATA_BYTES_MSG *data, int payload_index)
{
    const char *value = (const char *)prop->data;
    uint8_t length = strlen(value);
    if (stream_state.data_position < 2) {
        data->byte[payload_index++] = length;
        ++stream_state.data_position;
        if (payload_index == A3_DATA_LENGTH) {
            return payload_index;
        }
    }
    // This implementation could be faster but is endian free.
    int data_index = stream_state.data_position - 2;
    int data_size = MIN(A3_DATA_LENGTH - payload_index, length - data_index);
    memcpy(&data->byte[payload_index], &value[data_index], data_size);
    stream_state.data_position += data_size;
    payload_index += data_size;
    CheckForTransferTermination(length + 2);
    return payload_index;
}

int FillPropertyData(CAN_DATA_BYTES_MSG *data, int payload_index)
{
    if (stream_state.prop_position >= NUM_PROPS) {
        return A3_DATA_LENGTH;
    }
    a3_property_t *current_prop = &config[stream_state.prop_position];
    if (stream_state.data_position < 1) {
        data->byte[payload_index++] = current_prop->id;
        ++stream_state.data_position;
        if (payload_index == A3_DATA_LENGTH) {
            return payload_index;
        }
    }
    switch (current_prop->value_type) {
    case A3_U8:
        return FillInt(current_prop, data, payload_index, 1);
    case A3_U16:
        return FillInt(current_prop, data, payload_index, 2);
    case A3_U32:
        return FillInt(current_prop, data, payload_index, 4);
    case A3_STRING:
        return FillString(current_prop, data, payload_index);
    case A3_VECTOR_U8:
        return FillVectorU8(current_prop, data, payload_index);
    }
    return A3_DATA_LENGTH;
}

void commit_integer(a3_property_t *prop)
{
    memcpy(prop->data, stream_buffer, stream_state.data_size);
}

void commit_string(a3_property_t *prop)
{
    size_t data_len = MIN(stream_state.data_size, MAX_CONFIG_DATA_LENGTH - 1);
    memcpy(prop->data, stream_buffer, data_len);
    ((char *)prop->data)[data_len] = 0;
}

void commit_vector_u8(a3_property_t *prop)
{
    a3_vector_t *value = (a3_vector_t *)prop->data;
    size_t size = MIN(value->size, stream_state.data_size);
    memcpy(value->data, stream_buffer, size);
}

// Module API methods ///////////////////////////////////////////////////////////////////////

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
    uint32_t wire_addr = CAN_RX_DATA_BYTE(MAILBOX_MC, 2) + A3_ID_ADMIN_WIRES_BASE;
    InitializeWireWrites(wire_addr, PROP_MODULE_NAME, 1);
    CAN_DATA_BYTES_MSG data;
    int payload_index = FillPropertyData(&data, 0);
    A3SendDataStandard(wire_addr, payload_index, &data);
}

static void HandleContinueName()
{
    uint32_t wire_addr = stream_state.wire_addr;
    if (wire_addr == A3_ID_INVALID) {
        // no active stream, ignore.
        return;
    }
    CAN_DATA_BYTES_MSG data;
    int payload_index = FillPropertyData(&data, 0);
    A3SendDataStandard(wire_addr, payload_index, &data);
}

static void HandleRequestConfig()
{
    uint32_t wire_addr = CAN_RX_DATA_BYTE(MAILBOX_MC, 2) + A3_ID_ADMIN_WIRES_BASE;
    InitializeWireWrites(wire_addr, 0, NUM_PROPS);
    int payload_index = 0;
    CAN_DATA_BYTES_MSG data;
    data.byte[payload_index++] = NUM_PROPS;
    while (payload_index < A3_DATA_LENGTH && !DoneStream()) {
        payload_index = FillPropertyData(&data, payload_index);
    }
    A3SendDataStandard(wire_addr, payload_index, &data);
}

static void HandleContinueConfig()
{
    uint32_t wire_addr = stream_state.wire_addr;
    if (wire_addr == A3_ID_INVALID) {
        // no active stream, ignore.
        return;
    }
    int payload_index = 0;
    CAN_DATA_BYTES_MSG data;
    while (payload_index < A3_DATA_LENGTH && !DoneStream()) {
        payload_index = FillPropertyData(&data, payload_index);
    }
    A3SendDataStandard(wire_addr, payload_index, &data);
}

static void HandleModifyConfig()
{
    uint32_t wire_addr = CAN_RX_DATA_BYTE(MAILBOX_MC, 2) + A3_ID_ADMIN_WIRES_BASE;
    InitializeWireReads(wire_addr);
    A3SendRemoteFrameStandard(wire_addr);
}

static void HandleMissionControlCommand(uint8_t opcode)
{
    switch (opcode) {
    case A3_MC_PING: {
        CAN_DATA_BYTES_MSG data;
        data.byte[0] = A3_IM_PING_REPLY;
        A3SendDataStandard(a3_module_id, 1, &data);
        if (CAN_GET_DLC(MAILBOX_MC) >= 3 && CAN_RX_DATA_BYTE(MAILBOX_MC, 2)) {
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
    case A3_MC_MODIFY_CONFIG:
        HandleModifyConfig();
        break;
    }
}

void HandleMissionControlMessage(void* arg)
{
    (void)arg;
    uint8_t opcode = CAN_RX_DATA_BYTE(MAILBOX_MC, 0);
    
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
            a3_module_id = CAN_RX_DATA_BYTE(MAILBOX_MC, 5) + A3_ID_IM_BASE;
        }
        Pin_LED_Write(0);
        break;
    }
    default: {
        uint16_t target_module = CAN_RX_DATA_BYTE(MAILBOX_MC, 1) + A3_ID_IM_BASE;
        if (target_module == a3_module_id) {
            HandleMissionControlCommand(opcode);
        }
    }
    }
}

static void ParseInteger(a3_property_t *prop, uint8_t payload_index, uint8_t bytes_to_read, uint8_t integer_bytes)
{
    (void) prop;
    for (uint8_t i = 0; i < bytes_to_read; ++i) {
        stream_buffer[integer_bytes - stream_state.data_position - i - 1] =
            CAN_RX_DATA_BYTE(1, payload_index + i);
    }
}

static void ParseString(a3_property_t *prop, uint8_t payload_index, uint8_t bytes_to_read)
{
    (void) prop;
    uint8_t limit = MAX_CONFIG_DATA_LENGTH - stream_state.data_position - 1; // buffer overflows beyond this
    for (uint8_t i = 0; i < bytes_to_read && i < limit; ++i) {
        stream_buffer[stream_state.data_position + i] = (char) CAN_RX_DATA_BYTE(1, payload_index + i);
    }
}

static void ParseVectorU8(a3_property_t *prop, uint8_t payload_index, uint8_t bytes_to_read)
{
    (void) prop;
    for (uint8_t i = 0; i < bytes_to_read; ++i) {
        stream_buffer[stream_state.data_position + i] = (char) CAN_RX_DATA_BYTE(1, payload_index + i);
    }
}

static void ConsumeRxData(a3_property_t *prop, uint8_t payload_index, uint8_t bytes_to_read)
{
    if (prop != NULL && !prop->protected) {
        switch (prop->value_type) {
        case A3_U8:
            ParseInteger(prop, payload_index, bytes_to_read, 1);
            break;
        case A3_U16:
            ParseInteger(prop, payload_index, bytes_to_read, 2);
            break;
        case A3_U32:
            ParseInteger(prop, payload_index, bytes_to_read, 4);
            break;
        case A3_STRING:
            ParseString(prop, payload_index, bytes_to_read);
            break;
        case A3_VECTOR_U8:
            ParseVectorU8(prop, payload_index, bytes_to_read);
            break;
        }
    }
    stream_state.data_position += bytes_to_read;
    if (stream_state.data_position == stream_state.data_size) {
        // end reading a property
        if (prop != NULL && !prop->protected && prop->commit != NULL) {
            prop->commit(prop);
        }
        stream_state.data_position = 0;
        stream_state.data_size = 0;
        stream_state.prop_position = NOWHERE;
        --stream_state.num_remaining_properties;
    }
}

static void ReadDataFrame()
{
    uint8_t num_src_bytes = CAN_GET_DLC(1);
    uint8_t payload_index = 0;
    if (stream_state.num_remaining_properties == 0) {
        stream_state.num_remaining_properties = CAN_RX_DATA_BYTE(MAILBOX_OTHERS, payload_index);
        ++payload_index;
    }
    while (payload_index < num_src_bytes && !DoneStream()) {
        if (stream_state.prop_position == NOWHERE) {
            stream_state.prop_position = CAN_RX_DATA_BYTE(MAILBOX_OTHERS, payload_index);
            ++payload_index;
            if (payload_index >= num_src_bytes) {
                return;
            }
        }
        if (stream_state.data_size == 0) {
            stream_state.data_size = CAN_RX_DATA_BYTE(MAILBOX_OTHERS, payload_index);
            ++payload_index;
            if (payload_index >= num_src_bytes) {
                return;
            }
        }
        a3_property_t *prop;
        if (stream_state.prop_position >= NUM_PROPS) {
            // unknown property
            prop = NULL;
        } else {
            prop = &config[stream_state.prop_position];
        }
        uint8_t bytes_to_read = MIN(
            stream_state.data_size - stream_state.data_position,
            num_src_bytes - payload_index);
        ConsumeRxData(prop, payload_index, bytes_to_read);
        payload_index += bytes_to_read;
        if (DoneStream()) {
            TerminateStream();
            break;
        }
        A3SendRemoteFrameStandard(stream_state.wire_addr);
    }
}

static void HandleGeneralStandardMessage()
{
    uint32_t id = CAN_GET_RX_ID(MAILBOX_OTHERS);
    if (id != stream_state.wire_addr) {
        return;
    }
    // The message is a data frame to the current wire address
    ReadDataFrame();
}

void HandleGeneralMessage(void *arg)
{
    (void)arg;
    uint8_t is_extended = CAN_GET_RX_IDE(MAILBOX_OTHERS);
    if (!is_extended) {
        HandleGeneralStandardMessage();
        return;
    }
    uint32_t id = CAN_GET_RX_ID(MAILBOX_OTHERS);
    if (id != a3_module_uid) {
        // it's not about me
        return;
    }
    uint8_t opcode = CAN_RX_DATA_BYTE(MAILBOX_OTHERS, 0);
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

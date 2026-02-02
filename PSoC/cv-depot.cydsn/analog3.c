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

#include <stdint.h>
#include <stdlib.h>

#include "analog3.h"
#include "config.h"
#include "eeprom.h"
#include "hardware.h"

// Temporary buffer to keep config data while streaming.
// We don't use heap since its size is very limited.
static uint8_t stream_buffer[A3_MAX_CONFIG_DATA_LENGTH];

// CAN tx/rx methods /////////////////////////////////////////////////////

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
enum CurrentStream {
    kCurrentStreamNone,
    kCurrentStreamGetName,
    kCurrentStreamGetConfig,
    kCurrentStreamSetProps,
};

static struct stream_state {
    enum CurrentStream current_stream;
    uint32_t prop_position;
    uint32_t data_position;
    uint32_t wire_id;

    uint8_t num_remaining_properties;
    uint8_t data_size;
    uint8_t num_properties_sent;
} stream_state = {
    .current_stream = kCurrentStreamNone,
    .prop_position = 0,
    .data_position = 0,
    .wire_id = A3_ID_INVALID,
    .num_remaining_properties = 0,
    .data_size = 0,
    .num_properties_sent = 0,
};

/**
 * Initializes the admin wire for writes.
 *
 * @param wire_id - wire ID to start
 * @param prop_start_index - starting index of the properties
 * @param num_props - number of properties to send
 * @param stream - type of stream to initiate
 */
static void InitiateWireWrites(uint32_t wire_id, int prop_start_index, int num_props,
    enum CurrentStream stream)
{
    stream_state.current_stream = stream;
    stream_state.wire_id = wire_id;
    stream_state.prop_position = prop_start_index;
    stream_state.num_remaining_properties = num_props;
    stream_state.num_properties_sent = 0;
}

#define NOWHERE 0xffffffff

/**
 * Initializes the admin wire for writes.
 *
 * @param wire_id - wire ID to start
 * @param prop_start_index - starting index of the properties
 * @param num_props - number of properties to send
 * @param stream - type of stream to initiate
 */
static void InitiateWireReads(uint32_t wire_id, enum CurrentStream stream)
{
    stream_state.current_stream = stream;
    stream_state.wire_id = wire_id;
    stream_state.prop_position = NOWHERE;
    stream_state.num_remaining_properties = 0;
    stream_state.data_position = 0;
    stream_state.data_size = 0;
}

/**
 * Terminate the wire by clearing current stream states.
 */
static void TerminateWire()
{
    stream_state.wire_id = A3_ID_INVALID;
    stream_state.prop_position = 0;
    stream_state.data_position = 0;
    stream_state.num_remaining_properties = 0;
    stream_state.current_stream = kCurrentStreamNone;
    stream_state.num_properties_sent = 0;
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
            // entire transfer completed. terminate the stream
            TerminateWire();
        }
    }
}

static uint8_t DoneStream()
{
    return stream_state.num_remaining_properties == 0;
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

// Module API methods ///////////////////////////////////////////////////////////////////////

void InitializeA3Module()
{
    a3_module_uid = Load32(ADDR_MODULE_UID);
    if (a3_module_uid == 0) {
        a3_module_uid = rand() & 0x1fffffff;
        Save32(a3_module_uid, ADDR_MODULE_UID);
    }
    a3_module_id = A3_ID_UNASSIGNED;
    LoadString(module_name, A3_MAX_CONFIG_DATA_LENGTH, ADDR_NAME);
    if (module_name[0] == '\0') {
       strcpy(module_name, "cv-depot");
       SaveString(module_name, A3_MAX_CONFIG_DATA_LENGTH, ADDR_NAME);
    }
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

static void HandleRequestName(can_message_t *message)
{
    uint32_t wire_id = message->data[2] + A3_ID_ADMIN_WIRES_BASE;
    CAN_DATA_BYTES_MSG data;
    if (stream_state.current_stream == kCurrentStreamNone) {
        data.byte[0] = StreamStatusReady;
        A3SendDataStandard(wire_id, 1, &data);
        InitiateWireWrites(wire_id, PROP_MODULE_NAME, 1, kCurrentStreamGetName);
    } else {
        data.byte[0] = StreamStatusBusy;
        A3SendDataStandard(wire_id, 1, &data);
    }
}

static void HandleContinueName()
{
    uint32_t wire_id = stream_state.wire_id;
    if (wire_id == A3_ID_INVALID) {
        // no active stream, ignore.
        return;
    }
    CAN_DATA_BYTES_MSG data;
    int payload_index = FillPropertyData(&data, 0);
    A3SendDataStandard(wire_id, payload_index, &data);
}

static void HandleRequestConfig(can_message_t *message)
{
    uint32_t wire_id = message->data[2] + A3_ID_ADMIN_WIRES_BASE;
    CAN_DATA_BYTES_MSG data;
    if (stream_state.current_stream == kCurrentStreamNone) {
        data.byte[0] = StreamStatusReady;
        A3SendDataStandard(wire_id, 1, &data);
        InitiateWireWrites(wire_id, 0, NUM_PROPS, kCurrentStreamGetConfig);
    } else {
        data.byte[0] = StreamStatusBusy;
        A3SendDataStandard(wire_id, 1, &data);
    }
}

static void HandleContinueConfig()
{
    uint32_t wire_id = stream_state.wire_id;
    if (wire_id == A3_ID_INVALID) {
        // no active stream, ignore.
        return;
    }
    int payload_index = 0;
    CAN_DATA_BYTES_MSG data;
    if (!stream_state.num_properties_sent) {
        data.byte[payload_index++] = stream_state.num_remaining_properties;
        stream_state.num_properties_sent = 1;
    }
    while (payload_index < A3_DATA_LENGTH && !DoneStream()) {
        payload_index = FillPropertyData(&data, payload_index);
    }
    A3SendDataStandard(wire_id, payload_index, &data);
}

static void HandleModifyConfig(can_message_t *message)
{
    uint32_t wire_id = message->data[2] + A3_ID_ADMIN_WIRES_BASE;
    CAN_DATA_BYTES_MSG data;
    if (stream_state.current_stream == kCurrentStreamNone) {
        data.byte[0] = StreamStatusReady;
        A3SendDataStandard(wire_id, 1, &data);
        InitiateWireReads(wire_id, kCurrentStreamSetProps);
    } else {
        data.byte[0] = StreamStatusBusy;
        A3SendDataStandard(wire_id, 1, &data);
    }
}

static void HandleMissionControlCommand(uint8_t opcode, can_message_t *message)
{
    switch (opcode) {
    case A3_MC_PING: {
        CAN_DATA_BYTES_MSG data;
        data.byte[0] = A3_IM_PING_REPLY;
        A3SendDataStandard(a3_module_id, 1, &data);
        if (message->dlc >= 3 && message->data[2]) {
            BlinkGreen(70, 5);
        }
        break;
    }
    case A3_MC_REQUEST_NAME:
        HandleRequestName(message);
        break;
    case A3_MC_REQUEST_CONFIG:
        HandleRequestConfig(message);
        break;
    case A3_MC_MODIFY_CONFIG:
        HandleModifyConfig(message);
        break;
    }
}

void HandleMissionControlMessage(void* arg)
{
    can_message_t *message = (can_message_t *)arg;
    uint8_t opcode = message->data[0];

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
            = message->data[1] << 24
            | message->data[2] << 16
            | message->data[3] << 8
            | message->data[4];
        if (target_module == a3_module_uid) {
            a3_module_id = message->data[5] + A3_ID_IM_BASE;
            CAN_DATA_BYTES_MSG data;
            data.byte[0] = A3_IM_ID_ASSIGN_ACK;
            A3SendDataStandard(a3_module_id, 1, &data);
            RED_ENCODER_LED_OFF();
            BlinkGreen(70, 7);
        }
        break;
    }
    default: {
        uint16_t target_module = message->data[1] + A3_ID_IM_BASE;
        if (target_module == a3_module_id) {
            HandleMissionControlCommand(opcode, message);
        }
    }
    }
    CyGlobalIntDisable;
    q_full = 0;
    q_tail = (q_tail + 1) % MESSAGE_QUEUE_SIZE;
    CyGlobalIntEnable;
}

static void ParseInteger(can_message_t *message, a3_property_t *prop, uint8_t payload_index, uint8_t bytes_to_read, uint8_t integer_bytes)
{
    (void) prop;
    for (uint8_t i = 0; i < bytes_to_read; ++i) {
        stream_buffer[integer_bytes - stream_state.data_position - i - 1] =
            message->data[payload_index + i];
    }
}

static void ParseString(can_message_t *message, a3_property_t *prop, uint8_t payload_index, uint8_t bytes_to_read)
{
    (void) prop;
    uint8_t limit = A3_MAX_CONFIG_DATA_LENGTH - stream_state.data_position - 1; // buffer overflows beyond this
    for (uint8_t i = 0; i < bytes_to_read && i < limit; ++i) {
        stream_buffer[stream_state.data_position + i] = (char)message->data[payload_index + i];
    }
}

static void ParseVectorU8(can_message_t *message, a3_property_t *prop, uint8_t payload_index, uint8_t bytes_to_read)
{
    (void) prop;
    for (uint8_t i = 0; i < bytes_to_read; ++i) {
        stream_buffer[stream_state.data_position + i] = (char)message->data[payload_index + i];
    }
}

static void ConsumeRxData(can_message_t *message, a3_property_t *prop, uint8_t payload_index, uint8_t bytes_to_read)
{
    if (prop != NULL && !prop->protected) {
        switch (prop->value_type) {
        case A3_U8:
            ParseInteger(message, prop, payload_index, bytes_to_read, 1);
            break;
        case A3_U16:
            ParseInteger(message, prop, payload_index, bytes_to_read, 2);
            break;
        case A3_U32:
            ParseInteger(message, prop, payload_index, bytes_to_read, 4);
            break;
        case A3_STRING:
            ParseString(message, prop, payload_index, bytes_to_read);
            break;
        case A3_VECTOR_U8:
            ParseVectorU8(message, prop, payload_index, bytes_to_read);
            break;
        }
    }
    stream_state.data_position += bytes_to_read;
    if (stream_state.data_position == stream_state.data_size) {
        // end reading a property
        if (prop != NULL && !prop->protected && prop->commit != NULL) {
            prop->commit(prop, stream_buffer, stream_state.data_size);
        }
        stream_state.data_position = 0;
        stream_state.data_size = 0;
        stream_state.prop_position = NOWHERE;
        --stream_state.num_remaining_properties;
    }
}

static void ReadDataFrame(can_message_t *message)
{
    uint8_t num_src_bytes = message->dlc;
    uint8_t payload_index = 0;
    if (stream_state.num_remaining_properties == 0) {
        stream_state.num_remaining_properties = message->data[payload_index];
        ++payload_index;
    }
    while (payload_index < num_src_bytes && !DoneStream()) {
        if (stream_state.prop_position == NOWHERE) {
            stream_state.prop_position = message->data[payload_index];
            ++payload_index;
            if (payload_index >= num_src_bytes) {
                return;
            }
        }
        if (stream_state.data_size == 0) {
            stream_state.data_size = message->data[payload_index];
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
        ConsumeRxData(message, prop, payload_index, bytes_to_read);
        payload_index += bytes_to_read;
        if (DoneStream()) {
            TerminateWire();
            break;
        }
        CAN_DATA_BYTES_MSG data;
        A3SendDataStandard(stream_state.wire_id, 0, &data);
        A3SendRemoteFrameStandard(stream_state.wire_id);
    }
}

static void HandleGeneralStandardMessage(can_message_t *message)
{
    if (message->id == stream_state.wire_id) {
        switch (stream_state.current_stream) {
        case kCurrentStreamGetName:
            HandleContinueName();
            break;
        case kCurrentStreamGetConfig:
            HandleContinueConfig();
            break;
        case kCurrentStreamSetProps:
            ReadDataFrame(message);
            break;
        case kCurrentStreamNone:
            // shouldn't happen, ignore silently
            break;
        }
    }
}

void HandleGeneralMessage(void *arg)
{
    can_message_t *message = (can_message_t *)arg;
    uint8_t is_extended = message->extended;
    if (!is_extended) {
        HandleGeneralStandardMessage(message);
        return;
    }
    uint32_t id = message->id;
    if (id != a3_module_uid) {
        // it's not about me
        return;
    }
    uint8_t opcode = message->data[0];
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
    CyGlobalIntDisable;
    q_full = 0;
    q_head = (q_head + 1) % MESSAGE_QUEUE_SIZE;
    CyGlobalIntEnable;
}

/* [] END OF FILE */

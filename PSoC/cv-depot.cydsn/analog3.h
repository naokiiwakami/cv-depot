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

#pragma once

#include <stdint.h>

#include "project.h"

// ID assignments /////////////////////////////////
#define A3_ID_UNASSIGNED          0x0
#define A3_ID_MIDI_TIMING_CLOCK 0x100
#define A3_ID_MIDI_VOICE_BASE   0x101
#define A3_ID_MIDI_REAL_TIME    0x140

#define A3_ID_ADMIN_WIRES_BASE  0x680

#define A3_ID_MISSION_CONTROL   0x700
#define A3_ID_IM_BASE           0x700

#define A3_ID_INVALID      0xffffffff

// Message types //////////////////////////////////

/* MIDI channel voice messages */
#define A3_VOICE_MSG_SET_NOTE          0x07
#define A3_VOICE_MSG_GATE_OFF          0x08
#define A3_VOICE_MSG_GATE_ON           0x09
#define A3_VOICE_MSG_POLY_KEY_PRESSURE 0x0A

/* MIDI channel messages */
#define A3_VOICE_MSG_CONTROL_CHANGE    0x0B
#define A3_VOICE_MSG_PROGRAM_CHANGE    0x0C
#define A3_VOICE_MSG_CHANNEL_PRESSURE  0x0D
#define A3_VOICE_MSG_PITCH_BEND        0x0E

/* Module administration opcodes */
#define A3_ADMIN_SIGN_IN 0x01
#define A3_ADMIN_NOTIFY_ID 0x02
#define A3_ADMIN_REQ_UID_CANCEL 0x03

/* Mission control opcodes */
#define A3_MC_SIGN_IN 0x01
#define A3_MC_ASSIGN_MODULE_ID 0x02
#define A3_MC_PING 0x03
#define A3_MC_REQUEST_NAME 0x04
#define A3_MC_REQUEST_CONFIG 0x05
#define A3_MC_MODIFY_CONFIG 0x08

/* Individual module opcodes */
#define A3_IM_PING_REPLY 0x01
#define A3_IM_ID_ASSIGN_ACK 0x02

#define A3_DATA_LENGTH 8
#define A3_MAX_CONFIG_DATA_LENGTH 64

/* Stream status */
enum StreamStatus {
    StreamStatusReady = 0,
    StreamStatusBusy = 1,
    StreamStatusNotSupported = 2,
    StreamStatusNoSuchStream = 3,
};

enum A3PropertyValueType {
    A3_U8,
    A3_U16,
    A3_U32,
    // A3_I8,
    // A3_I16,
    // A3_I32,
    A3_STRING,
    A3_VECTOR_U8
};

typedef struct A3Vector {
    uint8_t size;
    void *data;
} a3_vector_t;

/**
 * Least set of parameters necessary for sharing config with the Mission Control.
 */
typedef struct A3Property {
    uint8_t id;  // attribute ID
    uint8_t value_type; // value type
    uint8_t protected;  // indicates if the value is write-protected
    void *data;
    // function to commit stream data
    void (*commit)(struct A3Property *, uint8_t *data, uint8_t len);
    // address to persist configuration data, 0xffff for a constant value
    uint16_t save_addr;
} a3_property_t;

// Properties that are common among modules
#define PROP_MODULE_UID 0
#define TYPE_MODULE_UID A3_U32
#define PROP_MODULE_TYPE 1
#define TYPE_MODULE_TYPE A3_U16
#define PROP_MODULE_NAME 2
#define TYPE_MODULE_NAME A3_STRING

extern uint32_t a3_module_uid;
extern uint16_t a3_module_id;

extern void InitializeA3Module();
extern void SignIn();
extern void HandleMissionControlMessage(void *arg);
extern void HandleGeneralMessage(void *arg);

// low-level A3 message exchange method.
// TODO: Bring these details into analog3.c
extern void A3SendDataStandard(uint32_t id, uint8_t dlc, CAN_DATA_BYTES_MSG *data);
extern void A3SendDataExtended(uint32_t id, uint8_t dlc, CAN_DATA_BYTES_MSG *data);

/* [] END OF FILE */

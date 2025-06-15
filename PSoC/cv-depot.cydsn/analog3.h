/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * WHICH IS THE PROPERTY OF your company.
 * ========================================
*/

#pragma once

#include <stdint.h>

#include "project.h"

// ID assignments /////////////////////////////////
#define A3_ID_MIDI_TIMING_CLOCK 0x100
#define A3_ID_MIDI_VOICE_BASE   0x101
#define A3_ID_MIDI_REAL_TIME    0x140

#define A3_ID_ADMIN_WIRES_BASE  0x680

#define A3_ID_MISSION_CONTROL   0x700
#define A3_ID_IM_BASE           0x700

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
#define A3_MC_CONTINUE_NAME 0x05
#define A3_MC_REQUEST_CONFIG 0x06
#define A3_MC_CONTINUE_CONFIG 0x07

/* Individual module opcodes */
#define A3_IM_PING_REPLY 0x01
#define A3_IM_NAME_REPLY 0x02
#define A3_IM_CONFIG_REPLY 0x03

/* Attribute IDs */
#define A3_ATTR_NAME 0x1

#define A3_DATA_LENGTH 8

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
typedef struct A3ModuleProperty {
    uint8_t id;  // attribute ID
    uint8_t value_type; // value type
    void *data;
} a3_module_property_t;

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

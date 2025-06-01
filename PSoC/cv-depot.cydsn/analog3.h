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

#define A3_ID_MISSION_CONTROL        0x700
#define A3_ID_IM_BASE                0x700

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

/* Individual module opcodes */
#define A3_IM_REPLY_PING 0x01
#define A3_IM_REPLY_NAME 0x02

#define A3_DATA_LENGTH 8

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

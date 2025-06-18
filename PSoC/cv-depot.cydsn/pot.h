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

/*
 * This component drives multiple MCP4011 digital pots.
 *
 * See
 * https://ww1.microchip.com/downloads/aemDocuments/documents/OTH/ProductDocuments/DataSheets/20001978D.pdf
 * for the chip behavior.
 *
 * The digital pot is controlled by a simple 2-pin Up/Down command. One pot object
 * tracks curent status of the device. Two control pins are assigned as:
 *
 *   Pin_Pot_Select_* - to enable U/D command for eatch device
 *   Pin_Pot_UD - shared U/D command port
 *
 * The behavior of the pot control is asynchronous. Once a command starts, the main program must call
 * PotUpdate() method periodically until a command completes. There's no timing control in this library.
 * The main program has responsibility to throttle method calls. See the data sheet for the precise
 * timing requirements. Though required minimum time interval is pretty short typically (less than 1uS),
 * it's unlikely that the main program has to run any timing control if we put the method call in the
 * main loop.
 */

#pragma once

#include "project.h"

enum PotId {
    POT_NOTE_1,
    POT_NOTE_2,
    POT_PORTAMENT_1,
    POT_PORTAMENT_2,
};

// pot control phases
enum PotCommandPhase {
    PHASE_IDLE,
    PHASE_DIRECTION_SET,
    PHASE_TO_LOAD,
    PHASE_TO_TRIGGER,
};

typedef struct pot {
    uint8_t current;
    uint8_t target;
    uint8_t phase;
    uint8_t level;
    enum PotId pot_id;
} pot_t;

// pot instances
extern pot_t pot_note_1;
extern pot_t pot_note_2;
extern pot_t pot_portament_1;
extern pot_t pot_portament_2;

/**
 * Initialize the pot environment.
 *
 * The method resets the command output, unselects all pot devices
 * and creates pot objects.
 */
extern void PotGlobalInit();

/**
 * Clear a pot object.
 *
 * The method sets current and target wiper position to the power-on default 0x1f.
 * (maximum wiper setting is 3fh)
 * Also, PHASE_IDLE is set to the phase.
 *
 * @param pot: pot_t - The pot object to initialize
 * @param select: void (*)(uint8) - Function to select pot (1:disable, 0:enable)
 */
extern void PotInit(pot_t *pot, enum PotId pot_id);

/**
 * Change target wiper position.
 *
 * @param pot: pot_t - The pot object to modify
 * @param target: uint8_t - Target position. 0 <= target < 0x40
 */
extern void PotSetTargetPosition(pot_t *pot, int8_t target);

/**
 * Request to move the wiper position to the terminal B.
 *
 * It is done by moving the wiper downwards 63 times.
 */
extern void PotEnsureToMoveToB(pot_t *pot);

/**
 * Updates the corresponding pot based on the pot object.
 *
 * If the target position is different from current position,
 * the method starts writing to the corresponding pot to move the
 * wiper. The method proceeds only one pot command step per call.
 * The main program should call this method periodically until the
 * command completes.
 *
 * @param pot: pot_t - The pot object
 * @returns uint8_t: 0 if the command is in progress, 1 otherwise
 */
extern uint8_t PotUpdate(pot_t *pot);

/* [] END OF FILE */

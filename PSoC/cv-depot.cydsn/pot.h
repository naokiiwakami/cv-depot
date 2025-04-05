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

/*
 * This component drives multiple MCP4011 digital pots.
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

#ifndef POT_H
#define POT_H
    
#include "project.h"
    
enum PotId {
    POT_NOTE_1,
    POT_NOTE_2,
    POT_PORTAMENT_1,
    POT_PORTAMENT_2,
};

// pot control phases
enum PotCommandPhase {
    PHASE_IDLE = 0,
    PHASE_DIRECTION_SET = 1,
    PHASE_CHIP_SELECTED = 2,
    PHASE_READY = 3,
};

typedef struct pot {
    uint8_t current;
    uint8_t target;
    uint8_t phase;
    uint32_t last_checkpoint;
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
extern void PotChangeTargetPosition(pot_t *pot, int8_t target);

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

    
#endif
/* [] END OF FILE */

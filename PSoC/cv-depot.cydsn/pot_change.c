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

#include "pot.h"
#include "pot_change.h"

typedef struct pot_change_request {
    uint8_t requested;
    int8_t target;  // set -1 to ensure to move to the terminal B
    pot_t *pot;
} pot_change_request_t;

// Request queue
#define REQUEST_QUEUE_SIZE 8
static pot_change_request_t pot_change_requests[REQUEST_QUEUE_SIZE];
static uint8_t request_head = 0;
static uint8_t request_tail = 0;
static uint8_t pot_queue_overflow = 0;


uint8_t PotChangePlaceRequest(pot_t *pot, int8_t wiper_position)
{
    if (pot_queue_overflow) {
        return 1;
    }
    pot_change_request_t *request_item = &pot_change_requests[request_tail];
    request_tail = (request_tail + 1) % REQUEST_QUEUE_SIZE;
    if (request_head == request_tail) {
        pot_queue_overflow = 1;
    }

    request_item->requested = 0;
    request_item->pot = pot;
    request_item->target = wiper_position;

    return 0;
}

void PotChangeHandleRequests()
{
    if (request_head == request_tail && !pot_queue_overflow) {
        // nothing to handle
        return;
    }

    pot_change_request_t *item = &pot_change_requests[request_head];
    pot_t *pot = item->pot;

    if (!item->requested) {
        if (item->target < 0) {
            PotEnsureToMoveToB(pot);
        } else {
            PotSetTargetPosition(pot, item->target);
        }
        item->requested = 1;
    }

    if (PotUpdate(pot)) {
        request_head = (request_head + 1) % REQUEST_QUEUE_SIZE;
        pot_queue_overflow = 0;
    }
}

/* [] END OF FILE */

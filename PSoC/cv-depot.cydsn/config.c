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

#include "project.h"

#include "config.h"
#include "eeprom.h"
#include "key_assigner.h"
#include "midi.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))


// module properties //////////////////////////////////////////////////

// identifiers
uint32_t a3_module_uid;
uint16_t a3_module_id;

static uint8_t a3_module_type = MODULE_TYPE_CV_DEPOT;
static uint8_t num_voices = NUM_VOICES;

char module_name[A3_MAX_CONFIG_DATA_LENGTH];

static a3_vector_t channels = {
    .size = NUM_VOICES,
    .data = &midi_config.channels,
};

static void CommitInteger(a3_property_t *, uint8_t *data, uint8_t len);
static void CommitString(a3_property_t *, uint8_t *data, uint8_t len);
static void CommitVectorU8(a3_property_t *, uint8_t *data, uint8_t len);

a3_property_t config[NUM_PROPS] = {
    {
        .id = PROP_MODULE_UID,
        .value_type = TYPE_MODULE_UID,
        .protected = 1,
        .data = &a3_module_uid,
        .commit = NULL,
        .save_addr = ADDR_MODULE_UID,
    }, {
        .id = PROP_MODULE_TYPE,
        .value_type = TYPE_MODULE_TYPE,
        .protected = 1,
        .data = &a3_module_type,
        .commit = NULL,
        .save_addr = ADDR_UNSET,
    }, {
        .id = PROP_MODULE_NAME,
        .value_type = TYPE_MODULE_NAME,
        .protected = 0,
        .data = module_name,
        .commit = CommitString,
        .save_addr = ADDR_NAME,
    }, {
        .id = PROP_NUM_VOICES,
        .value_type = A3_U8,
        .protected = 0,
        .data = &num_voices,
        .commit = CommitInteger,
        .save_addr = ADDR_UNSET,
    }, {
        .id = PROP_KEY_ASSIGNMENT_MODE,
        .value_type = A3_U8,
        .protected = 0,
        .data = &midi_config.key_assignment_mode,
        .commit = CommitInteger,
        .save_addr = ADDR_KEY_ASSIGNMENT_MODE,
    }, {
        .id = PROP_KEY_PRIORITY,
        .value_type = A3_U8,
        .protected = 0,
        .data = &midi_config.key_priority,
        .commit = CommitInteger,
        .save_addr = ADDR_KEY_PRIORITY,
    }, {
        .id = PROP_MIDI_CHANNELS,
        .value_type = A3_VECTOR_U8,
        .protected = 0,
        .data = &channels,
        .commit = CommitVectorU8,
        .save_addr = ADDR_MIDI_CH_1,
    },
};

void CommitInteger(a3_property_t *prop, uint8_t *data, uint8_t len)
{
    memcpy(prop->data, data, len);
    if (prop->save_addr == ADDR_UNSET) {
        return;
    }
    EEPROM_UpdateTemperature();
    switch (prop->value_type) {
    case A3_U8:
        EEPROM_WriteByte(*(uint8_t *)prop->data, prop->save_addr);
        break;
    case A3_U16:
        Save16(*(uint16_t *)prop->data, prop->save_addr);
        break;
    case A3_U32:
        Save32(*(uint32_t *)prop->data, prop->save_addr);
        break;
    }
}

void CommitString(a3_property_t *prop, uint8_t *data, uint8_t len)
{
    size_t data_len = MIN(len, A3_MAX_CONFIG_DATA_LENGTH - 1);
    memcpy(prop->data, data, data_len);
    ((char *)prop->data)[data_len] = 0;
    if (prop->save_addr == ADDR_UNSET) {
        return;
    }
    SaveString((const char *)prop->data, A3_MAX_CONFIG_DATA_LENGTH, prop->save_addr);
}

void CommitVectorU8(a3_property_t *prop, uint8_t *data, uint8_t len)
{
    a3_vector_t *value = (a3_vector_t *)prop->data;
    size_t size = MIN(value->size, len);
    memcpy(value->data, data, size);
    if (prop->save_addr == ADDR_UNSET) {
        return;
    }
    EEPROM_UpdateTemperature();
    uint8_t *elements = (uint8_t *)value->data;
    for (size_t i = 0; i < size; ++i) {
        while (CYRET_LOCKED == EEPROM_WriteByte(elements[i], prop->save_addr + i)) {}
    }
}

/* [] END OF FILE */

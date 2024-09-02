/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "probe_api/common.h"
#include "probe_api/result.h"
#include "probe_api/time.h"

#define MAX_REPLY_LENGTH      2000
#define MAX_PACKETS           10

bool in_packet;
uint32_t packet_idx;
uint32_t packet_data_idx;
bool packet_send[MAX_PACKETS];
char packet_data[MAX_PACKETS][MAX_REPLY_LENGTH];

void mocks_init(void)
{
    int i;
    for(i = 0; i < MAX_PACKETS; i++)
    {
        packet_send[i] = false;
    }
    packet_idx = 0;
    packet_data_idx = 0;
    in_packet = false;
}

char* get_send_packet(int num)
{
    return &(packet_data[num][0]);
}

int get_num_send_packets(void)
{
    int res = 0;
    int i;
    for(i = 0; i < MAX_PACKETS; i++)
    {
        if(true == packet_send[i])
        {
            res++;
        }
    }
    return res;
}

void send_part(char* part, uint32_t size, uint32_t offset, uint32_t length)
{
    (void)part;
    (void)size;
    (void)offset;
    (void)length;
}

bool common_cmd_target_info(uint32_t loop)
{
    (void)loop;
    return true;
}

void reply_packet_prepare(void)
{
    in_packet = true;
    packet_data_idx = 0;
}

void reply_packet_add(char* data)
{
    if(NULL == data)
    {
        return;
    }
    int len = strlen(data);
    memcpy(&(packet_data[packet_idx][packet_data_idx]), data, len);
    packet_data_idx += len;
}

void reply_packet_send(void)
{
    packet_data[packet_idx][packet_data_idx] = 0;
    packet_send[packet_idx] = true;
    packet_idx++;
    if(MAX_PACKETS == packet_idx)
    {
        packet_idx = 0;
    }
    packet_data_idx = 0;
}

void mon_cmd_version(void)
{

}

void encode_text_to_hex_string(char * text, uint32_t buf_length, char * buf)
{
    (void)text;
    (void)buf_length;
    (void)buf;
}

void gdb_is_not_busy_anymore(void)
{

}

bool add_action(action_typ act)
{
    (void)act;
    return false;
}

Result do_connect(action_data_typ* const action)
{
    (void)action;
    return ERR_WRONG_VALUE;
}

Result do_disconnect(action_data_typ* const action)
{
    (void)action;
    return ERR_WRONG_VALUE;
}

Result do_read_ap_reg(action_data_typ* const action, uint32_t bank, uint32_t reg)
{
    (void)action;
    (void)bank;
    (void)reg;
    return ERR_WRONG_VALUE;
}

Result do_write_ap_reg(action_data_typ* const action, uint32_t bank, uint32_t reg, uint32_t data)
{
    (void)action;
    (void)bank;
    (void)reg;
    (void)data;
    return ERR_WRONG_VALUE;
}

Result do_read_ap(action_data_typ* const action, uint32_t address)
{
    (void)action;
    (void)address;
    return ERR_WRONG_VALUE;
}

Result do_write_ap(action_data_typ* const action, uint32_t address, uint32_t data)
{
    (void)action;
    (void)address;
    (void)data;
    return ERR_WRONG_VALUE;
}

Result do_get_Result_OK(action_data_typ* const action)
{
    (void)action;
    return ERR_WRONG_VALUE;
}

Result do_get_Result_data(action_data_typ* const action)
{
    (void)action;
    return ERR_WRONG_VALUE;
}

void mon_cmd_help(char* command)
{
    (void)command;
}

bool add_action_with_parameter(action_typ act, parameter_typ* parsed_parameter)
{
    (void)act;
    (void)parsed_parameter;
    return false;
}

void div_and_mod(uint32_t divident, uint32_t divisor, uint32_t* quotient, uint32_t* remainder)
{
    *remainder = divident%divisor;
    *quotient = divident/divisor;
}

uint8_t* cli_get_parameter(const uint32_t parameter_index)
{
	(void)parameter_index;
	return NULL;
}

void start_timeout(timeout_typ* to, const uint32_t time_ms)
{
	(void)to;
	(void)time_ms;
}

bool timeout_expired(timeout_typ* to)
{
	(void)to;
	return true;
}

action_data_typ * book_action_slot(void)
{
	return NULL;
}

void swd_protocol_set_AP_sel(uint32_t val)
{
	(void)val;
}

Result add_target_action(action_data_typ * const action)
{
	(void)action;
	return ERR_WRONG_STATE;
}

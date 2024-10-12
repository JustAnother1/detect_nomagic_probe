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
#include <string.h>
#include <stdlib.h>
#include "probe_api/result.h"
#include "probe_api/debug_log.h"
#include "probe_api/swd.h"
#include "probe_api/cli.h"
#include "probe_api/time.h"
#include "probe_api/common.h"
#include "target.h"

#define TIMEOUT_TIME_MS       10000


// RP2040:
// Core 0: 0x01002927
// Core 1: 0x11002927
// Rescue DP: 0xf1002927
// Core0 and Core1 instance IDs can be changed
// so to be sure check also these end points:
// 0x21002927, 0x31002927, 0x41002927, 0x51002927, 0x61002927, 0x71002927
// 0x81002927, 0x91002927, 0xa1002927, 0xb1002927, 0xc1002927, 0xd1002927,
// 0xe1002927

// decoded:
// IDCODE:
// bit 0     = 1;
// bit 1-11  = Designer (JEP106)
// bit 27-12 = PartNo
// bit 28-31 = Version (implementation defined)

// Part Number : 0x1002
// Designer = Raspberry Pi Trading Ltd.
// JEP 106 = 9x 0x7f then 0x13

typedef struct{
    uint32_t target_id;
    uint32_t apsel;
} connect_param_typ;

static connect_param_typ connect_parameter[] = {
        // targetID | APSel |
        {0x01002927,      0 }, // RP2040 core 0
        {0x11002927,      0 }, // RP2040 core 1
        {0x21002927,      0 }, // RP2040 altered id
        {0x31002927,      0 }, // RP2040 altered id
        {0x41002927,      0 }, // RP2040 altered id
        {0x51002927,      0 }, // RP2040 altered id
        {0x61002927,      0 }, // RP2040 altered id
        {0x71002927,      0 }, // RP2040 altered id
        {0x81002927,      0 }, // RP2040 altered id
        {0x91002927,      0 }, // RP2040 altered id
        {0xa1002927,      0 }, // RP2040 altered id
        {0xb1002927,      0 }, // RP2040 altered id
        {0xc1002927,      0 }, // RP2040 altered id
        {0xd1002927,      0 }, // RP2040 altered id
        {0xe1002927,      0 }, // RP2040 altered id
        {0xf1002927,      0 }, // RP2040 rescue data port
};

#define NUM_CONNECT_LOCATIONS (sizeof(connect_parameter)/sizeof(connect_param_typ))

static bool test_swd_v1(void);
static bool test_swd_v2(void);
static Result start_scan(void);
static Result start_connect(bool isSWDv2, uint32_t core_id, uint32_t APsel);

static bool checked_swdv1;
static bool checked_swdv2;
static bool single_location;
static uint32_t step;
static uint32_t location;
static timeout_typ to;

static bool swd_isSWDv2 = false;
static uint32_t swd_core_id = 0;
static uint32_t swd_APsel = 0;

static action_data_typ* cur_action;


bool cmd_target_info(uint32_t loop)
{
    if(0 == loop)
    {
        debug_line("Target Status");
        debug_line("=============");
        debug_line("target: no target");
    }
    else
    {
        return common_cmd_target_info(loop -1);
    }
    return false; // true == Done; false = call me again
}

bool target_is_SWDv2(void)
{
    return swd_isSWDv2;
}

uint32_t target_get_SWD_core_id(uint32_t core_num) // only required for SWDv2 (TARGETSEL)
{
    (void) core_num;
    return swd_core_id;
}

uint32_t target_get_SWD_APSel(uint32_t core_num)
{
    (void) core_num;
    return swd_APsel;
}

// get called from CLI.
// loop is 0 on first call and will increase by 1 on each following call.
// return false means we are not finished,
// return true means we are finished.
// returning false leads to this function being called again by CLI
bool cmd_swd_test(uint32_t loop)
{
    if(0 == loop)
    {
        uint8_t* para_str = cli_get_parameter(0);
        start_timeout(&to, TIMEOUT_TIME_MS);
        checked_swdv1 = false;
        checked_swdv2 = false;
        step = 0;
        location = 0;
        single_location = false;
        cur_action = NULL;
        if('1' == *para_str)
        {
            // SWDv1 only
            checked_swdv2 = true;
        }
        else if('2' == *para_str)
        {
            // SWDv2 only
            checked_swdv1 = true;
            para_str = cli_get_parameter(1);
            if(NULL != para_str)
            {
                location = (uint32_t)atoi((char*)para_str);
                single_location = true;
            }
        }
        return false; // not done yet
    }
    else
    {
        // check timeout
        if(true == timeout_expired(&to))
        {
            // Timeout !!!
            debug_line("ERROR: detect loop TIMEOUT !!!!");
            return true;
        }

        if(NULL != cur_action)
        {
            if(false == cur_action->is_done)
            {
                return false; // not done yet
            }
        }
        if(false == checked_swdv1)
        {
            // check if we can connect using SWDv1
            return test_swd_v1();
        }
        else if(false == checked_swdv2)
        {
            // check if we can connect using SWDv1
            return test_swd_v2();
        }
        else
        {
            return true; // we are done now
        }
    }
}

static bool test_swd_v1(void)
{
    // open SWD connection
    if(NULL != cur_action)
    {
        if(RESULT_OK != cur_action->result)
        {
            debug_line("SWDv1: Failed to connect!");
            step = 0;
            checked_swdv1 = true;
            cur_action = NULL;
            return false;
        }
    }
    if(0 == step)
    {
        Result res;
        debug_line(" ");
        debug_line("trying to connect using SWDv1 ....");
        res = start_connect(false, 0, 0);
        if(RESULT_OK != res)
        {
            debug_line("ERROR: failed to connect using SWDv1 !");
            return true;
        }
        else
        {
            step = 1;
            return false;
        }
    }
    else if(1 == step)
    {
        if(RESULT_OK == cur_action->result)
        {
            Result res;
            res = start_scan();
            if(RESULT_OK != res)
            {
                debug_line("ERROR: failed to scan target using SWDv1 !");
                return true;
            }
            else
            {
                step = 2;
            }
        }
        else
        {
            debug_line("ERROR: SWDv1: failed to connect!");
            checked_swdv1 = true;
            step = 0;
        }
        return false;
    }
    else // if(2 == step)
    {
        debug_line("Done with SWDv1 !");
        checked_swdv1 = true;
        step = 0;
        cur_action = NULL;
        return false;
    }
}

static bool test_swd_v2(void)
{
    // open SWD connection
    if(0 == step)
    {
        if(NUM_CONNECT_LOCATIONS > location)
        {
            Result res;
            debug_line(" ");
            debug_line("SWDv2: trying to connect on location %ld/%d (target id: 0x%08lx)....",
                    location + 1, NUM_CONNECT_LOCATIONS, connect_parameter[location].target_id);
            res = start_connect(true,
                                connect_parameter[location].target_id,
                                connect_parameter[location].apsel);
            if(RESULT_OK != res)
            {
                debug_line("ERROR: failed to connect using SWDv2 !");
                return true;
            }
            else
            {
                step = 1;
                return false;
            }
        }
        else
        {
            // scanned all location -> we are done
            debug_line("Done !");
            return true;
        }
        return false;
    }
    else if(1 == step)
    {
        if(RESULT_OK == cur_action->result)
        {
            Result res;
            res = start_scan();
            if(RESULT_OK != res)
            {
                debug_line("ERROR: failed to scan target using SWDv1 !");
                return true;
            }
            else
            {
                step = 2;
            }
        }
        else
        {
            debug_line("ERROR: connect failed (%ld) !", cur_action->result);
            location++;
            step = 0;
            if(true == single_location)
            {
                return true;
            }
        }
        return false;
    }
    else // if(2 == step)
    {
        if(RESULT_OK == cur_action->result)
        {
            location++;
            step = 0;
            if(true == single_location)
            {
                return true;
            }
            return false;
        }
        else
        {
            debug_line("ERROR: scan failed (%ld) !", cur_action->result);
            location++;
            step = 0;
            if(true == single_location)
            {
                return true;
            }
            return false;
        }
    }
}

static Result start_scan(void)
{
    if(false == add_action(SWD_SCAN))
    {
        return ERR_QUEUE_FULL_TRY_AGAIN;
    }
    else
    {
        return RESULT_OK;
    }
}

static Result start_connect(bool isSWDv2, uint32_t core_id, uint32_t APsel)
{
    swd_isSWDv2 = isSWDv2;
    swd_core_id = core_id;
    swd_APsel = APsel;

    if(false == add_action(SWD_CONNECT))
    {
        return ERR_QUEUE_FULL_TRY_AGAIN;
    }
    else
    {
        return RESULT_OK;
    }
}


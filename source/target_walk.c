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

#include "target_walk.h"
#include "swd.h"
#include "debug_log.h"

void handle_scan(walk_data_typ* data);
static Result check_AP(uint32_t idr, bool first_call, uint32_t * phase, walk_data_typ* data);


static uint32_t ROM_Table_Adress;

void handle_scan(walk_data_typ* data)
{
    if(0 == data->phase)
    {
        data->phase++;
        data->intern_0 = 0; // AP address
    }
    else if(1 == data->phase)
    {
        debug_line("testing AP %ld", data->intern_0);
        swd_protocol_set_AP_sel(data->intern_0);
        // data->res = read_ap_register(AP_BANK_IDR, AP_REGISTER_IDR, &(data->read_0), true);
        data->cur_step.type = STEP_AP_REG_READ;
        data->cur_step.par_i_0 = AP_BANK_IDR;
        data->cur_step.par_i_1 = AP_REGISTER_IDR;
        data->cur_step.phase = 0;
        data->cur_step.result = RESULT_OK;
        data->cur_step.is_done = false;
        data->phase++;
    }
    else if((2 == data->phase) || (3 == data->phase))
    {
        if(RESULT_OK == data->cur_step.result)
        {
            // found an AP
            Result tres;
            if(2 == data->phase)
            {
                if(0 != data->cur_step.read_0)
                {
                    debug_line("Found AP !");
                    data->intern_1 = data->cur_step.read_0;
                    tres = check_AP(data->intern_1, true, &(data->sub_phase), data);
                    data->phase = 3;
                }
                else
                {
                    // no more APs in this device
                    debug_line("AP %ld: IDR = 0", data->intern_0);
                    swd_protocol_set_AP_sel(data->intern_0 -1); // use the last good AP
                    debug_line("Done!");
                    data->result = RESULT_OK;
                    data->is_done = true;
                    tres = RESULT_OK;
                }
            }
            else
            {
                tres = check_AP(data->intern_1, false, &(data->sub_phase), data);
            }
            if(RESULT_OK == tres)
            {
                data->intern_0++;
                if(256 > data->intern_0)
                {
                    data->phase = 1;
                }
                else
                {
                    // we checked all possible AP
                    data->result = RESULT_OK;
                    data->is_done = true;
                }
            }
            else if(ERR_NOT_COMPLETED == tres)
            {
                // try again
            }
            else
            {
                // step failed
                data->result = data->cur_step.result;
                data->is_done = true;
            }
        }
        else
        {
            // step failed
            data->result = data->cur_step.result;
            data->is_done = true;
        }
    }
}

static Result check_AP(uint32_t idr, bool first_call, uint32_t * phase, walk_data_typ* data)
{
    // static Result phase = 0;
    uint32_t class = (idr & (0xf << 13))>> 13;
    if(8 == class)
    {
        // Memory Access Port (MEM-AP)
        if(true == first_call)
        {
            debug_line("APv1:");
            debug_line("AP: IDR: Revision: %ld", (idr & (0xful<<28))>>28 );
            debug_line("AP: IDR: Jep 106 : %ld x 0x7f + 0x%02lx", (idr & (0xf << 24))>>24, (idr & (0x7f<<17))>>17 );
            debug_line("AP: IDR: class :   %ld", class );
            debug_line("AP: IDR: variant:  %ld", (idr & (0xf<<4))>>4 );
            debug_line("AP: IDR: type:     %ld", (idr & 0xf) );
            *phase = 1;
            return ERR_NOT_COMPLETED;
        }
        else if(1 == *phase)
        {
            data->cur_step.type = STEP_AP_REG_READ;
            data->cur_step.par_i_0 = AP_BANK_CSW;
            data->cur_step.par_i_1 = AP_REGISTER_CSW;
            data->cur_step.phase = 0;
            data->cur_step.result = RESULT_OK;
            data->cur_step.is_done = false;
            *phase = 2;
            return ERR_NOT_COMPLETED;
        }
        else if(2 == *phase)
        {
            if(RESULT_OK == data->cur_step.result)
            {
                // found an AP
                debug_line("AP: CSW  : 0x%08lx", data->cur_step.read_0);

                // change CSW !!!
                data->cur_step.read_0 = data->cur_step.read_0 & ~0x3ful; // no auto address increment
                data->cur_step.read_0 = data->cur_step.read_0 | 0x80000002; // DbgSwEnable + data size = 32bit

                // write CSW
                data->cur_step.type = STEP_AP_REG_WRITE;
                data->cur_step.par_i_0 = AP_BANK_CSW;
                data->cur_step.par_i_1 = AP_REGISTER_CSW;
                data->cur_step.par_i_1 = data->cur_step.read_0;
                data->cur_step.read_0 = 0;
                data->cur_step.phase = 0;
                data->cur_step.result = RESULT_OK;
                data->cur_step.is_done = false;
                *phase = 3;
                return ERR_NOT_COMPLETED;
            }
            else
            {
                // step failed
                debug_line("Failed to read CSW (%ld) !", data->cur_step.result);
                return data->cur_step.result;
            }
        }

        else if(3 == *phase)
        {
            if(RESULT_OK == data->cur_step.result)
            {
                data->cur_step.type = STEP_AP_REG_READ;
                data->cur_step.par_i_0 = AP_BANK_BASE;
                data->cur_step.par_i_1 = AP_REGISTER_BASE;
                data->cur_step.phase = 0;
                data->cur_step.result = RESULT_OK;
                data->cur_step.is_done = false;
                *phase = 4;
                return ERR_NOT_COMPLETED;
            }
            else
            {
                // step failed
                debug_line("Failed to write CSW (%ld) !", data->cur_step.result);
                return data->cur_step.result;
            }
        }

        else if(4 == *phase)
        {
            if(RESULT_OK == data->cur_step.result)
            {
                debug_line("AP: BASE : 0x%08lx", data->cur_step.read_0);
                ROM_Table_Adress = data->cur_step.read_0 & 0xfffffffc; // lowest two bits of address must be  0. (4 Byte = 32 bit alignment)
                debug_line("AP: ROM Table starts at 0x%08lx", ROM_Table_Adress);
                data->cur_step.type = STEP_AP_REG_READ;
                data->cur_step.par_i_0 = AP_BANK_CFG;
                data->cur_step.par_i_1 = AP_REGISTER_CFG;
                data->cur_step.phase = 0;
                data->cur_step.result = RESULT_OK;
                data->cur_step.is_done = false;
                *phase = 5;
                return ERR_NOT_COMPLETED;
            }
            else
            {
                // step failed
                debug_line("Failed to read BASE (%ld) !", data->cur_step.result);
                return data->cur_step.result;
            }
        }

        else if(5 == *phase)
        {
            if(RESULT_OK == data->cur_step.result)
            {
                bool val;
                debug_line("AP: CFG  : 0x%08lx", data->cur_step.read_0);
                if(0 == (data->cur_step.read_0 & 0x02))
                {
                    val = false;
                }
                else
                {
                    val = true;
                }
                debug_line("long address supported = %d", val);

                if(0 == (data->cur_step.read_0 & 0x04))
                {
                    val = false;
                }
                else
                {
                    val = true;
                }
                debug_line("large data supported = %d", val);

                data->cur_step.type = STEP_AP_REG_READ;
                data->cur_step.par_i_0 = AP_BANK_CFG1;
                data->cur_step.par_i_1 = AP_REGISTER_CFG1;
                data->cur_step.phase = 0;
                data->cur_step.result = RESULT_OK;
                data->cur_step.is_done = false;
                data->intern_0 = 0;
                *phase = 6;
                return ERR_NOT_COMPLETED;
            }
            else
            {
                // step failed
                debug_line("Failed to read CFG (%ld) !", data->cur_step.result);
                return data->cur_step.result;
            }
        }

        else if(6 == *phase)
        {
            if(RESULT_OK == data->cur_step.result)
            {
                debug_line("AP: CFG1 : 0x%08lx", data->cur_step.read_0);
                data->cur_step.type = STEP_AP_READ;
                data->cur_step.par_i_0 = ROM_Table_Adress;
                data->cur_step.phase = 0;
                data->cur_step.result = RESULT_OK;
                data->cur_step.is_done = false;
                data->intern_0 = 0;
                *phase = 7;
                return ERR_NOT_COMPLETED;
            }
            else
            {
                // step failed
                debug_line("Failed to read CFG1 (%ld) !", data->cur_step.result);
                return data->cur_step.result;
            }
        }

        else if(7 == *phase)
        {
            if(RESULT_OK == data->cur_step.result)
            {
                if(0 == data->cur_step.read_0)
                {
                    // end of ROM Table found
                    *phase = 8;
                    return ERR_NOT_COMPLETED;
                }
                if(1 == (data->cur_step.read_0 &1))
                {
                    // valid entry
                    int32_t address = (int32_t)(data->cur_step.read_0 & 0xfffff000);  // address offset is signed
                    debug_line("ROM Table : found 0x%08lx", address);
                    debug_line("ROM Table : address 0x%08lx", (int32_t)ROM_Table_Adress + address);
                }
                else
                {
                    debug_line("ROM Table : ignoring 0x%08lx", data->cur_step.read_0);
                }
                data->intern_0++;
                if(1024 < data->intern_0)
                {
                    // max size
                    *phase = 8;
                    return ERR_NOT_COMPLETED;
                }

                // read next entry
                debug_line("AP: CFG1 : 0x%08lx", data->cur_step.read_0);
                data->cur_step.type = STEP_AP_READ;
                data->cur_step.par_i_0 = ROM_Table_Adress + (data->intern_0 * 4);
                data->cur_step.phase = 0;
                data->cur_step.result = RESULT_OK;
                data->cur_step.is_done = false;
                return ERR_NOT_COMPLETED;

                // ROM Table:
                // ==========
                // each 32bit Entry is defined as:
                // bit 31-12: Address Offset _signed_ base address offset relative to the ROM table base address
                // bit 11-2: reserved
                // bit 1: Format: 0= 8 bit format(not used); 1= 32bit format.
                // bit 0: Entry present: 0= ignore this entry; 1= valid entry
                // entry 0x00000000 marks end of table!

                // actual table: (rp2040 @ 0xe00ff003) = BASE & 0xfffffffc; // lowest two bits are 0.
                // offset, name,   value,                    description
                // ------  ----    -----                     -----------
                // 0,      SCS,    0xfff0f003,               points to the SCS @ 0xe000e000.
                // 4,      ROMDWT, 0xfff02002 or 0xfff02003, points to DWT @ 0xe0001000 bit0 is 1 if DWT is fitted.
                // 8,      ROMBPU, 0xfff03002 or 0xfff03003, points to the BPU @ 0xe0002000 bit 0 is set if BPU is fitted.
                // 0xc,    End,    0x00000000,               End of table marker.


                 // i = 0
                 // do{
                 //  read memory @ Base & 0xfffffffc + 4*i
                 //  i = 0 -> SCS
                 //  i = 1 -> ROMDWT
                 //  i = 2 -> ROMBPU
                 // } while(i < 1024 and read != 0)

                // TODO read ROM Table from address pointed to by BASE Register (Architecture P 318)
                // TODO check number of Break Points
                // TODO check number of Watch points

            }
            else
            {
                // step failed
                debug_line("Failed to read ROM Table (%ld) !", data->cur_step.result);
                return data->cur_step.result;
            }
        }

        else if(8 == *phase)
        {
            debug_line("Done with this AP!");
            return RESULT_OK; // done with this AP
        }
        else
        {
            // phase has invalid value
            debug_line("check ap: invalid phase (%ld)!", *phase);
            return ERR_WRONG_STATE;
        }
    }
    else if(9 == class)
    {
        // Memory Access Port (MEM-AP)
        debug_line("APv2:");
        debug_line("AP: IDR: Revision: %ld", (idr & (0xful<<28))>>28 );
        debug_line("AP: IDR: Jep 106 : %ld x 0x7f + 0x%02lx", (idr & (0xf << 24))>>24, (idr & (0x7f<<17))>>17 );
        debug_line("AP: IDR: class :   %ld", class );
        debug_line("AP: IDR: variant:  %ld", (idr & (0xf<<4))>>4 );
        debug_line("AP: IDR: type:     %ld", (idr & 0xf) );
        // TODO

        return RESULT_OK; // done with this AP
    }
    else
    {
        debug_line("AP unknown class %ld !", class);
        return RESULT_OK; // done with this AP
    }
}


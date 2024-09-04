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

#include "target_scan.h"
#include "probe_api/swd.h"
#include "probe_api/debug_log.h"
#include "probe_api/steps.h"

#define INTERN_AP_ADDRESS     1
#define INTERN_IDR            2
#define INTERN_ROM_OFFSET     3

static uint32_t ROM_Table_Adress;
static uint32_t SCS_Adress;
static uint32_t DWT_Adress;
static uint32_t BPU_Adress;

static Result check_AP(uint32_t idr, bool first_call, action_data_typ * const action);


Result handle_scan(action_data_typ * const action, bool first_call)
{
    if(true == first_call)
    {
        *(action->cur_phase) = 1;
        action->intern[INTERN_AP_ADDRESS] = 0;
    }

    if(1 == *(action->cur_phase))
    {
        debug_line("testing AP %ld", action->intern[INTERN_AP_ADDRESS]);
        swd_protocol_set_AP_sel(action->intern[INTERN_AP_ADDRESS]);
        return do_read_ap_reg(action, AP_BANK_IDR, AP_REGISTER_IDR);
    }
    else if(2 == *(action->cur_phase))
    {
        return do_get_Result_data(action);
    }
    else if((3 == *(action->cur_phase)) || (4 == *(action->cur_phase)))
    {
        if(RESULT_OK == action->result)
        {
            // found an AP
            Result tres;
            if(3 == *(action->cur_phase))
            {
                if(0 != action->read_0)
                {
                    debug_line("Found AP !");
                    action->intern[INTERN_IDR] = action->read_0;
                    action->cur_phase = &(action->sub_phase);
                    tres = check_AP(action->intern[INTERN_IDR], true,action);
                    action->cur_phase = &(action->main_phase);
                    *(action->cur_phase) = 4;
                }
                else
                {
                    // no more APs in this device
                    debug_line("AP %ld: IDR = 0", action->intern[INTERN_AP_ADDRESS]);
                    swd_protocol_set_AP_sel(action->intern[INTERN_AP_ADDRESS] -1); // use the last good AP
                    debug_line("Done!");
                    action->result = RESULT_OK;
                    action->is_done = true;
                    return RESULT_OK;
                }
            }
            else
            {
                action->cur_phase = &(action->sub_phase);
                tres = check_AP(action->intern[INTERN_IDR], false, action);
                action->cur_phase = &(action->main_phase);
            }
            if(RESULT_OK == tres)
            {
                action->intern[INTERN_AP_ADDRESS]++;
                if(256 > action->intern[INTERN_AP_ADDRESS])
                {
                    *(action->cur_phase) = 1;
                    return ERR_NOT_COMPLETED;
                }
                else
                {
                    // we checked all possible AP
                    action->result = RESULT_OK;
                    action->is_done = true;
                    return RESULT_OK;
                }
            }
            else if(ERR_NOT_COMPLETED == tres)
            {
                // try again
                return ERR_NOT_COMPLETED;
            }
            else
            {
                // step failed
                action->is_done = true;
                return action->result;
            }
        }
        else
        {
            // step failed
            action->is_done = true;
            return action->result;
        }
    }

    return ERR_WRONG_STATE;
}

static Result check_AP(uint32_t idr, bool first_call, action_data_typ * const action)
{
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
            *(action->cur_phase) = 1;
            return ERR_NOT_COMPLETED;
        }

        else if(1 == *(action->cur_phase))
        {
            return do_read_ap_reg(action, AP_BANK_CSW, AP_REGISTER_CSW);
        }

        else if(2 == *(action->cur_phase))
        {
            return do_get_Result_data(action);
        }

        else if(3 == *(action->cur_phase))
        {
            if(RESULT_OK == action->result)
            {
                // found an AP
                debug_line("AP: CSW  : 0x%08lx", action->read_0);

                // change CSW !!!
                action->read_0 = action->read_0 & ~0x3ful; // no auto address increment
                action->read_0 = action->read_0 | 0x80000002; // DbgSwEnable + data size = 32bit

                // write CSW
                return do_write_ap_reg(action, AP_BANK_CSW, AP_REGISTER_CSW, action->read_0);
            }
            else
            {
                // step failed
                debug_line("Failed to read CSW (%ld) !", action->result);
                return action->result;
            }
        }

        else if(4 == *(action->cur_phase))
        {
            return do_read_ap_reg(action, AP_BANK_BASE, AP_REGISTER_BASE);
        }

        else if(5 == *(action->cur_phase))
        {
            return do_get_Result_data(action);
        }

        else if(6 == *(action->cur_phase))
        {
            if(RESULT_OK == action->result)
            {
                debug_line("AP: BASE : 0x%08lx", action->read_0);
                // lowest two bits of address must be  0. (4 Byte = 32 bit alignment)
                ROM_Table_Adress = action->read_0 & 0xfffffffc;
                debug_line("AP: ROM Table starts at 0x%08lx", ROM_Table_Adress);
                return do_read_ap_reg(action, AP_BANK_CFG, AP_REGISTER_CFG);
            }
            else
            {
                // step failed
                debug_line("Failed to read BASE (%ld) !", action->result);
                return action->result;
            }
        }

        else if(7 == *(action->cur_phase))
        {
            return do_get_Result_data(action);
        }

        else if(8 == *(action->cur_phase))
        {
            if(RESULT_OK == action->result)
            {
                bool val;
                debug_line("AP: CFG  : 0x%08lx", action->read_0);
                if(0 == (action->read_0 & 0x02))
                {
                    val = false;
                }
                else
                {
                    val = true;
                }
                debug_line("long address supported = %d", val);

                if(0 == (action->read_0 & 0x04))
                {
                    val = false;
                }
                else
                {
                    val = true;
                }
                debug_line("large data supported = %d", val);

                return do_read_ap_reg(action, AP_BANK_CFG1, AP_REGISTER_CFG1);
            }
            else
            {
                // step failed
                debug_line("Failed to read CFG (%ld) !", action->result);
                return action->result;
            }
        }

        else if(9 == *(action->cur_phase))
        {
            return do_get_Result_data(action);
        }

        else if(10 == *(action->cur_phase))
        {
            if(RESULT_OK == action->result)
            {
                debug_line("AP: CFG1 : 0x%08lx", action->read_0);
                action->intern[INTERN_ROM_OFFSET] = 1;
                return do_read_ap(action, ROM_Table_Adress);
            }
            else
            {
                // step failed
                debug_line("Failed to read CFG1 (%ld) !", action->result);
                return action->result;
            }
        }

        else if(11 == *(action->cur_phase))
        {
            return do_get_Result_data(action);
        }

        else if(12 == *(action->cur_phase))
        {
            if(RESULT_OK == action->result)
            {
                if(0 == action->read_0)
                {
                    // end of ROM Table found
                    *(action->cur_phase) = 15;
                    return ERR_NOT_COMPLETED;
                }

                if(1 == (action->read_0 &1))
                {
                    // valid entry
                    int32_t address = (int32_t)(action->read_0 & 0xfffff000);  // address offset is signed
					SCS_Adress = (uint32_t)address;
					debug_line("System Control Space (SCS):");
                    debug_line("ROM Table[0] : found 0x%08lx", action->read_0);
                    debug_line("ROM Table[0] : address 0x%08lx", (int32_t)ROM_Table_Adress + address);
                }
                else
                {
					SCS_Adress = 0;
					debug_line("System Control Space (SCS):");
                    debug_line("ROM Table[0] : ignoring 0x%08lx", action->read_0);
                }

                // read next entry
                return do_read_ap(action, ROM_Table_Adress + (action->intern[INTERN_ROM_OFFSET] * 4));

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
                debug_line("Failed to read ROM Table (%ld) !", action->result);
                return action->result;
            }
        }

        else if(13 == *(action->cur_phase))
        {
            return do_get_Result_data(action);
        }

        else if(14 == *(action->cur_phase))
        {
            if(0 == action->read_0)
            {
                // end of ROM Table found
                *(action->cur_phase) = 15;
                return ERR_NOT_COMPLETED;
            }
            if(1 == (action->read_0 &1))
            {
                // valid entry
                int32_t address = (int32_t)(action->read_0 & 0xfffff000);  // address offset is signed

                switch(action->intern[INTERN_ROM_OFFSET])
                {
                case 1: // Data watchpoint unit (DWT)
                	DWT_Adress = ROM_Table_Adress + (uint32_t)address;
                	debug_line("Data watchpoint unit (DWT):");
                	break;

                case 2: // Breakpoint unit (BPU)
                	BPU_Adress = ROM_Table_Adress + (uint32_t)address;
                	debug_line("Breakpoint unit (BPU):");
                	break;

                default:
                	debug_line("Rom Offset %ld:", action->intern[INTERN_ROM_OFFSET]);
                	break;
                }

                debug_line("ROM Table[%ld] : found 0x%08lx", action->intern[INTERN_ROM_OFFSET], address);
                debug_line("ROM Table[%ld] : address 0x%08lx", action->intern[INTERN_ROM_OFFSET], (int32_t)ROM_Table_Adress + address);
            }
            else
            {
                switch(action->intern[INTERN_ROM_OFFSET])
                {
                case 1: // Data watchpoint unit (DWT)
                	DWT_Adress = 0;
                	debug_line("Data watchpoint unit (DWT):");
                	break;

                case 2: // Breakpoint unit (BPU)
                	BPU_Adress = 0;
                	debug_line("Breakpoint unit (BPU):");
                	break;
                }
                debug_line("ROM Table[%ld] : ignoring 0x%08lx", action->intern[INTERN_ROM_OFFSET], action->read_0);
            }
            *(action->cur_phase) = 12;
            action->intern[INTERN_ROM_OFFSET] = action->intern[INTERN_ROM_OFFSET] + 1;
            return do_read_ap(action, ROM_Table_Adress + (action->intern[INTERN_ROM_OFFSET] * 4));
        }

        // Data watchpoint unit (DWT)
        else if(15 == *(action->cur_phase))
        {
        	if(0 != DWT_Adress)
        	{
        		return do_read_ap(action, DWT_Adress);
        	}
        	else
        	{
        		*(action->cur_phase) = 18;
        		return ERR_NOT_COMPLETED;
        	}
        }

        else if(16 == *(action->cur_phase))
        {
            return do_get_Result_data(action);
        }

        else if(17 == *(action->cur_phase))
        {
            if(RESULT_OK == action->result)
            {
                uint32_t num = (action->read_0 >>28);
                debug_line("Device has %ld watchpoints.", num);
				*(action->cur_phase) = 18;
				return ERR_NOT_COMPLETED;
            }
            else
            {
                // step failed
                debug_line("Failed to read DWT_CTRL (%ld) !", action->result);
                return action->result;
            }
        }

        // Breakpoint unit (BPU)
        else if(18 == *(action->cur_phase))
        {
        	if(0 != BPU_Adress)
        	{
        		return do_read_ap(action, BPU_Adress);
        	}
        	else
        	{
        		*(action->cur_phase) = 21;
        		return ERR_NOT_COMPLETED;
        	}
        }

        else if(19 == *(action->cur_phase))
        {
            return do_get_Result_data(action);
        }

        else if(20 == *(action->cur_phase))
        {
            if(RESULT_OK == action->result)
            {
                uint32_t num = ((action->read_0 >>4) & 0xf);
                debug_line("Device has %ld breakpoints.", num);
				*(action->cur_phase) = 21;
				return ERR_NOT_COMPLETED;
            }
            else
            {
                // step failed
                debug_line("Failed to read BP_CTRL (%ld) !", action->result);
                return action->result;
            }
        }

        else if(21 == *(action->cur_phase))
        {
            debug_line("Done with this AP!");
            return RESULT_OK; // done with this AP
        }

        else
        {
            // phase has invalid value
            debug_line("check ap: invalid phase (%ld)!", *(action->cur_phase));
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

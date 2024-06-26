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

#ifndef SOURCE_CFG_TARGET_CLI_COMMANDS_H_
#define SOURCE_CFG_TARGET_CLI_COMMANDS_H_

#include <stdint.h>

bool cmd_target_info(uint32_t loop);
bool cmd_swd_test(uint32_t loop);

#define TARGET_CLI_COMMANDS \
{"swd_test",    "explore the SWD interface", cmd_swd_test}, \


#endif /* SOURCE_CFG_TARGET_CLI_COMMANDS_H_ */

/*
 * target.h
 *
 *  Created on: 18.05.2024
 *      Author: lars
 */

#ifndef SOURCE_TARGET_H_
#define SOURCE_TARGET_H_

#include <stdint.h>
#include "probe_api/common.h"

bool target_is_SWDv2(void);
uint32_t target_get_SWD_core_id(uint32_t core_num); // only required for SWDv2 (TARGETSEL)
uint32_t target_get_SWD_APSel(uint32_t core_num);


#endif /* SOURCE_TARGET_H_ */

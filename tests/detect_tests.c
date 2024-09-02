#include <stdio.h>
#include "detect_tests.h"
#include "mocks.h"


void* detect_setup(const MunitParameter params[], void* user_data)
{
    (void)params;
    (void)user_data;
    mocks_init();
    return NULL;
}

MunitResult test_swd_v2(const MunitParameter params[], void* user_data)
{
    // Objective: RP2040 is SWD v2 not v1
    (void) params;
    (void) user_data;
    munit_assert_false(target_is_SWDv2());
    return MUNIT_OK;
}

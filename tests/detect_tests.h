#ifndef TESTS_CLI_TESTS_H_
#define TESTS_CLI_TESTS_H_

#include "munit.h"
#include <stdint.h>
#include <stdbool.h>

void* detect_setup(const MunitParameter params[], void* user_data);
MunitResult test_swd_v2(const MunitParameter params[], void* user_data);
#endif /* TESTS_CLI_TESTS_H_ */

/*
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | https://www.php.net/license/3_01.txt                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Nikita Popov <nikic@php.net>                                |
   +----------------------------------------------------------------------+
 */

#include <main/php.h>

#include "fuzzer.h"
#include "fuzzer-sapi.h"

#define MAX_STEPS 1000
#define MAX_SIZE (8 * 1024)
static uint32_t steps_left;

/* Because the fuzzer is always compiled with clang,
 * we can assume that we don't use global registers / hybrid VM. */
typedef int (ZEND_FASTCALL *opcode_handler_t)(zend_execute_data *);

static zend_always_inline void fuzzer_step(void) {
	if (--steps_left == 0) {
		/* Reset steps before bailing out, so code running after bailout (e.g. in
		 * destructors) will get another MAX_STEPS, rather than UINT32_MAX steps. */
		steps_left = MAX_STEPS;
		zend_bailout();
	}
}

static void fuzzer_execute_ex(zend_execute_data *execute_data) {
	while (1) {
		int ret;
		fuzzer_step();
		if ((ret = ((opcode_handler_t) EX(opline)->handler)(execute_data)) != 0) {
			if (ret > 0) {
				execute_data = EG(current_execute_data);
			} else {
				return;
			}
		}
	}
}

static zend_op_array *(*orig_compile_string)(zend_string *source_string, const char *filename);

static zend_op_array *fuzzer_compile_string(zend_string *str, const char *filename) {
	if (ZSTR_LEN(str) > MAX_SIZE) {
		/* Avoid compiling huge inputs via eval(). */
		zend_bailout();
	}

	return orig_compile_string(str, filename);
}

static void (*orig_execute_internal)(zend_execute_data *execute_data, zval *return_value);

static void fuzzer_execute_internal(zend_execute_data *execute_data, zval *return_value) {
	fuzzer_step();

	uint32_t num_args = ZEND_CALL_NUM_ARGS(execute_data);
	for (uint32_t i = 0; i < num_args; i++) {
		/* Some internal functions like preg_replace() may be slow on large inputs.
		 * Limit the maximum size of string inputs. */
		zval *arg = ZEND_CALL_VAR_NUM(execute_data, i);
		if (Z_TYPE_P(arg) == IS_STRING && Z_STRLEN_P(arg) > MAX_SIZE) {
			zend_bailout();
		}
	}

	orig_execute_internal(execute_data, return_value);
}

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
	if (Size > MAX_SIZE) {
		/* Large inputs have a large impact on fuzzer performance,
		 * but are unlikely to be necessary to reach new codepaths. */
		return 0;
	}

	steps_left = MAX_STEPS;
	fuzzer_do_request_from_buffer("/fuzzer.php", (const char *) Data, Size, /* execute */ 1);

	return 0;
}

int LLVMFuzzerInitialize(int *argc, char ***argv) {
	/* Compilation will often trigger fatal errors.
	 * Use tracked allocation mode to avoid leaks in that case. */
	putenv("USE_TRACKED_ALLOC=1");

	/* Just like other SAPIs, ignore SIGPIPEs. */
	signal(SIGPIPE, SIG_IGN);

	fuzzer_init_php();

	zend_execute_ex = fuzzer_execute_ex;
	orig_execute_internal = zend_execute_internal ? zend_execute_internal : execute_internal;
	zend_execute_internal = fuzzer_execute_internal;
	orig_compile_string = zend_compile_string;
	zend_compile_string = fuzzer_compile_string;

	/* fuzzer_shutdown_php(); */
	return 0;
}

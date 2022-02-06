/*
 * Copyright (c) 2022 Tyler J. Anderson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 *    3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file pm2_5-api-tests.c
 *
 * @brief Unit tests for PMS 5003 PM2.5 sensor
 */

#include <pm2_5.h>

#include <stdio.h>
#include <stdbool.h>

#define PM2_5_TEST_UNIT(name, func) do {			\
		printf("Running tests for unit " #name ":\n");	\
		func(&testcnt, &failcnt);			\
	} while (0);


#define PM2_5_TEST_FN(name, func, expected) do {			\
		bool is_passed;						\
									\
		printf("\tTest " #name ": " #func			\
		       " == " #expected "...");				\
									\
		is_passed = func == expected;				\
									\
		is_passed ? printf("SUCCESS\n")				\
			: printf("FAILURE\n");				\
		*testcnt += 1;						\
		*failcnt += is_passed ? 0 : 1;				\
	} while (0);

#define PM2_5_CONFIG_SUCCESS(dev) do {					\
		uint8_t intf = 0xff;					\
		dev.intf_ptr = &intf;					\
		dev.send_cb = pm2_5_user_send_success;			\
		dev.receive_cb = pm2_5_user_receive_success;		\
		dev.available_cb = pm2_5_user_available_success;	\
	} while (0);

#define PM2_5_CONFIG_FAILURE(dev) do {					\
		uint8_t intf = 0xff;					\
		dev.intf_ptr = &intf;					\
		dev.send_cb = pm2_5_user_send_failure;			\
		dev.receive_cb = pm2_5_user_receive_failure;		\
		dev.available_cb = pm2_5_user_available_failure;	\
	} while (0);

/** @brief pointer to describe test function pointers */
typedef bool (*pm2_5_test_cb)(void);

/** @brief pointer to hold library of tests */
static pm2_5_test_cb *test_library;

/** @brief index for the next cb to add */
static unsigned int pm2_5_test_index = 0;

int8_t pm2_5_user_send_success(const uint8_t *data,
			       uint8_t len, void *intf_ptr)
{
	if (!data) {
		return -1;
	}

	if (data[0] != PM2_5_START_BYTE_1 || data[1] != PM2_5_START_BYTE_2) {
		return -1;
	}

	return 0;
}
	 
int8_t pm2_5_user_receive_success(uint8_t *data,
				  uint8_t len, void *intf_ptr)
{
	uint16_t sum = 0;

	if (!data) {
		return -1;
	}

	data[0] = PM2_5_START_BYTE_1;
	data[1] = PM2_5_START_BYTE_2;

	/* TODO: Implement lenght bytes in DATA[2] and DATA[3] */
	for (uint8_t i = 3; i < len - 2; i++) {
		data[i] = 0x05;
	}

	for (uint8_t i = 0; i < len - 2; i++) {
		sum += data[i];
	}

	data[len - 2] = (uint8_t) (sum >> 8);
	data[len - 1] = (uint8_t) (sum & 0x00ff);

	return 0;
}

uint8_t pm2_5_user_available_success(void *intf_ptr)
{
	return 1;
}

int8_t pm2_5_user_send_failure(const uint8_t *data,
			       uint8_t len, void *intf_ptr)
{
	return -1;
}
	 
int8_t pm2_5_user_receive_failure(uint8_t *data,
				  uint8_t len, void *intf_ptr)
{
	return -1;
}

uint8_t pm2_5_user_available_failure(void *intf_ptr)
{
	return 0;
}

static unsigned int unit_pm2_5_init(int *testcnt, int *failcnt)
{
	uint8_t intf = 0xfc;
	bool rst;

	pm2_5_dev *null_dev = NULL;
	pm2_5_dev miss_send;
	pm2_5_dev miss_rec;
	pm2_5_dev miss_avail;
	pm2_5_dev success_dev;
	pm2_5_dev failure_dev;

	/* Test null */
	PM2_5_TEST_FN(null_dev, pm2_5_init(null_dev), PM2_5_E_NULL_PTR);



	/* Test missing send */
	miss_send.intf_ptr = &intf;
	miss_send.send_cb = NULL;
	miss_send.receive_cb = pm2_5_user_receive_success;
	miss_send.available_cb = pm2_5_user_available_success;

	PM2_5_TEST_FN(miss_send, pm2_5_init(&miss_send), PM2_5_E_NULL_PTR);



	/* Test missing receive */
	miss_rec.intf_ptr = &intf;
	miss_rec.send_cb = pm2_5_user_send_success;
	miss_rec.receive_cb = NULL;
	miss_rec.available_cb = pm2_5_user_available_success;

	PM2_5_TEST_FN(miss_rec, pm2_5_init(&miss_rec), PM2_5_E_NULL_PTR);



	/* Test available available */
	miss_avail.intf_ptr = &intf;
	miss_avail.send_cb = pm2_5_user_send_success;
	miss_avail.receive_cb = pm2_5_user_receive_success;
	miss_avail.available_cb = NULL;

	PM2_5_TEST_FN(miss_avail, pm2_5_init(&miss_avail), PM2_5_E_NULL_PTR);



	/* Test for success */
	success_dev.intf_ptr = &intf;
	success_dev.send_cb = pm2_5_user_send_success;
	success_dev.receive_cb = pm2_5_user_receive_success;
	success_dev.available_cb = pm2_5_user_available_success;

	PM2_5_TEST_FN(success_dev, pm2_5_init(&success_dev), PM2_5_OK);



	/* Test for failure */
	failure_dev.intf_ptr = &intf;
	failure_dev.send_cb = pm2_5_user_send_failure;
	failure_dev.receive_cb = pm2_5_user_receive_failure;
	failure_dev.available_cb = pm2_5_user_available_failure;

	PM2_5_TEST_FN(failure_dev, pm2_5_init(&failure_dev), PM2_5_OK);



	return *testcnt;
}

int unit_pm2_5_set_mode(int *testcnt, int *failcnt)
{
	uint8_t intf = 0xFF;
	pm2_5_dev dev_success;
	pm2_5_dev dev_fail;

	/* Success */
	dev_success.intf_ptr = &intf;
	dev_success.send_cb = pm2_5_user_send_success;
	dev_success.receive_cb = pm2_5_user_receive_success;
	dev_success.available_cb = pm2_5_user_available_success;

	pm2_5_init(&dev_success);

	PM2_5_TEST_FN(set_mode_passive_success,
		      pm2_5_set_mode(&dev_success, PM2_5_MODE_PASSIVE),
		      PM2_5_OK);



	PM2_5_TEST_FN(set_mode_active_success,
		      pm2_5_set_mode(&dev_success, PM2_5_MODE_ACTIVE),
		      PM2_5_OK);



	/* Failure */
	dev_fail.intf_ptr = &intf;
	dev_fail.send_cb = pm2_5_user_send_failure;
	dev_fail.receive_cb = pm2_5_user_receive_failure;
	dev_fail.available_cb = pm2_5_user_available_failure;

	pm2_5_init(&dev_fail);

	PM2_5_TEST_FN(set_mode_passive_failure,
		      pm2_5_set_mode(&dev_fail, PM2_5_MODE_PASSIVE),
		      PM2_5_E_COMM_FAILURE);



	PM2_5_TEST_FN(set_mode_active_failure,
		      pm2_5_set_mode(&dev_fail, PM2_5_MODE_ACTIVE),
		      PM2_5_E_COMM_FAILURE);



	return *testcnt;
}

int unit_pm2_5_get_data(int *testcnt, int *failcnt)
{
	uint8_t intf = 0xFF;
	pm2_5_dev dev_success;
	pm2_5_dev dev_failure;
	pm2_5_data data_good;
	pm2_5_data *data_null = NULL;

	/* Success */
	dev_success.intf_ptr = &intf;
	dev_success.send_cb = pm2_5_user_send_success;
	dev_success.receive_cb = pm2_5_user_receive_success;
	dev_success.available_cb = pm2_5_user_available_success;

	pm2_5_init(&dev_success);

	PM2_5_TEST_FN(pm2_5_get_data_success,
		      pm2_5_get_data(&dev_success, &data_good),
		      PM2_5_OK);

	/* Null ptr check */
	PM2_5_TEST_FN(pm2_5_get_data_null_data,
		      pm2_5_get_data(&dev_success, data_null),
		      PM2_5_E_NULL_PTR);

	/* Failure */
	dev_failure.intf_ptr = &intf;
	dev_failure.send_cb = pm2_5_user_send_failure;
	dev_failure.receive_cb = pm2_5_user_receive_failure;
	dev_failure.available_cb = pm2_5_user_available_failure;

	pm2_5_init(&dev_failure);

	PM2_5_TEST_FN(pm2_5_get_data_failure,
		      pm2_5_get_data(&dev_failure, &data_good),
		      PM2_5_E_COMM_FAILURE);

	return *testcnt;
}

int unit_pm2_5_sleep(int *testcnt, int *failcnt)
{
	pm2_5_dev dev_success;
	pm2_5_dev dev_failure;

	/* Test Success */
	PM2_5_CONFIG_SUCCESS(dev_success);

	pm2_5_init(&dev_success);

	PM2_5_TEST_FN(pm2_5_sleep_success,
		      pm2_5_sleep(&dev_success), PM2_5_OK);

	PM2_5_TEST_FN(pm2_5_sleep_wakeup_success,
		      pm2_5_wake(&dev_success), PM2_5_OK);

	/* Test Failure */
	PM2_5_CONFIG_FAILURE(dev_failure);

	pm2_5_init(&dev_failure);

	PM2_5_TEST_FN(pm2_5_sleep_sleep_success,
		      pm2_5_sleep(&dev_failure), PM2_5_E_COMM_FAILURE);

	dev_failure.sleep = 1;

	PM2_5_TEST_FN(pm2_5_sleep_wakeup_failure,
		      pm2_5_wake(&dev_failure), PM2_5_E_COMM_FAILURE);

	return *testcnt;
}

int main() {
	int testcnt = 0;
	int failcnt = 0; 

	/* Initialize test library */
	test_library = malloc(sizeof(pm2_5_test_cb) * 10);

	/* Setup the init unit */
	PM2_5_TEST_UNIT(pm2_5_init, unit_pm2_5_init);

	/* Run the set mode unit */
	PM2_5_TEST_UNIT(pm2_5_set_mode, unit_pm2_5_set_mode);

	/* Run the get data unit */
	PM2_5_TEST_UNIT(pm2_5_get_data, unit_pm2_5_get_data);

	/* Run the sleep/wake unit */
	PM2_5_TEST_UNIT(pm2_5_sleep, unit_pm2_5_sleep);

	/* Run the tests */
	/* for (unsigned int i = 0; i < pm2_5_test_index; i++) { */
		/* failcnt += test_library[i]() ? 0 : 1; */
	/* } */

	/* Print the report */
	printf("Tests Complete: %d tests failed out of %d tests\n",
	       failcnt, testcnt);

	free(test_library);

	/* if failed, return -1 * the number of failed tests */
	return failcnt > 0 ? -1 * failcnt : 0;
}

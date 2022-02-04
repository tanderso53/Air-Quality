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
 * @file pm2_5-error.h
 *
 * @brief Header to help pico projects translate PMS 5003 API
 * errors and warnings
 *
 * @note Do not include this file in headers!!!!!!!!
 */

#ifndef PM2_5_ERROR_H
#define PM2_5_ERROR_H

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */

	enum pm2_5_error_level {
		PM2_5_ERROR,
		PM2_5_WARNING,
		PM2_5_INFO,
		PM2_5_NOT_FOUND
	};

	/** @brief Struct describing error in PMS 5003 API */
	typedef struct {
		const int8_t errno;
		const enum pm2_5_error_level level;
		const char* name;
		const char* description;
	} pm2_5_error;

	/** @brief List of errors that can be produced from PMS 5003
	 * API */
	const pm2_5_error pm2_5_errlist[] = {
		{
			.errno = 0,
			.level = PM2_5_INFO,
			.name = "PM2_5_OK",
			.description = "PM2_5 OK"
		},

		{
			.errno = -1,
			.level = PM2_5_ERROR,
			.name = "PM2_5_E_NULL_PTR",
			.description = "PM2_5 Passed Null Pointer"
		},
		{
			.errno = -2,
			.level = PM2_5_ERROR,
			.name = "PM2_5_E_COMM_FAILURE",
			.description = "PM2_5 Communication Failure"
		},

		{
			.errno = -3,
			.level = PM2_5_ERROR,
			.name = "PM2_5_E_BAD_CHECKSUM",
			.description = "PM2_5 Bad Checksum"
		},

		{
			.errno = -4,
			.level = PM2_5_ERROR,
			.name = "PM2_5_E_ARRAY_LEN",
			.description = "PM2_5 INCORRECT ARRAY LENGTH"
		},

		{
			.errno = -5,
			.level = PM2_5_ERROR,
			.name = "PM2_5_E_BAD_CMD",
			.description = "PM2_5 Unknown or Malformed command"
		},

		{
			.errno = -6,
			.level = PM2_5_ERROR,
			.name = "PM2_5_E_BAD_START_BYTES",
			.description = "PM2_5 Received Message with "
				"incorrect start bytes"
		},

		{
			.errno = 1,
			.level = PM2_5_WARNING,
			.name = "PM2_5_W_NO_DATA",
			.description = "PM2_5 No data available"
		}
	};

	/** @brief Get error level from errno */
	inline enum pm2_5_error_level pm2_5_err_level(int8_t iface_errno) {
		const uint8_t size = sizeof(pm2_5_errlist)/
			sizeof(pm2_5_errlist[0]);

		for (uint8_t i = 0; i < size; i++) {

			if (pm2_5_errlist[i].errno == iface_errno) {
				return pm2_5_errlist[i].level;
			}

		}

		return PM2_5_NOT_FOUND;
	}

	/** @brief Get error name from errno */
	inline const char* pm2_5_err_name(int8_t iface_errno) {
		const uint8_t size = sizeof(pm2_5_errlist)/
			sizeof(pm2_5_errlist[0]);

		for (uint8_t i = 0; i < size; i++) {

			if (pm2_5_errlist[i].errno == iface_errno) {
				return pm2_5_errlist[i].name;
			}

		}

		return NULL;
	}

	/** @brief Get error description from errno */
	inline const char* pm2_5_err_description(int8_t iface_errno) {
		const uint8_t size = sizeof(pm2_5_errlist)/
			sizeof(pm2_5_errlist[0]);

		for (uint8_t i = 0; i < size; i++) {

			if (pm2_5_errlist[i].errno == iface_errno) {
				return pm2_5_errlist[i].description;
			}

		}

		return NULL;
	}
#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */

#endif /* #define PM2_5_PM2_5_ERROR_H */

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
 * @file pm2_5.c
 * @brief API implementation for PMS 5003 PM2.5 sensor
 */

#include <pm2_5.h>

/*
**********************************************************************
*********************** Internal Definitions *************************
**********************************************************************
*/

static uint8_t _check_null(pm2_5_dev *dev);

static uint8_t _check_checksum(const uint8_t *rawdata);

static uint16_t _generate_checksum(const uint8_t *datainput, uint8_t len);

static uint16_t _parse_uint(uint8_t msb, uint8_t lsb);

static int8_t _format_command(uint8_t command, uint16_t arg, uint8_t *out,
			      uint8_t len);

static int8_t _parse_data(const uint8_t *rawdata, uint8_t len,
			  pm2_5_data *data);

/*
**********************************************************************
*********************** API IMPLEMENTATION *************************
**********************************************************************
*/

int8_t pm2_5_init(pm2_5_dev *dev)
{
	if (!_check_null(dev)) {
		return PM2_5_E_NULL_PTR;
	}

	dev->mode = PM2_5_MODE_ACTIVE;
	dev->sleep = 0; /* 0 means awake, 1 means sleeping */

	return PM2_5_OK;
}

int8_t pm2_5_set_mode(pm2_5_dev *dev, uint8_t mode)
{
	int8_t rst;
	uint8_t cmd[PM2_5_CMD_LEN_BYTES];

	if (!_check_null(dev)) {
		return PM2_5_E_NULL_PTR;
	}

	switch (mode) {
	case PM2_5_MODE_ACTIVE:
		rst = _format_command(PM2_5_CMD_CHANGE_MODE,
				      PM2_5_ARG_CHANGE_MODE_ACTIVE,
				      cmd, sizeof(cmd));
		break;
	case PM2_5_MODE_PASSIVE:
		rst = _format_command(PM2_5_CMD_CHANGE_MODE,
				      PM2_5_ARG_CHANGE_MODE_PASSIVE,
				      cmd,sizeof(cmd));
		break;
	default:
		rst = PM2_5_E_BAD_CMD;
		break;
	}

	if (rst != PM2_5_OK) {
		return rst;
	}

	rst = dev->send_cb(cmd, sizeof(cmd), dev->intf_ptr);

	if (rst != PM2_5_OK) {
		return PM2_5_E_COMM_FAILURE;
	}

	/* The mode stored in object is only updated if successful */
	dev->mode = mode;

	return PM2_5_OK;
}

int8_t pm2_5_get_data(pm2_5_dev *dev, pm2_5_data *data)
{
	int8_t rst;
	uint8_t rsp[PM2_5_RX_DATA_LEN_BYTES];
	uint8_t cmd[PM2_5_CMD_LEN_BYTES];

	if (!_check_null(dev) || !data) {
		return PM2_5_E_NULL_PTR;
	}

	switch (dev->mode) {

	case PM2_5_MODE_ACTIVE:
		/* In active mode, no command is required, data is
		 * always sent. Retrieve only the next set of data
		 * from the buffer */
		rst = dev->receive_cb(rsp, PM2_5_RX_DATA_LEN_BYTES,
				      dev->intf_ptr);

		if (rst != 0) {
			return PM2_5_E_COMM_FAILURE;
		}

		break;

	case PM2_5_MODE_PASSIVE:
		/* Check first if sleeping, and wake if it is */
		if (dev->sleep) {
			rst = pm2_5_wake(dev);

			if (rst != PM2_5_OK) {
				return rst;
			}
		}

		/* In passive mode, a command must be sent first
		 * requesting data */
		rst = _format_command(PM2_5_CMD_PASSIVE_READ,
				      PM2_5_ARG_NO_ARG, cmd,
				      sizeof(cmd));

		if (rst != PM2_5_OK) {
			return rst;
		}

		/* Send the command */
		rst  = dev->send_cb(cmd, PM2_5_CMD_LEN_BYTES,
				    dev->intf_ptr);

		if (rst != 0) {
			return PM2_5_E_COMM_FAILURE;
		}

		/* If successful, get the response */
		rst = dev->receive_cb(rsp, PM2_5_RX_DATA_LEN_BYTES,
				      dev->intf_ptr);

		if (rst != 0) {
			return PM2_5_E_COMM_FAILURE;
		}

		break;

	default:
		return PM2_5_E_BAD_CMD;
		break;
	}

	/* Parse the received data into the given data object */
	rst = _parse_data(rsp, PM2_5_RX_DATA_LEN_BYTES, data);

	if (rst != PM2_5_OK) {
		return rst;
	}

	return PM2_5_OK;
}

int8_t pm2_5_sleep(pm2_5_dev *dev)
{
	int8_t rst;
	uint8_t cmd[PM2_5_CMD_LEN_BYTES];

	if (!_check_null(dev)) {
		return PM2_5_E_NULL_PTR;
	}

	/* Don't need to send command if already sleeping */
	if (dev->sleep) {
		return PM2_5_OK;
	}

	rst = _format_command(PM2_5_CMD_SET_SLEEP,
			      PM2_5_ARG_SET_SLEEP_SLEEP, cmd,
			      sizeof(cmd));

	if (rst != PM2_5_OK) {
		return rst;
	}

	rst = dev->send_cb(cmd, sizeof(cmd), dev->intf_ptr);

	if (rst != 0) {
		return PM2_5_E_COMM_FAILURE;
	}

	dev->sleep = 1;

	return PM2_5_OK;
}

int8_t pm2_5_wake(pm2_5_dev *dev)
{
	int8_t rst;
	uint8_t cmd[PM2_5_CMD_LEN_BYTES];

	if (!_check_null(dev)) {
		return PM2_5_E_NULL_PTR;
	}

	/* Don't need to send command if already awake */
	if (!dev->sleep) {
		return PM2_5_OK;
	}

	rst = _format_command(PM2_5_CMD_SET_SLEEP,
			      PM2_5_ARG_SET_SLEEP_WAKEUP, cmd,
			      sizeof(cmd));

	if (rst != PM2_5_OK) {
		return rst;
	}

	rst = dev->send_cb(cmd, sizeof(cmd), dev->intf_ptr);

	if (rst != 0) {
		return PM2_5_E_COMM_FAILURE;
	}

	dev->sleep = 0;

	return PM2_5_OK;
}

/*
**********************************************************************
*********************** INTERNAL IMPLEMENTATION **********************
**********************************************************************
*/

uint8_t _check_null(pm2_5_dev *dev)
{
	if (!dev || !dev->intf_ptr || !dev->send_cb || !dev->receive_cb
	    || !dev->send_cb || !dev->available_cb) {
		return 0;
	}

	return 1;
}

/**
 * @brief Compare checksome in received messages
 *
 * Checksum is the last two bytes of the transmission. Then
 * checksum is then compared against the rest of the message
 * except the two checksum bytes
 *
 * sum =  byte0 + byte1 + ... + byte29
 * chk = (byte30 << 8) | byte31
 * sum == chk
 */
static uint8_t _check_checksum(const uint8_t *rawdata)
{
	const uint8_t len = PM2_5_RX_DATA_LEN_BYTES;
	uint16_t chk;
	uint16_t sum = 0;

	chk = _parse_uint(rawdata[len - 2], rawdata[len - 1]);

	for (uint8_t i = 0; i < len - 2; i++) {
		sum += rawdata[i];
	}

	return sum == chk ? 1 : 0;
}

static uint16_t _generate_checksum(const uint8_t *datainput, uint8_t len)
{
	uint16_t rst = 0;

	for (uint8_t i = 0; i < len; i++) {
		rst += datainput[i];
	}

	return rst;
}

static uint16_t _parse_uint(uint8_t msb, uint8_t lsb)
{
	uint16_t rst;

	rst = (uint16_t) msb << 8;
	rst = rst | (uint16_t) lsb;

	return rst;
}

static int8_t _format_command(uint8_t command, uint16_t arg, uint8_t *out,
		     uint8_t len)
{
	uint16_t checksum;

	if (!out) {
		return PM2_5_E_NULL_PTR;
	}

	/* must be the appropriate byte length */
	if (len < PM2_5_CMD_LEN_BYTES) {
		return PM2_5_E_ARRAY_LEN;
	}

	/* fill out the standard start bytes and the given cmd */
	out[0] = PM2_5_START_BYTE_1;
	out[1] = PM2_5_START_BYTE_2;
	out[2] = command;

	/* Send MSB first of arg, then LSB */
	out[3] = (uint8_t) (arg >> 8);
	out[4] = (uint8_t) (arg & 0x00FF);

	/* Command requires a 16 bit checksome */
	checksum = _generate_checksum(out, 5);

	/* Send checksum MSB first, then LSB */
	out[5] = (uint8_t) (checksum >> 8);
	out[6] = (uint8_t) (checksum & 0x00FF);

	return PM2_5_OK;
}

static int8_t _parse_data(const uint8_t *rawdata, uint8_t len,
			  pm2_5_data *data)
{
	uint16_t dataarr[PM2_5_RX_DATA_NUM_POINTS];
	const uint8_t datastart = 4; /* Data starts at rawdata[4] */

	if (!rawdata || !data) {
		return PM2_5_E_NULL_PTR;
	}

	if (len != PM2_5_RX_DATA_LEN_BYTES) {
		return PM2_5_E_ARRAY_LEN;
	}

	/* Check for correct start bytes */
	if (rawdata[0] != PM2_5_START_BYTE_1 ||
	    rawdata[1] != PM2_5_START_BYTE_2) {
		return PM2_5_E_BAD_START_BYTES;
	}

	/* Check checksum */
	if (!_check_checksum(rawdata)) {
		return PM2_5_E_BAD_CHECKSUM;
	}

	for (uint8_t i = 0; i < PM2_5_RX_DATA_NUM_POINTS; i++) {
		uint8_t msb;
		uint8_t lsb;

		msb = rawdata[i * 2 + datastart];
		lsb = rawdata[i * 2 + datastart + 1];

		dataarr[i] = _parse_uint(msb, lsb);
	}

	/* Fill in data struct from position definitions */
	data->pm1_0_std = dataarr[PM2_5_POS_PM1_0_STD];
	data->pm2_5_std = dataarr[PM2_5_POS_PM2_5_STD];
	data->pm10_std = dataarr[PM2_5_POS_PM10_STD];
	data->pm1_0_atm = dataarr[PM2_5_POS_PM1_0_ATM];
	data->pm2_5_atm = dataarr[PM2_5_POS_PM2_5_ATM];
	data->atm_unit = dataarr[PM2_5_POS_UNIT_ATM];
	data->np_0_3 = dataarr[PM2_5_POS_NP_0_3];
	data->np_0_5 = dataarr[PM2_5_POS_PM1_0_STD];
	data->np_1_0 = dataarr[PM2_5_POS_PM1_0_STD];
	data->np_2_5 = dataarr[PM2_5_POS_NP_2_5];
	data->np_5_0 = dataarr[PM2_5_POS_NP_5_0];
	data->np_10 = dataarr[PM2_5_POS_NP_10];

	return PM2_5_OK;
}

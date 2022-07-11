#include "bme680-interface.h"
#include "pm2_5-interface.h"
#include <pm2_5-error.h>
#include "esp-at-modem.h"
#include "ws2812.pio.h"
#include "debugmsg.h"
#include "aq-error-state.h"
#include "aq-stdio.h"
#include "pico/multicore.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/watchdog.h"

#ifndef AIR_QUALITY_INFO_LED_PIN
#define AIR_QUALITY_INFO_LED_PIN 16
#endif

#ifndef AIR_QUALITY_PM2_5_TX_PIN
#define AIR_QUALITY_PM2_5_TX_PIN 8
#endif

#ifndef AIR_QUALITY_PM2_5_RX_PIN
#define AIR_QUALITY_PM2_5_RX_PIN 9
#endif

#ifndef AIR_QUALITY_PM2_5_UART
#define AIR_QUALITY_PM2_5_UART uart1
#endif

#ifndef AIR_QUALITY_ADC_BATT_GPIO_PIN
#define AIR_QUALITY_ADC_BATT_GPIO_PIN 28
#endif

#ifndef AIR_QUALITY_ADC_BATT_ADC_CH
#define AIR_QUALITY_ADC_BATT_ADC_CH 2
#endif

#ifndef AIR_QUALITY_BATT_LOW_V
#define AIR_QUALITY_BATT_LOW_V ((double) 3.60)
#endif

#ifndef AIR_QUALITY_WIFI_TX_PIN
#define AIR_QUALITY_WIFI_TX_PIN 10
#endif

#ifndef AIR_QUALITY_WIFI_RX_PIN
#define AIR_QUALITY_WIFI_RX_PIN 11
#endif

#ifndef AIR_QUALITY_WIFI_GPIO_EN_PIN
#define AIR_QUALITY_WIFI_GPIO_EN_PIN 12
#endif

#ifndef AIR_QUALITY_WIFI_GPIO_RESET_PIN
#define AIR_QUALITY_WIFI_GPIO_RESET_PIN 13
#endif

#ifndef AIR_QUALITY_WIFI_PIO
#define AIR_QUALITY_WIFI_PIO pio1
#endif

#ifndef AIR_QUALITY_WIFI_BAUD
#define AIR_QUALITY_WIFI_BAUD 115200
#endif

#ifndef AIR_QUALITY_WIFI_TX_SM
#define AIR_QUALITY_WIFI_TX_SM 0
#endif

#ifndef AIR_QUALITY_WIFI_RX_SM
#define AIR_QUALITY_WIFI_RX_SM 1
#endif

#ifndef PICO_BOARD
#define PICO_BOARD "unknown"
#endif

#ifndef PICO_TARGET_NAME
#define PICO_TARGET_NAME "unknown"
#endif

/* Useful local macros */
#define ARRAY_LEN(array) sizeof(array)/sizeof(array[0])

/*
**********************************************************************
******************** PROGRAM DEFINITIONS *****************************
**********************************************************************
*/

static uint32_t *op_reg = NULL; /**< @brief Operational Register */

static esp_at_cfg aq_wifi_cfg;

static esp_at_status aq_wifi_status;

/** @brief Print out data from environmental sensors as json string
 * @p d Data struct from bme68x vendor library
 */
void air_quality_print_data(struct bme68x_data *d, uint32_t millis);

static void aq_bme680_handle_error(int8_t i_errno, aq_status *s);

static void aq_pm2_5_handle_error(int8_t i_errno, aq_status *s);

static void aq_wifi_set_flags(aq_status *s);

static uint16_t aq_abrev_netmask(const char *nm);

/*
**********************************************************************
********************** PROGRAM IMPLEMENTATIONS ***********************
**********************************************************************
*/

void air_quality_print_data(struct bme68x_data *d, uint32_t millis)
{
	aq_nprintf("{\"sensor\": \"BME680\", \"data\": [");

	aq_nprintf("{\"name\": \"temperature\", "
		   "\"value\": %.2f, "
		   "\"unit\": \"degC\", "
		   "\"timemillis\": %lu}, ", d->temperature,
		   (unsigned long) millis);

	aq_nprintf("{\"name\": \"pressure\", "
		   "\"value\": %.2f, "
		   "\"unit\": \"Pa\", "
		   "\"timemillis\": %lu}, ", d->pressure,
		   (unsigned long) millis);

	aq_nprintf("{\"name\": \"humidity\", "
		   "\"value\": %.2f, "
		   "\"unit\": \"%%\", "
		   "\"timemillis\": %lu}, ", d->humidity,
		   (unsigned long) millis);

	aq_nprintf("{\"name\": \"gas resistance\", "
		   "\"value\": %.2f, "
		   "\"unit\": \"Ohms\", "
		   "\"timemillis\": %lu}], ",
		   d->gas_resistance, (unsigned long) millis);

	aq_nprintf("\"status\": {"
		   "\"sensor\": \"%#x\"}}",
		   d->status);
}

void aq_bme680_handle_error(int8_t i_errno, aq_status *s)
{
	switch (i_errno) {
	case BME68X_OK:
		aq_status_unset_status(AQ_STATUS_REGION_BME680 ^
				       AQ_STATUS_I_BME680_READING,
				       s);
		break;
	case BME68X_E_COM_FAIL:
		aq_status_set_status(AQ_STATUS_E_BME680_COMM_FAIL,
				     s);
		break;
	default:
		aq_status_set_status(AQ_STATUS_E_BME680_GENERAL_FAIL,
				     s);
		break;
	}
}

void aq_pm2_5_print_data(pm2_5_dev *dev, pm2_5_data *d,
			 unsigned long millis)
{
	aq_nprintf( "{\"sensor\": \"PMS 5003\", "
		    "\"data\": [");

	aq_nprintf( "{\"name\": \"PM1.0 Std\", "
		    "\"value\": %u, "
		    "\"unit\": \"ug/m^3\", "
		    "\"timemillis\": %lu}, ",
		    d->pm1_0_std, millis);

	aq_nprintf( "{\"name\": \"PM2.5 Std\", "
		    "\"value\": %u, "
		    "\"unit\": \"ug/m^3\", "
		    "\"timemillis\": %lu}, ",
		    d->pm2_5_std, millis);

	aq_nprintf( "{\"name\": \"pm10_std\", "
		    "\"value\": %u, "
		    "\"unit\": \"ug/m^3\", "
		    "\"timemillis\": %lu}, ",
		    d->pm10_std, millis);

	aq_nprintf( "{\"name\": \"NP > 0.3um\", "
		    "\"value\": %u, "
		    "\"unit\": \"num/0.1L air\", "
		    "\"timemillis\": %lu}, ",
		    d->np_0_3, millis);

	aq_nprintf( "{\"name\": \"NP > 0.5um\", "
		    "\"value\": %u, "
		    "\"unit\": \"num/0.1L air\", "
		    "\"timemillis\": %lu}, ",
		    d->np_0_5, millis);

	aq_nprintf( "{\"name\": \"NP > 1.0um\", "
		    "\"value\": %u, "
		    "\"unit\": \"num/0.1L air\", "
		    "\"timemillis\": %lu}, ",
		    d->np_1_0, millis);

	aq_nprintf( "{\"name\": \"NP > 2.5um\", "
		    "\"value\": %u, "
		    "\"unit\": \"num/0.1L air\", "
		    "\"timemillis\": %lu}, ",
		    d->np_2_5, millis);

	aq_nprintf( "{\"name\": \"NP > 5.0\", "
		    "\"value\": %u, "
		    "\"unit\": \"num/0.1L air\", "
		    "\"timemillis\": %lu}, ",
		    d->np_5_0, millis);

	aq_nprintf( "{\"name\": \"NP > 10\", "
		    "\"value\": %u, "
		    "\"unit\": \"num/0.1L air\", "
		    "\"timemillis\": %lu}], ",
		    d->np_10, millis);

	aq_nprintf( "\"status\": {"
		    "\"opmode\": \"%s\", "
		    "\"sleep\": %s}}",
		    dev->mode == PM2_5_MODE_ACTIVE ? "ACTIVE" : "PASSIVE",
		    dev->sleep ? "true" : "false");
}

void aq_pm2_5_handle_error(int8_t i_errno, aq_status *s)
{
	char level[16];
	if (i_errno == PM2_5_OK) {
		/* All errors/warnings will clear on successful
		 * library operation */
		aq_status_unset_status(AQ_STATUS_REGION_PM2_5 ^
				       AQ_STATUS_I_PM2_5_READING, s);
		return;
	}

	switch (pm2_5_err_level(i_errno)) {
	case PM2_5_INFO:
		strcpy(level, "INFO");
		break;
	case PM2_5_WARNING:
		strcpy(level, "WARNING");
		aq_status_set_status(AQ_STATUS_W_PM2_5_NO_DATA, s);
		break;
	case PM2_5_ERROR:
		strcpy(level, "ERROR");
		aq_status_set_status(AQ_STATUS_E_PM2_5_GENERAL_FAIL, s);
		break;
	default:
		strcpy(level, "UNKNOWN");
		break;
	}

	printf("%s: %s\n", level,
	       pm2_5_err_description(i_errno));
}

void aq_adc_init()
{
	adc_init();

	adc_gpio_init(AIR_QUALITY_ADC_BATT_GPIO_PIN);
}

double aq_batt_voltage(aq_status *s)
{
	double vbatt;
	const double cf = 2 * 3.3 / (1 << 12);

	adc_select_input(AIR_QUALITY_ADC_BATT_ADC_CH);
	vbatt = cf * (double) adc_read();

	if (vbatt < AIR_QUALITY_BATT_LOW_V) {
		aq_status_set_status(AQ_STATUS_W_BATT_LOW, s);
	} else {
		aq_status_unset_status(AQ_STATUS_W_BATT_LOW, s);
	}

	return vbatt;
}

void aq_print_batt(aq_status *s)
{
	aq_nprintf("{\"sensor\": \"Board\", "
		   "\"data\": [");

	aq_nprintf("{\"name\": \"V Batt\", "
		   "\"value\": %0.2f, "
		   "\"unit\": \"V\", "
		   "\"timemillis\": %lu}], ",
		   aq_batt_voltage(s),
		   to_ms_since_boot(get_absolute_time()));

	aq_nprintf("\"status\": {"
		   "\"charging\": \"%s\"}}",
		   "unknown");
}

void aq_wifi_set_flags(aq_status *s)
{
	int rslt;

	rslt = esp_at_cipstatus(&aq_wifi_cfg, &aq_wifi_status);

	if (rslt) {
		aq_status_set_status(AQ_STATUS_E_WIFI_FAIL |
				     AQ_STATUS_W_WIFI_DISCONNECTED, s);
		aq_status_unset_status(AQ_STATUS_I_CLIENT_CONNECTED, s);

		DEBUGDATA("esp_at_cipstatus() failed with status",
			  rslt, "%d");

		return;
	} else {
		aq_status_unset_status(AQ_STATUS_E_WIFI_FAIL, s);
	}

	DEBUGDATA("Checking wifi status:", aq_wifi_status.status,
		  "%#.4x");

	/* need to know if clients are connected so we
	 * don't waste time writing to them */
	if (aq_wifi_status.status & ESP_AT_STATUS_CLIENT_CONNECTED) {
		aq_status_set_status(AQ_STATUS_I_CLIENT_CONNECTED, s);
	} else {
		aq_status_unset_status(AQ_STATUS_I_CLIENT_CONNECTED, s);
	}

	if (aq_wifi_status.status & ESP_AT_STATUS_WIFI_CONNECTED) {
		aq_status_unset_status(AQ_STATUS_W_WIFI_DISCONNECTED, s);
	} else {
		aq_status_set_status(AQ_STATUS_W_WIFI_DISCONNECTED, s);
	}
}

uint16_t aq_abrev_netmask(const char *nm)
{
	char str[24];
	uint32_t mask = 0;
	char *tk;
	char *last;

	DEBUGDATA("Full netmask", nm, "%s");

	if (!nm) {
		return -1;
	}

	strncpy(str, nm, sizeof(str) - 1);
	str[ARRAY_LEN(str) - 1] = '\0';

	for (tk = strtok_r(str, ".", &last);
	     tk;
	     tk = strtok_r(NULL, ".", &last)) {
		uint32_t bits;

		mask = mask << 8;

		bits = strtol(tk, NULL, 10);
		DEBUGDATA("Netmask byte", bits, "%lu");
		mask = mask | bits;
	}

	for (uint16_t i = 0; i < 32; ++i) {
		if (mask & (1ul << i))
			return 32 - i;
	}

	return 0;
}

/*
**********************************************************************
****************************** MAIN **********************************
**********************************************************************
*/

int main() {
	int8_t ret = 0;

	/* Interfaces */
	bme680_intf b_intf;
	pm2_5_intf p_intf;
	aq_status status = {
		.led_pio = pio0,
		.led_sm = 0,
		.led_pin = AIR_QUALITY_INFO_LED_PIN
	};

	/* Output Data Structures */
	struct bme68x_data d;
	pm2_5_data pdata;

	/* Configuration Parameters */
	bme680_run_mode m = FORCED_MODE;
	const uint16_t sample_delay_ms = 10000;
	absolute_time_t next_sample_time;

	stdio_usb_init();

	aq_status_init(&status);
	op_reg = &status.status;

#ifdef AIR_QUALITY_WAIT_CONNECTION
	aq_status_set_status(AQ_STATUS_U_REQ_USB, &status);

	for (;;) {

		if (stdio_usb_connected()) {
			printf("Welcome! You are connected!\n");
			aq_status_unset_status(AQ_STATUS_U_REQ_USB,
					       &status);
			break;
		}

		sleep_ms(100);
	}
#endif /* AIR_QUALITY_WAIT_CONNECTION */

	/* initialize variables in interface struct */
	b_intf.i2c = NULL; /* NULL i2c will select default */
	b_intf.timeout = 1000; /* 500ms timeout on i2c read/write */

#ifdef BME680_INTERFACE_SELFTEST
	/* Option to compile in a selft test of sensor at start of
	 * MCU */
	printf("Beginning BME680 Selftest...Standby...\n");
	ret = selftest_bme680_sensor(&b_intf, BME68X_I2C_ADDR_LOW);

	if (ret == BME68X_OK) {
		printf("BME680 Selftest SUCCESS...Continuing...\n");
	} else if (ret > 0) {
		printf("BME680 Selftest WARNING with code %d...Continuing...\n",
		       ret);
	} else {
		printf("BME680 Selftest FAILURE with code %d...Ending...\n",
			ret);
		aq_status_set_status(AQ_STATUS_E_BME680_SELFTEST_FAIL,
				     &status);
		return 1;
	}
#endif /* #ifdef BME680_INTERFACE_SELFTEST */

	/* Initialize battery checker */
	aq_adc_init();

	/* Initialize WiFi Module */
	if (esp_at_init_module(&aq_wifi_cfg, AIR_QUALITY_WIFI_PIO,
			       AIR_QUALITY_WIFI_TX_SM,
			       AIR_QUALITY_WIFI_RX_SM,
			       AIR_QUALITY_WIFI_TX_PIN,
			       AIR_QUALITY_WIFI_RX_PIN,
			       AIR_QUALITY_WIFI_BAUD,
			       AIR_QUALITY_WIFI_GPIO_EN_PIN,
			       AIR_QUALITY_WIFI_GPIO_RESET_PIN)) {
		aq_status_unset_status(AQ_STATUS_W_WIFI_DISCONNECTED,
				       &status);
	} else {
		aq_status_set_status(AQ_STATUS_W_WIFI_DISCONNECTED |
				     AQ_STATUS_E_WIFI_FAIL, &status);
		printf("ERROR: Failed to intitialize WiFi module\n");
	}

	if (!(status.status & AQ_STATUS_W_WIFI_DISCONNECTED)) {
		ret = esp_at_cipserver_init(&aq_wifi_cfg);

		if (ret < 0) {
			printf("Error: Could not initialize WiFi server\n");
			aq_status_set_status(AQ_STATUS_W_WIFI_DISCONNECTED,
					     &status);
		} else {
			aq_status_unset_status(AQ_STATUS_E_WIFI_FAIL,
					       &status);
		}
	}

#ifdef AIR_QUALITY_WAIT_CONNECTION
	if (!(status.status & AQ_STATUS_W_WIFI_DISCONNECTED)) {
		aq_status_set_status(AQ_STATUS_U_REQ_USER_INPUT,
				     &status);
		esp_at_passthrough(&aq_wifi_cfg);
		aq_status_unset_status(AQ_STATUS_U_REQ_USER_INPUT,
				       &status);
	}
#endif /* #ifdef AIR_QUALITY_WAIT_CONNECTION */

	ret = bme680_init(&b_intf, BME68X_I2C_ADDR_LOW, m);
	aq_bme680_handle_error(ret, &status);

	/* Start PM2_5 Sensor */
	p_intf.uart = AIR_QUALITY_PM2_5_UART;
	ret = pm2_5_intf_init(&p_intf, AIR_QUALITY_PM2_5_TX_PIN,
			      AIR_QUALITY_PM2_5_RX_PIN);
	aq_pm2_5_handle_error(ret, &status);

	ret = pm2_5_set_mode(&p_intf.dev, PM2_5_MODE_PASSIVE);
	aq_pm2_5_handle_error(ret, &status);

	next_sample_time = make_timeout_time_ms(sample_delay_ms);

	/* Initialize stdio processing thread */
	aq_wifi_set_flags(&status);
	aq_stdio_init(&status, &aq_wifi_status);

	/* Start the watchdog to require update every two cycles */
	watchdog_enable(20000, true);

	/* Keep polling the sensor for data if initialization was
	 * successful. This loop will only break on error. */
	for (;;) {
		absolute_time_t readtime;
		uint8_t print_pm = 0;

		/* Check-in with the watchdog */
		watchdog_update();

		/* Check USB STDIO */
		if (stdio_usb_connected()) {
			aq_status_set_status(AQ_STATUS_I_USBCOMM_CONNECTED,
					     &status);
		} else {
			aq_status_unset_status(AQ_STATUS_I_USBCOMM_CONNECTED,
					       &status);
		}

		/* Check wifi */
		aq_wifi_set_flags(&status);

		aq_status_set_status(AQ_STATUS_I_BME680_READING,
				     &status);
		ret = bme680_sample(m, &b_intf, &d);

		/* Get time before handling error so it's as close as
		 * possible */
		readtime = get_absolute_time();
		next_sample_time = delayed_by_ms(next_sample_time,
						 sample_delay_ms);

		aq_status_unset_status(AQ_STATUS_I_BME680_READING,
				       &status);

		/* Check BME680 sensor status bit for relevent
		 * warnings */
		if (d.status & BME68X_HEAT_STAB_MSK)
			aq_status_unset_status(AQ_STATUS_W_BME680_GAS_UNSTABLE,
					       &status);
		else
			aq_status_set_status(AQ_STATUS_W_BME680_GAS_UNSTABLE,
					     &status);
		if (d.status & BME68X_GASM_VALID_MSK)
			aq_status_unset_status(AQ_STATUS_W_BME680_GAS_INVALID,
					       &status);
		else
			aq_status_set_status(AQ_STATUS_W_BME680_GAS_INVALID,
					     &status);

		aq_bme680_handle_error(ret, &status);

		if (ret == BME68X_W_NO_NEW_DATA) {
			continue;
		}

		if (ret < 0) {
			break;
		}

		/* PM2.5 READ */
		aq_status_set_status(AQ_STATUS_I_PM2_5_READING,
				     &status);
		ret = pm2_5_get_data(&p_intf.dev, &pdata);
		aq_status_unset_status(AQ_STATUS_I_PM2_5_READING,
				       &status);
		aq_pm2_5_handle_error(ret, &status);
		print_pm = ret == 0 ? 1 : 0;

		/* Print out all the data */
		aq_nprintf("{\"program\": \"%s\", \"board\": \"%s\", "
			   "\"status\": %lu, "
			   "\"ip address\": \"%s/%d\", "
			   "\"status masks\": {"
			   "\"wait\": %lu, "
			   "\"info\": %lu, "
			   "\"warning\": %lu, "
			   "\"error\": %lu"
			   "}, "
			   "\"output\": [",
			   PICO_TARGET_NAME, PICO_BOARD, status.status,
			   aq_wifi_status.ipv4,
			   aq_abrev_netmask(aq_wifi_status.ipv4_netmask),
			   AQ_STATUS_MASK_WAIT,
			   AQ_STATUS_MASK_INFO,
			   AQ_STATUS_MASK_WARNING,
			   AQ_STATUS_MASK_ERROR);

		aq_print_batt(&status);

		aq_nprintf(", ");

		air_quality_print_data(&d, to_ms_since_boot(readtime));

		if (print_pm) {
			aq_nprintf(", ");

			aq_pm2_5_print_data(&p_intf.dev, &pdata,
					    to_ms_since_boot(readtime));
		}

		aq_nprintf("], \"sentmillis\": %lu}\n",
			   to_ms_since_boot(get_absolute_time()));

		/* Help core1 process stdio if it isn't done yet */
		aq_stdio_process();

		/* Tell stdio core to sleep when done, and sleep this
		 * core until next sample time */
		aq_stdio_sleep_until(next_sample_time);
		sleep_until(next_sample_time);
	}

	/* Deinit i2c if loop broke */
	bme680_deinit(&b_intf);
	pm2_5_intf_deinit(&p_intf);

	return 1;
}

#ifndef ESP_AT_MODEM_H
#define ESP_AT_MODEM_H

#include <stddef.h>

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

int init_wifi_module();

int wifi_send_string(const char *s, size_t len);

int send_wifi_cmd(const char *cmd, char *rsp, unsigned int len);

void wifi_passthrough();

#endif /* #ifndef ESP_AT_MODEM_H */

# Pico Compatible ESP-AT WiFi Library

By Tyler J. Anderson

A library to allow Raspberry Pi Pico boards and similar using the
Pico SDK to communicate with a WiFi co-processor running Espressif
System's variant of the AT Command language.

## License

The Pico ESP-AT WiFi Library is Copyright (C) 2022 Tyler J. Anderson
and may be used, modified, and distributed under the terms of the BSD
3-clause license. See LICENSE in the source distribution for
details. The license must be included with all distributions of this
library.

## Features

- UART passthrough shell to allow sending of commands on stdin
  directly to co-processor
- Device status checking
- TCP server creation and muxing
- Sending data to remote TCP clients

## Supported Chips

- ESP8266

## Dependencies

- Pico SDK
- Debugmsg library
- AT Response Parsing Library (included in source distribution)

## Building and Linking

The Pico ESP-AT WiFi Library uses CMake to describe the build
process, and is written as an interface library. To build, add
the following to your CMakeLists.txt for your project, replacing
paths and project name as required.

``` cmake
add_subdirectory(
	${PATH_TO_ESP_AT_LIBRARY})

target_link_libraries(your-cmake-project PRIVATE
	esp_at_modem)
```

To build documentation, enter the library directory
and enter the following commands.

``` bash
cmake -DESP_AT_BUILD_DOCS=ON -S . -B build

cmake --build build
```

## Links

- [Espressif ESP-AT Command Reference](https://docs.espressif.com/projects/esp-at/en/latest/esp32/index.html)

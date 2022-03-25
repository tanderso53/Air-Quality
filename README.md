# Air Quality Sensor Platform for Pico-based systems

By Tyler J. Andersion

## License

The Air Quality Sensor Platform is Copyright (C) 2022 Tyler
J. Anderson and may be used, modified, and distributed under the terms
of the BSD 3-clause license. See LICENSE in the source distribution
for details. The license must be included with all distributions of
this library.

## Features

- PM1.0, PM2.5, PM5, and PM10 measurement
- Temperature, humidity, and pressure
- VOC measurement
- TCP stream server for data delivery over WiFi to multiple clients
- JSON formatted data output on WiFi and USB

## Data Format

The data are formatted as a JSON string. JSON objects representing
each sensor module can be found by indexing the retrieved string at
`.output[i]`, where `i` is the index of the sensor.

The array of data for each sensor object is found by indexing the
object found as indexed above `.data`. Each object in the `data` array
has a structure as follows:

``` json
{
	"name": "Metric name",
	"value": <Metric value at reading>,
	"unit": "Metric unit type",
	"millis": "Milliseconds after boot reading was measured at"
}
```


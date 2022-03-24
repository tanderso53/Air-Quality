# AT Response Parsing Library

By Tyler J. Anderson

A library to parse responses from devices using the AT command
language common on Esspressif Systems co-processors.

## License

The AT Response Parsing Library is Copyright (C) 2022 Tyler
J. Anderson and may be used, modified, and distributed under the terms
of the BSD 3-clause license. See LICENSE in the source distribution
for details. The license must be included with all distributions of
this library.

## Features

- Tokenize each line into useful chunks of data
- Search lines for matching parameters
- Determine if data tokens are strings or integers

## Building and Linking

The AT Response Parsing Library uses CMake to describe the build
process, and is written as an interface library. To build, add
the following to your CMakeLists.txt for your project, replacing
paths and project name as required.

``` cmake
add_subdirectory(
	${PATH_TO_AT_RESPONSE_LIBRARY})

target_link_libraries(your-cmake-project PRIVATE
	at-parse)
```

To build tests and documentation, enter the library directory
and enter the following commands.

``` bash
cmake -DAT_PARSE_BUILD_DOCS=ON -DAT_PARSE_BUILD_TESTS=ON -S . -B build

cmake --build build
```

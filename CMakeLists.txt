cmake_minimum_required(VERSION 3.22)

add_executable(air-quality
	src/air-quality.c
	)

target_link_libraries(air-quality pico_stdlib)

pico_add_extra_outputs(air-quality)

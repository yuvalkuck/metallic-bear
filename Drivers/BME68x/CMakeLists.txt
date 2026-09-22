CMAKE_MINIMUM_REQUIRED(VERSION 3.22)
SET(LIB_NAME bme68x)

ENABLE_LANGUAGE(C)
SET(LIBRARY_NAME "${LIB_NAME}_Driver")
MESSAGE(STATUS "Load ${LIBRARY_NAME}")

# Interface library for includes and symbols
# Create STM32_Drivers static library
ADD_LIBRARY(${LIBRARY_NAME} OBJECT)
TARGET_SOURCES(${LIBRARY_NAME} PRIVATE
        4.4.8/bme68x.c
)
TARGET_INCLUDE_DIRECTORIES(${LIBRARY_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/4.4.8
)

target_link_libraries(${LIBRARY_NAME} PUBLIC
        stm32cubemx   # <-- name depends on your generated project; check top-level CMakeLists.txt
)

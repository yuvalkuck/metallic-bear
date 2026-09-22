CMAKE_MINIMUM_REQUIRED(VERSION 3.22)
SET(LIB_NAME bme688)

ENABLE_LANGUAGE(C)
SET(LIBRARY_NAME "${LIB_NAME}_Driver")
MESSAGE(STATUS "Load ${LIBRARY_NAME}")

# Interface library for includes and symbols
# Create STM32_Drivers static library
ADD_LIBRARY(${LIBRARY_NAME} OBJECT)
TARGET_INCLUDE_DIRECTORIES(${LIBRARY_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

TARGET_SOURCES(${LIBRARY_NAME} PRIVATE
        bme688_driver.c
        bme688_i2c.c
)

target_link_libraries(${LIBRARY_NAME} PUBLIC
        bme68x_Driver
        stm32cubemx
)

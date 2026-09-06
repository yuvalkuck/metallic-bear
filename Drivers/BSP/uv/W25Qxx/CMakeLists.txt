CMAKE_MINIMUM_REQUIRED(VERSION 3.22)
SET(LIB_NAME w25qxx)



ENABLE_LANGUAGE(C)
SET(LIBRARY_NAME "${LIB_NAME}_Driver")

# Interface library for includes and symbols
# Create STM32_Drivers static library
ADD_LIBRARY(${LIBRARY_NAME} OBJECT)
TARGET_INCLUDE_DIRECTORIES(${LIBRARY_NAME} PUBLIC
        ${${CMAKE_CURRENT_SOURCE_DIR}}
)

TARGET_SOURCES(${LIBRARY_NAME} PRIVATE
        w25q_driver.c
        w25q_spi_driver.c
)

target_link_libraries(${LIBRARY_NAME} PUBLIC
        stm32cubemx   # <-- name depends on your generated project; check top-level CMakeLists.txt
)
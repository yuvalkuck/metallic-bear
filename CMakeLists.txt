CMAKE_MINIMUM_REQUIRED(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
SET(CMAKE_C_STANDARD 11)
SET(CMAKE_C_STANDARD_REQUIRED ON)
SET(CMAKE_C_EXTENSIONS ON)


# Define the build type
IF (NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE "Debug")
ENDIF ()

# Set the project name
SET(CMAKE_PROJECT_NAME metallic-bear)

# Enable compile command to ease indexing with e.g. clangd
SET(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
PROJECT(${CMAKE_PROJECT_NAME})
MESSAGE("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
ENABLE_LANGUAGE(C ASM)

# Create an executable object type
ADD_EXECUTABLE(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
ADD_SUBDIRECTORY(cmake/stm32cubemx)
ADD_SUBDIRECTORY(Drivers/BME68x)
ADD_SUBDIRECTORY(Drivers/BSP/uv/BME688)
ADD_SUBDIRECTORY(Drivers/BSP/uv/W25Qxx)

# Link directories setup
TARGET_LINK_DIRECTORIES(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined library search paths
)

# Add sources to executable
TARGET_SOURCES(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user sources here
)

# Add include paths
TARGET_INCLUDE_DIRECTORIES(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined include paths
)

# Add project symbols (macros)
TARGET_COMPILE_DEFINITIONS(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined symbols
)


# Remove wrong libob.a library dependency when using cpp files
LIST(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
TARGET_LINK_LIBRARIES(${CMAKE_PROJECT_NAME}
        stm32cubemx
        w25qxx_Driver
        bme68x_Driver
        bme688_Driver
        # Add user defined libraries
)

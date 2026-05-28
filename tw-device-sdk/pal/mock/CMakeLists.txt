# Mock PAL backend for unit testing.
# All functions record calls and return configurable values.
# No side effects, fully deterministic.

set(MOCK_PAL_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(MOCK_PAL_SOURCES
    ${MOCK_PAL_DIR}/src/mock_pal_gpio.c
    ${MOCK_PAL_DIR}/src/mock_pal_i2c.c
    ${MOCK_PAL_DIR}/src/mock_pal_net.c
    ${MOCK_PAL_DIR}/src/mock_pal_os.c
    ${MOCK_PAL_DIR}/src/mock_pal_nvs.c
    ${MOCK_PAL_DIR}/src/mock_pal_power.c
    ${MOCK_PAL_DIR}/src/mock_pal_ota.c
    ${MOCK_PAL_DIR}/src/mock_pal_system.c
    ${MOCK_PAL_DIR}/src/mock_pal_log.c
    ${MOCK_PAL_DIR}/src/mock_pal_flash.c
)

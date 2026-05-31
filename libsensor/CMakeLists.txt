cmake_minimum_required(VERSION 3.10)
project(libsensor VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(sensor SHARED src/sensor.cpp)

if(TARGET spihal)
    # モノレポビルド: spi-hal サブディレクトリが先に処理されている場合はリンク
    target_link_libraries(sensor PRIVATE spihal pthread)
else()
    # スタンドアロンビルド（cmake -S libsensor/）: SPI HAL のソースをそのままコンパイル
    target_sources(sensor PRIVATE ../spi-hal/src/spi_driver.cpp)
    target_link_libraries(sensor pthread)
endif()

target_include_directories(sensor
    PUBLIC  include
            ../spi-hal/include
)

set_target_properties(sensor PROPERTIES
    VERSION   1.1.0
    SOVERSION 1
)

install(TARGETS sensor LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/libsensor)

# ── ADS1115 センサー（I2C, 任意）───────────────────────────────
# i2c-hal が存在する場合のみビルドする独立ターゲット。
# i2c-hal を取り除いても上の sensor（MCP3008/SPI）は影響を受けない。
if(TARGET i2chal OR EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../i2c-hal/src/i2c_driver.cpp)
    add_library(ads1115 SHARED src/ads1115.cpp)

    if(TARGET i2chal)
        # モノレポビルド: i2c-hal サブディレクトリが先に処理済みならリンク
        target_link_libraries(ads1115 PRIVATE i2chal)
    else()
        # スタンドアロンビルド: I2C HAL のソースをそのままコンパイル
        target_sources(ads1115 PRIVATE ../i2c-hal/src/i2c_driver.cpp)
        target_include_directories(ads1115 PRIVATE ../common/include)
    endif()

    target_include_directories(ads1115
        PUBLIC  include
                ../i2c-hal/include
    )
    set_target_properties(ads1115 PROPERTIES
        VERSION   1.1.0
        SOVERSION 1
    )
    install(TARGETS ads1115 LIBRARY DESTINATION lib)
endif()

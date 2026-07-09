cmake_minimum_required(VERSION 3.10)
project(libsensor VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# spi-hal（spihal）が無い構成では、モノレポビルドの TARGET も、スタンドアロン
# フォールバックの取り込み元ソースも無いため sensor をビルドできない。EXISTS
# ガードで依存の有無を確認し、無ければ target 自体をスキップする（ads1115 ブロックと同パターン）。
if(TARGET spihal OR EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../spi-hal/src/spi_driver.cpp)
    add_library(sensor SHARED src/sensor.cpp)

    target_include_directories(sensor
        PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                $<INSTALL_INTERFACE:include/libsensor>
    )

    set(_sensor_deps "")
    if(TARGET spihal)
        # モノレポビルド: spihal を PUBLIC リンク。sensor.hpp が ispi_driver.hpp を
        # include するため、include 要件は INTERFACE（公開）。消費側は find_dependency(spihal)
        # 経由で解決する。
        target_link_libraries(sensor PUBLIC spihal)
        target_link_libraries(sensor PRIVATE pthread)
        set(_sensor_deps spihal)
    else()
        # スタンドアロンビルド（cmake -S libsensor/）: SPI HAL のソースをそのままコンパイルし、
        # ビルド時のみ spi-hal の include を通す（インストール interface には出さない）。
        target_sources(sensor PRIVATE ../spi-hal/src/spi_driver.cpp)
        target_include_directories(sensor
            PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../spi-hal/include>)
        target_link_libraries(sensor pthread)
    endif()

    set_target_properties(sensor PROPERTIES
        VERSION   1.1.0
        SOVERSION 1
    )

    if(COMMAND eds_install_package)
        eds_install_package(PACKAGE sensor TARGET sensor
            VERSION ${PROJECT_VERSION} INCLUDE_DIR libsensor
            DEPENDENCIES ${_sensor_deps})
    else()
        install(TARGETS sensor LIBRARY DESTINATION lib)
    endif()
    install(DIRECTORY include/ DESTINATION include/libsensor)
else()
    message(STATUS "libsensor: spi-hal (dependency) not found; skipping sensor target")
endif()

# ── ADS1115 センサー（I2C, 任意）───────────────────────────────
# i2c-hal が存在する場合のみビルドする独立ターゲット。
# i2c-hal を取り除いても上の sensor（MCP3008/SPI）は影響を受けない。
if(TARGET i2chal OR EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../i2c-hal/src/i2c_driver.cpp)
    add_library(ads1115 SHARED src/ads1115.cpp)

    target_include_directories(ads1115
        PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                $<INSTALL_INTERFACE:include/libsensor>
    )

    set(_ads1115_deps "")
    if(TARGET i2chal)
        # モノレポビルド: i2chal を PUBLIC リンク（ads1115.hpp が ii2c_driver.hpp を include）
        target_link_libraries(ads1115 PUBLIC i2chal)
        set(_ads1115_deps i2chal)
    else()
        # スタンドアロンビルド: I2C HAL のソースをそのままコンパイル
        target_sources(ads1115 PRIVATE ../i2c-hal/src/i2c_driver.cpp)
        target_include_directories(ads1115 PRIVATE ../common/include)
        target_include_directories(ads1115
            PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../i2c-hal/include>)
    endif()

    set_target_properties(ads1115 PROPERTIES
        VERSION   1.1.0
        SOVERSION 1
    )
    if(COMMAND eds_install_package)
        eds_install_package(PACKAGE ads1115 TARGET ads1115
            VERSION ${PROJECT_VERSION} INCLUDE_DIR libsensor
            DEPENDENCIES ${_ads1115_deps})
    else()
        install(TARGETS ads1115 LIBRARY DESTINATION lib)
    endif()
endif()

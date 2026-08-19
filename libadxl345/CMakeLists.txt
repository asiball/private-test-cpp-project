cmake_minimum_required(VERSION 3.10)
project(libadxl345 VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# spi-hal（spihal）が無い構成では、モノレポビルドの TARGET も、スタンドアロン
# フォールバックの取り込み元ソースも無いため adxl345 をビルドできない。EXISTS
# ガードで依存の有無を確認し、無ければ target 自体をスキップする（libsensor/ads1115 と同パターン）。
if(TARGET spihal OR EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../spi-hal/src/spi_driver.cpp)
    add_library(adxl345 SHARED src/adxl345.cpp)

    target_include_directories(adxl345
        PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                $<INSTALL_INTERFACE:include/libadxl345>
    )

    set(_adxl345_deps "")
    if(TARGET spihal)
        # モノレポビルド: spihal を PUBLIC リンク（adxl345.hpp が ispi_driver.hpp を include）
        target_link_libraries(adxl345 PUBLIC spihal)
        set(_adxl345_deps spihal)
    else()
        # スタンドアロンビルド（cmake -S libadxl345/）: SPI HAL のソースをそのままコンパイル
        target_sources(adxl345 PRIVATE ../spi-hal/src/spi_driver.cpp)
        target_include_directories(adxl345
            PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../spi-hal/include>)
    endif()

    set_target_properties(adxl345 PROPERTIES
        VERSION   1.1.0
        SOVERSION 1
    )

    if(COMMAND eds_install_package)
        eds_install_package(PACKAGE adxl345 TARGET adxl345
            VERSION ${PROJECT_VERSION} INCLUDE_DIR libadxl345
            DEPENDENCIES ${_adxl345_deps})
    else()
        install(TARGETS adxl345 LIBRARY DESTINATION lib)
    endif()
    install(DIRECTORY include/ DESTINATION include/libadxl345)
else()
    message(STATUS "libadxl345: spi-hal (dependency) not found; skipping adxl345 target")
endif()

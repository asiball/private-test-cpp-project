cmake_minimum_required(VERSION 3.10)
project(examples CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ADS1115 + GPIO ALERT デモ: ads1115 と gpio の両ターゲットが揃う時のみビルド。
# どちらかを取り除いても他のビルドは壊れない（このサンプルがスキップされるだけ）。
if(TARGET ads1115 AND TARGET gpio)
    add_executable(ads1115_alert_demo ads1115_alert_demo.cpp)
    target_link_libraries(ads1115_alert_demo PRIVATE ads1115 gpio)
    target_include_directories(ads1115_alert_demo PRIVATE
        ${CMAKE_SOURCE_DIR}/libsensor/include
        ${CMAKE_SOURCE_DIR}/gpio/include
    )
    install(TARGETS ads1115_alert_demo RUNTIME DESTINATION bin)
else()
    message(STATUS "examples: ads1115_alert_demo をスキップ（ads1115 / gpio が必要）")
endif()

# ADXL345（SPI レジスタ型）基本サンプル: libadxl345 が揃う時のみビルド
if(TARGET adxl345)
    add_executable(adxl345_basic adxl345_basic.cpp)
    target_link_libraries(adxl345_basic PRIVATE adxl345)
    target_include_directories(adxl345_basic PRIVATE
        ${CMAKE_SOURCE_DIR}/libadxl345/include
        ${CMAKE_SOURCE_DIR}/spi-hal/include
    )
    install(TARGETS adxl345_basic RUNTIME DESTINATION bin)
else()
    message(STATUS "examples: adxl345_basic をスキップ（adxl345 が必要）")
endif()

# GPIO エッジ割り込みサンプル: gpio が揃う時のみビルド
if(TARGET gpio)
    add_executable(gpio_edge_demo gpio_edge_demo.cpp)
    target_link_libraries(gpio_edge_demo PRIVATE gpio)
    target_include_directories(gpio_edge_demo PRIVATE
        ${CMAKE_SOURCE_DIR}/gpio/include
    )
    install(TARGETS gpio_edge_demo RUNTIME DESTINATION bin)
else()
    message(STATUS "examples: gpio_edge_demo をスキップ（gpio が必要）")
endif()

# KernelSpiDriver 最小サンプル: spi-hal が揃う時のみビルド
if(TARGET spihal)
    add_executable(kernel_spi_demo kernel_spi_demo.cpp)
    target_link_libraries(kernel_spi_demo PRIVATE spihal)
    target_include_directories(kernel_spi_demo PRIVATE
        ${CMAKE_SOURCE_DIR}/spi-hal/include
    )
    install(TARGETS kernel_spi_demo RUNTIME DESTINATION bin)
else()
    message(STATUS "examples: kernel_spi_demo をスキップ（spihal が必要）")
endif()

# I2cDriver 生アクセスサンプル: i2c-hal が揃う時のみビルド
if(TARGET i2chal)
    add_executable(i2c_raw_demo i2c_raw_demo.cpp)
    target_link_libraries(i2c_raw_demo PRIVATE i2chal)
    target_include_directories(i2c_raw_demo PRIVATE
        ${CMAKE_SOURCE_DIR}/i2c-hal/include
    )
    install(TARGETS i2c_raw_demo RUNTIME DESTINATION bin)
else()
    message(STATUS "examples: i2c_raw_demo をスキップ（i2chal が必要）")
endif()

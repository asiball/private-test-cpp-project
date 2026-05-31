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

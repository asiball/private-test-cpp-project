cmake_minimum_required(VERSION 3.10)
project(libdevice VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(device SHARED src/device.cpp)

if(TARGET spi_driver)
    # モノレポビルド: driver サブディレクトリが先に処理されている場合はリンク
    target_link_libraries(device PRIVATE spi_driver pthread)
else()
    # スタンドアロンビルド（cmake -S lib/）: ドライバソースをそのままコンパイル
    target_sources(device PRIVATE ../driver/src/spi_driver.cpp)
    target_link_libraries(device pthread)
endif()

target_include_directories(device
    PUBLIC  include
            ../driver/include
)

set_target_properties(device PROPERTIES
    VERSION   1.1.0
    SOVERSION 1
)

install(TARGETS device LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/device)

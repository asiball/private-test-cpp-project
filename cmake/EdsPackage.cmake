# EdsPackage.cmake — find_package 対応の install(EXPORT) + Config 生成を共通化（issue #47）
#
# このモジュールはトップレベル CMakeLists からのみ include される。
# 各コンポーネントは `if(COMMAND eds_install_package)` でガードして呼ぶため、
# 単体（standalone）ビルド時はこの関数が未定義となり、従来どおりの素の
# install(TARGETS) にフォールバックする（モノレポビルドのときだけ find_package
# 用の Config を生成する）。
include_guard(GLOBAL)
include(CMakePackageConfigHelpers)

# eds_install_package(
#   PACKAGE       <name>        # find_package(<name>) で探す名前（例: spihal）
#   TARGET        <target>      # エクスポートするターゲット
#   VERSION       <ver>         # ConfigVersion 用バージョン
#   INCLUDE_DIR   <subdir>      # ヘッダのインストール先サブディレクトリ（include/<subdir>）
#   [DEPENDENCIES <pkg> ...]    # Config 内で find_dependency する依存パッケージ
# )
#
# 生成物（lib/cmake/<PACKAGE>/ にインストール）:
#   <PACKAGE>Config.cmake / <PACKAGE>ConfigVersion.cmake / <PACKAGE>Targets.cmake
# 名前空間は eds:: に統一する（消費側は eds::<target> でリンク）。
function(eds_install_package)
    set(oneValueArgs PACKAGE TARGET VERSION INCLUDE_DIR)
    set(multiValueArgs DEPENDENCIES)
    cmake_parse_arguments(EP "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    install(TARGETS ${EP_TARGET}
        EXPORT  ${EP_PACKAGE}Targets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)

    install(EXPORT ${EP_PACKAGE}Targets
        FILE      ${EP_PACKAGE}Targets.cmake
        NAMESPACE eds::
        DESTINATION lib/cmake/${EP_PACKAGE})

    # Config テンプレートに埋め込む find_dependency ブロックを組み立てる
    set(EDS_PACKAGE_NAME "${EP_PACKAGE}")
    set(EDS_PACKAGE_DEPENDENCIES "")
    if(EP_DEPENDENCIES)
        set(EDS_PACKAGE_DEPENDENCIES "include(CMakeFindDependencyMacro)")
        foreach(_dep ${EP_DEPENDENCIES})
            string(APPEND EDS_PACKAGE_DEPENDENCIES "\nfind_dependency(${_dep})")
        endforeach()
    endif()

    configure_package_config_file(
        ${CMAKE_SOURCE_DIR}/cmake/eds-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/${EP_PACKAGE}Config.cmake
        INSTALL_DESTINATION lib/cmake/${EP_PACKAGE})

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/${EP_PACKAGE}ConfigVersion.cmake
        VERSION       ${EP_VERSION}
        COMPATIBILITY SameMajorVersion)

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${EP_PACKAGE}Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${EP_PACKAGE}ConfigVersion.cmake
        DESTINATION lib/cmake/${EP_PACKAGE})
endfunction()

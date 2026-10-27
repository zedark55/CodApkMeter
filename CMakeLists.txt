cmake_minimum_required(VERSION 3.22)
project(CodApkMeter VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Bluetooth)
qt_standard_project_setup(REQUIRES 6.8)

include(FetchContent)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(mbedtls
    URL https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.5/mbedtls-3.6.5.tar.bz2
    URL_HASH SHA256=4a11f1777bb95bf4ad96721cac945a26e04bf19f57d905f241fe77ebeddf46d8
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(mbedtls)

qt_add_library(metercore STATIC src/protocol.cpp src/protocol.h)
target_link_libraries(metercore PUBLIC Qt6::Core PRIVATE mbedcrypto)
target_include_directories(metercore PUBLIC src)
qt_add_executable(codapkmeter src/main.cpp src/backend.cpp src/backend.h src/keyvault.cpp src/keyvault.h)
target_link_libraries(codapkmeter PRIVATE metercore mbedcrypto Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Bluetooth)
qt_add_qml_module(codapkmeter URI CodApkMeter VERSION 1.0
    QML_FILES qml/Main.qml qml/components/Panel.qml qml/components/ActionButton.qml
              qml/components/MetricCard.qml qml/components/SectionLabel.qml qml/components/FormField.qml
    RESOURCES protocols/demo-water.json protocols/demo-electricity.json)
set_target_properties(codapkmeter PROPERTIES
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
    QT_ANDROID_PACKAGE_NAME "org.codapkmeter.app"
    QT_ANDROID_MIN_SDK_VERSION 26
    QT_ANDROID_TARGET_SDK_VERSION 35
    QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
    QT_ANDROID_VERSION_CODE 1)
if(ANDROID)
    # New or edited package assets must invalidate the APK, including files
    # absent from androiddeployqt's previous dependency file.
    file(GLOB_RECURSE android_package_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/android/*")
    set_property(TARGET codapkmeter APPEND PROPERTY LINK_DEPENDS ${android_package_files})
endif()

include(CTest)
if(BUILD_TESTING AND NOT ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(protocol_tests tests/tst_protocol.cpp)
    target_link_libraries(protocol_tests PRIVATE metercore Qt6::Test mbedcrypto)
    target_compile_definitions(protocol_tests PRIVATE PROTOCOL_DIR="${CMAKE_CURRENT_SOURCE_DIR}/protocols")
    add_test(NAME protocols COMMAND protocol_tests)
    qt_add_executable(backend_tests tests/tst_backend.cpp src/backend.cpp src/backend.h src/keyvault.cpp src/keyvault.h)
    target_link_libraries(backend_tests PRIVATE metercore Qt6::Bluetooth Qt6::Test mbedcrypto)
    qt_add_resources(backend_tests "fixtures" PREFIX "/qt/qml/CodApkMeter"
        FILES protocols/demo-water.json protocols/demo-electricity.json)
    add_test(NAME backend COMMAND backend_tests)
endif()
install(TARGETS codapkmeter RUNTIME DESTINATION bin LIBRARY DESTINATION lib)

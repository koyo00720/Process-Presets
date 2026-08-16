cmake_minimum_required(VERSION 3.21)

# This is the sole authoritative application version.  It drives the title,
# QCoreApplication metadata, and the Windows executable version resource.
project(ProcessPresets VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

if(WIN32)
    configure_file(
        src/version.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/processpresets_version.rc
        @ONLY
    )
endif()

qt_add_executable(ProcessPresets WIN32
    src/main.cpp
    src/icon.rc
    ${CMAKE_CURRENT_BINARY_DIR}/processpresets_version.rc
    src/models.cpp
    src/settingsrepository.cpp
    src/settingsdialog.cpp
    src/cpudiscovery.cpp
    src/processhelper.cpp
    src/processselectiondialog.cpp
    src/hierarchicalheaderview.cpp
    src/cpustructuredialog.cpp
    src/mainwindow.cpp
)

qt_add_resources(ProcessPresets "processpresets_resources"
    PREFIX "/"
    FILES src/icon.png
)

target_include_directories(ProcessPresets PRIVATE src)
target_link_libraries(ProcessPresets PRIVATE Qt6::Widgets)
target_compile_definitions(ProcessPresets PRIVATE PROCESS_PRESETS_VERSION="${PROJECT_VERSION}")

if(WIN32)
    target_compile_definitions(ProcessPresets PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    target_link_libraries(ProcessPresets PRIVATE kernel32)
endif()

if(MSVC)
    target_compile_options(ProcessPresets PRIVATE /utf-8)
endif()

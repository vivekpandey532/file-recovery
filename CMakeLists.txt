cmake_minimum_required(VERSION 3.16)
project(FileRecovery VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

set(SOURCES
    main.cpp
    core/DiskScanner.cpp
    core/NTFSParser.cpp
    core/FATParser.cpp
    core/FileCarver.cpp
    core/FileRecovery.cpp
    ui/MainWindow.cpp
    ui/ScanDialog.cpp
    ui/ResultsView.cpp
)

set(HEADERS
    core/DiskScanner.h
    core/NTFSParser.h
    core/FATParser.h
    core/FileCarver.h
    core/FileRecovery.h
    ui/MainWindow.h
    ui/ScanDialog.h
    ui/ResultsView.h
)

add_executable(FileRecovery WIN32 ${SOURCES} ${HEADERS})

target_link_libraries(FileRecovery PRIVATE Qt6::Widgets Qt6::Concurrent)

# Required for raw disk access on Windows
if(WIN32)
    target_link_libraries(FileRecovery PRIVATE setupapi)
endif()

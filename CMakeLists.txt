cmake_minimum_required(VERSION 3.20)
project(safemap_kyiv)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(SOURCES
        main.cpp

        Infrastructure.h
        Infrastructure.cpp
        Establishments.h
        Establishments.cpp
        Crowdsourcing.h
        Crowdsourcing.cpp
        RoutePlanner.h
        RoutePlanner.cpp

        MainWindow.h
        MainWindow.cpp
        ReportDialog.h
        ReportDialog.cpp
        DetailsDialog.h
        DetailsDialog.cpp
        RouteDialog.h
        RouteDialog.cpp
)

add_executable(safemap_kyiv ${SOURCES})

target_link_libraries(safemap_kyiv PRIVATE Qt6::Widgets)
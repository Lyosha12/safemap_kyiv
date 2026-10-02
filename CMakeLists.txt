cmake_minimum_required(VERSION 3.20)
project(safemap_kyiv)

set(CMAKE_CXX_STANDARD 17)

add_executable(safemap_kyiv
        main.cpp
        Infrastructure.h
        Infrastructure.cpp
        Establishments.h
        Establishments.cpp
        Crowdsourcing.cpp
        Crowdsourcing.h
        RoutePlanner.cpp
        RoutePlanner.h
)
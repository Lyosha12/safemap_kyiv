cmake_minimum_required(VERSION 3.20)
project(safemap_kyiv)

set(CMAKE_CXX_STANDARD 17)

# Вказуємо підтримку Windows 10+ для MinGW та cpp-httplib
add_compile_definitions(_WIN32_WINNT=0x0A00)

include_directories(include)

add_executable(safemap_kyiv
        main.cpp
        Infrastructure.h
        Infrastructure.cpp
        Establishments.h
        Establishments.cpp
        Crowdsourcing.h
        Crowdsourcing.cpp
        RoutePlanner.h
        RoutePlanner.cpp
)

if(WIN32)
    target_link_libraries(safemap_kyiv ws2_32 crypt32)
endif()
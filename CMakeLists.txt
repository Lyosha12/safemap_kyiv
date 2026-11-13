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
        EstablishmentFactory.h
        EstablishmentFactory.cpp
        DataLoader.h
        DataLoader.cpp
        AlertMonitor.h
        AlertMonitor.cpp
)

find_package(Threads REQUIRED)
target_link_libraries(safemap_kyiv Threads::Threads)

if(MINGW AND NOT OPENSSL_ROOT_DIR)
    get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(OPENSSL_ROOT_DIR "${MINGW_BIN_DIR}/.." ABSOLUTE)
endif()
find_package(OpenSSL 3.0)
if(OPENSSL_FOUND)
    target_compile_definitions(safemap_kyiv PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT SAFEMAP_REAL_ALERTS)
    target_link_libraries(safemap_kyiv OpenSSL::SSL OpenSSL::Crypto)
    message(STATUS "SafeMap: OpenSSL ${OPENSSL_VERSION} знайдено, реальна тривога увімкнена")
else()
    message(WARNING "SafeMap: OpenSSL не знайдено, режим реальної тривоги буде недоступний")
endif()

if(WIN32)
    target_link_libraries(safemap_kyiv ws2_32 crypt32)
endif()

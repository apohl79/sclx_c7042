project(sclx)
cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

aux_source_directory(. SOURCES)

find_package(PkgConfig)
pkg_check_modules(TASKS libtasks>=1.6 REQUIRED)
find_package(JsonCpp REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost 1.41.0 COMPONENTS system REQUIRED)

add_definitions(-g -Wall -Wextra -Wlong-long -Wmissing-braces -std=c++1y -pthread)
# hide some warnings for the websocket server
add_definitions(-Wno-deprecated-declarations -Wno-unused-parameter)

include_directories(${TASKS_INCLUDE_DIRS})
include_directories(${JSONCPP_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${TASKS_LIBRARY_DIRS})
link_directories(${JSONCPP_LIBRARY_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} ${TASKS_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${JSONCPP_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${OPENSSL_CRYPTO_LIBRARIES})

cmake_minimum_required(VERSION 2.8.9)
project(aquachain-miner CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")   #All warning
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra") #Extra warning flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static") # static cflag
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread") # pthread cflag
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3") # optimize
file (STRINGS "VERSION" VERSION)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVERSION=\"${VERSION}\"")
#add_compile_definitions(VERSION=\""${VERSION}"\")

# sources for exe
file(GLOB SOURCES "src/*.cpp")
add_executable(${PROJECT_NAME} ${SOURCES})

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/aquahash/include)
include_directories(${CMAKE_SOURCE_DIR}/spdlog/include)
include_directories(${CMAKE_SOURCE_DIR}/vend/include)
include_directories(/tmp/curl/include)

# link libjsoncpp, libgmp, libaquahash, and libspdlog
target_link_libraries(${PROJECT_NAME} -ljsoncpp)
target_link_libraries(${PROJECT_NAME} -lgmp)
target_link_libraries(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/aquahash/libaquahash.a)
target_link_libraries(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/spdlog/libspdlog.a)

# these are from the output of /tmp/curl/bin/curl-config --static-libs
target_link_libraries(${PROJECT_NAME} /tmp/curl/lib/libcurl.a -lcares -lz)

# link pthread last
target_link_libraries(${PROJECT_NAME} -lpthread)


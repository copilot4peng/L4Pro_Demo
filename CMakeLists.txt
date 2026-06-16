cmake_minimum_required(VERSION 3.10)

project(L4Pro_Demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(searchlight_demo
    main.cpp
    src/SearchlightLogger.cpp
    src/SearchlightProtocol.cpp
    src/SearchlightConfig.cpp
    src/SearchlightState.cpp
    src/SerialPort.cpp
    src/SearchlightCommandBuilder.cpp
    src/SearchlightLedController.cpp
    src/SearchlightServoController.cpp
    src/Searchlight.cpp
    src/SearchlightManager.cpp
)

target_include_directories(searchlight_demo PRIVATE src)

find_package(Threads REQUIRED)
target_link_libraries(searchlight_demo PRIVATE Threads::Threads)

# add_executable(protocol_smoke_test
#     tests/protocol_smoke_test.cpp
#     src/SearchlightLogger.cpp
#     src/SearchlightProtocol.cpp
#     src/SearchlightCommandBuilder.cpp
# )

# target_include_directories(protocol_smoke_test PRIVATE src)

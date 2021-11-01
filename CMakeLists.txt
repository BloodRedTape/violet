cmake_minimum_required(VERSION 3.10)

project(violet)

set(SFML_BUILD_WINDOW FALSE)
set(SFML_BUILD_AUDIO FALSE)
set(SFML_BUILD_GRAPHICS FALSE)
add_subdirectory(SFML)

set(VIOLET_SOURCES
    main.cpp
)

add_executable(violet ${VIOLET_SOURCES})
target_link_libraries(violet sfml-system sfml-network fmt)
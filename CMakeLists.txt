cmake_minimum_required(VERSION 3.25)
project(ClickBeyondFrames)

find_package(Geode REQUIRED)

geode_add_mod(
    TARGET CBFBeyond
    SOURCES src/main.cpp
    SOURCES src/Checker.cpp  # We'll split later if needed
    INCLUDES include
    RESOURCES resources
)

geode_finalize_mod()

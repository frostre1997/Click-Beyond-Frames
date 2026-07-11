cmake_minimum_required(VERSION 3.25)
project(ClickBeyondFramesAPI)

geode_add_mod(
    TARGET ClickBeyondFramesAPI
    SOURCES
        src/API.cpp
        src/Checker.cpp
    INCLUDES
        include
)

geode_finalize_mod()

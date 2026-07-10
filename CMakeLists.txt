cmake_minimum_required(VERSION 3.25)
project(ClickBeyondFrames)

find_package(Geode REQUIRED)

geode_add_mod(
    TARGET ClickBeyondFrames
    SOURCES src/main.cpp
    RESOURCES resources
)

# If using submodule, add the include path
# target_include_directories(ClickBeyondFrames PRIVATE api/include)

geode_finalize_mod()

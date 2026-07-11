#pragma once
#include <Geode/Result.hpp>
#include <string>

namespace cbfbeyond::dev {

    // Returns true if Click Beyond Frames is installed
    inline bool isInstalled() {
        return true;
    }

    // Returns the current mod version
    std::string getVersion();

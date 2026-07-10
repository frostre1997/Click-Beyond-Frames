#pragma once
#include <Geode/Result.hpp>
#include <string>

namespace clickbeyondframes::dev {

    // Returns true if Click Beyond Frames is installed (this header existing implies it)
    inline bool isInstalled() {
        return true;
    }

    // Returns the current mod version
    std::string getVersion();

}

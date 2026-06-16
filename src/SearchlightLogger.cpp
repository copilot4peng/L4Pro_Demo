#include "SearchlightLogger.h"

#include <iostream>
#include <mutex>

namespace SearchlightControl {

void print_log(std::string message) {
    static std::mutex log_mutex;
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cout << "[SearchlightManager] " << message << std::endl;
}

}  // namespace SearchlightControl

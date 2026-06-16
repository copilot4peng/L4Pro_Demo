#ifndef SEARCHLIGHT_CONFIG_H
#define SEARCHLIGHT_CONFIG_H

#include <string>

namespace SearchlightControl {

// 初始化参数类：当前协议只允许使用串口路径和固定波特率连接设备。
class SearchlightInitConfig {
public:
    SearchlightInitConfig();

    std::string driver;
    int baud_rate;
    bool print_raw_serial;
    int init_wait_ms;
    int offline_timeout_ms;
};

}  // namespace SearchlightControl

#endif  // SEARCHLIGHT_CONFIG_H

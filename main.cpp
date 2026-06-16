#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "SearchlightLogger.h"
#include "SearchlightManager.h"

using SearchlightControl::Searchlight;
using SearchlightControl::SearchlightInitConfig;
using SearchlightControl::SearchlightManager;
using SearchlightControl::print_log;

namespace {

void printUsage(const char* app) {
    std::cout << "用法: " << app << " [--port /dev/tty.xxx] [--baud 115200] [--raw|--no-raw]\n"
              << "启动后可输入命令:\n"
              << "  status                 查看状态\n"
              << "  home                   伺服回中\n"
              << "  move <x> <y>           伺服移动到绝对角度\n"
              << "  led on|off             LED 开关\n"
              << "  light <0-100>          设置亮度\n"
              << "  flash on|off           闪烁开关\n"
              << "  hz <1-50>              设置闪烁频率\n"
              << "  quit                   退出\n";
}

bool parseInt(const std::string& text, int& value) {
    char* end = NULL;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == NULL || *end != '\0') {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    SearchlightInitConfig config;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--port" && i + 1 < argc) {
            config.driver = argv[++i];
        } else if (arg == "--baud" && i + 1 < argc) {
            int baud = 0;
            if (!parseInt(argv[++i], baud)) {
                std::cerr << "波特率参数非法\n";
                return 1;
            }
            config.baud_rate = baud;
        } else if (arg == "--raw") {
            config.print_raw_serial = true;
        } else if (arg == "--no-raw") {
            config.print_raw_serial = false;
        } else {
            std::cerr << "未知参数: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    SearchlightManager& manager = SearchlightManager::getInstance();
    manager.initialize(config);

    print_log("命令行：探照灯控制台已启动，输入 help 查看命令");
    std::string line;
    while (std::cout << "searchlight> " && std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        if (cmd.empty()) {
            continue;
        }
        if (cmd == "help") {
            printUsage(argv[0]);
        } else if (cmd == "quit" || cmd == "exit") {
            break;
        } else if (cmd == "status") {
            std::string status;
            manager.get_status(status);
            std::cout << status << std::endl;
        } else if (cmd == "home") {
            manager.home();
        } else if (cmd == "move") {
            int x = 0;
            int y = 0;
            if (!(iss >> x >> y)) {
                print_log("命令行：move 需要两个整数参数，例如 move 30 20");
                continue;
            }
            manager.move_to(x, y);
        } else if (cmd == "led") {
            std::string value;
            iss >> value;
            if (value == "on") {
                manager.open_led();
            } else if (value == "off") {
                manager.close_led();
            } else {
                print_log("命令行：led 参数只能是 on 或 off");
            }
        } else if (cmd == "light") {
            int level = 0;
            if (!(iss >> level)) {
                print_log("命令行：light 需要亮度整数，例如 light 50");
                continue;
            }
            manager.setting_light_level(level);
        } else if (cmd == "flash") {
            std::string value;
            iss >> value;
            if (value == "on") {
                manager.open_flash();
            } else if (value == "off") {
                manager.close_flash();
            } else {
                print_log("命令行：flash 参数只能是 on 或 off");
            }
        } else if (cmd == "hz") {
            int hz = 0;
            if (!(iss >> hz)) {
                print_log("命令行：hz 需要频率整数，例如 hz 10");
                continue;
            }
            manager.setting_flash(hz);
        } else {
            print_log("命令行：未知命令 " + cmd);
        }
    }

    manager.shutdown();
    return 0;
}

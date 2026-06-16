#include "Searchlight.h"

#include <chrono>
#include <sstream>

#include "SearchlightLogger.h"
#include "SearchlightProtocol.h"

namespace SearchlightControl {

LightTask::LightTask() {}

LightTask::LightTask(const std::string& command_name, const std::vector<uint8_t>& frame_bytes)
    : command(command_name), parameters(frame_bytes) {}

Searchlight::Searchlight()
    : serial_(new SerialPort()),
      led_controller_(builder_),
      servo_controller_(builder_),
      running_(false) {}

Searchlight::~Searchlight() {
    shutdown();
}

void Searchlight::initialize(const SearchlightInitConfig& config) {
    shutdown();
    config_ = config;

    print_log("初始化：开始初始化探照灯管理器");
    print_log("初始化：驱动=" + config_.driver + "，波特率=" +
              std::to_string(config_.baud_rate) +
              "，原始串口日志=" + std::string(config_.print_raw_serial ? "开启" : "关闭"));

    if (config_.baud_rate != 115200) {
        print_log("初始化：协议要求固定 115200bps，当前参数非法，状态置为【无设备】");
        state_.markNoDevice("波特率非法，探照灯协议固定为 115200bps");
        return;
    }

    if (!serial_->openPort(config_.driver, config_.baud_rate)) {
        state_.markNoDevice("串口打开失败，设备不存在或权限不足");
        print_log("初始化：串口打开失败，状态置为【无设备】");
        return;
    }

    running_.store(true);
    read_serial = std::thread(&Searchlight::readSerialLoop, this);
    worker_thread = std::thread(&Searchlight::workerLoop, this);
    waitInitialDeviceFrame();
}

bool Searchlight::online() {
    return state_.isOnline(config_.offline_timeout_ms);
}

bool Searchlight::setting_light_level(int level) {
    if (!ensureControlReady("设置亮度")) {
        return false;
    }
    std::vector<uint8_t> frame;
    {
        std::lock_guard<std::mutex> lock(build_mutex_);
        if (!led_controller_.buildLightLevel(level, frame)) {
            return false;
        }
    }
    return enqueueCommand("设置亮度 " + std::to_string(level), frame);
}

bool Searchlight::setting_flash(int hz) {
    if (!ensureControlReady("设置闪烁频率")) {
        return false;
    }
    std::vector<uint8_t> frame;
    {
        std::lock_guard<std::mutex> lock(build_mutex_);
        if (!led_controller_.buildFlashHz(hz, frame)) {
            return false;
        }
    }
    return enqueueCommand("设置闪烁频率 " + std::to_string(hz) + "Hz", frame);
}

bool Searchlight::open_flash() {
    if (!ensureControlReady("开启闪烁")) {
        return false;
    }
    std::vector<uint8_t> frame;
    {
        std::lock_guard<std::mutex> lock(build_mutex_);
        if (!led_controller_.buildOpenFlash(frame)) {
            return false;
        }
    }
    return enqueueCommand("开启闪烁", frame);
}

bool Searchlight::close_flash() {
    if (!ensureControlReady("关闭闪烁")) {
        return false;
    }
    std::vector<uint8_t> frame;
    {
        std::lock_guard<std::mutex> lock(build_mutex_);
        if (!led_controller_.buildCloseFlash(frame)) {
            return false;
        }
    }
    return enqueueCommand("关闭闪烁", frame);
}

bool Searchlight::home() {
    if (!ensureControlReady("伺服回中")) {
        return false;
    }
    std::vector<uint8_t> frame;
    {
        std::lock_guard<std::mutex> lock(build_mutex_);
        if (!servo_controller_.buildHome(frame)) {
            return false;
        }
    }
    return enqueueCommand("伺服回中", frame);
}

bool Searchlight::move_to(int x, int y) {
    if (!ensureControlReady("伺服绝对角度控制")) {
        return false;
    }
    std::vector<uint8_t> frame;
    {
        std::lock_guard<std::mutex> lock(build_mutex_);
        if (!servo_controller_.buildMoveTo(x, y, frame)) {
            return false;
        }
    }
    return enqueueCommand("伺服移动 x=" + std::to_string(x) + ",y=" + std::to_string(y), frame);
}

bool Searchlight::open_led() {
    if (!ensureControlReady("开灯")) {
        return false;
    }
    std::vector<uint8_t> frame;
    {
        std::lock_guard<std::mutex> lock(build_mutex_);
        if (!led_controller_.buildOpenLed(frame)) {
            return false;
        }
    }
    return enqueueCommand("开灯", frame);
}

bool Searchlight::close_led() {
    if (!ensureControlReady("关灯")) {
        return false;
    }
    std::vector<uint8_t> frame;
    {
        std::lock_guard<std::mutex> lock(build_mutex_);
        if (!led_controller_.buildCloseLed(frame)) {
            return false;
        }
    }
    return enqueueCommand("关灯", frame);
}

void Searchlight::get_status(std::string& status) {
    online();
    status = state_.toStatusString();
}

void Searchlight::shutdown() {
    const bool was_running = running_.exchange(false);
    if (!was_running && !serial_->isOpen()) {
        return;
    }

    print_log("关闭：准备停止探照灯管理器线程");
    queue_cv_.notify_all();

    if (read_serial.joinable()) {
        read_serial.join();
    }
    if (worker_thread.joinable()) {
        worker_thread.join();
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.clear();
    }

    serial_->closePort();
    print_log("关闭：探照灯管理器已停止");
}

bool Searchlight::enqueueCommand(const std::string& command, const std::vector<uint8_t>& frame) {
    if (frame.empty()) {
        print_log("队列：指令 " + command + " 生成失败，未入队");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push_back(LightTask(command, frame));
    }
    state_.markCommandQueued(command);
    print_log("队列：指令已入队，command=" + command + "，HEX=" + ProtocolCodec::toHex(frame));
    queue_cv_.notify_one();
    return true;
}

bool Searchlight::ensureControlReady(const std::string& command) {
    if (!serial_->isOpen()) {
        print_log("控制：" + command + " 被拒绝，串口未打开");
        return false;
    }
    if (!online()) {
        print_log("控制：" + command + " 被拒绝，当前状态不是【在线】");
        return false;
    }
    return true;
}

void Searchlight::readSerialLoop() {
    print_log("读线程：启动，开始监听串口数据");
    std::vector<uint8_t> receive_buffer;
    uint8_t temp[256];

    while (running_.load()) {
        const ssize_t n = serial_->readBytes(temp, sizeof(temp), 100);
        if (n < 0) {
            state_.markNoDevice("串口读取失败");
            print_log("读线程：串口读取失败，状态置为【无设备】");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (n > 0) {
            std::vector<uint8_t> raw(temp, temp + n);
            if (config_.print_raw_serial) {
                print_log("读线程：收到串口原始数据，HEX=" + ProtocolCodec::toHex(raw));
            }
            receive_buffer.insert(receive_buffer.end(), raw.begin(), raw.end());

            while (running_.load()) {
                DecodedFrame frame;
                std::string error;
                const bool ok = ProtocolCodec::tryDecode(receive_buffer, frame, error);
                if (!error.empty()) {
                    state_.markChecksumError(error);
                    print_log("协议：解析失败，" + error);
                }
                if (!ok) {
                    break;
                }

                state_.markOnlineByFrame(frame);
                // print_log("协议：收到有效帧，payload=" + ProtocolCodec::toHex(frame.payload) +
                //           "，计数器=" + std::to_string(static_cast<int>(frame.frame_counter)));
            }
        } else {
            online();
        }
    }

    print_log("读线程：退出");
}

void Searchlight::workerLoop() {
    print_log("工作线程：启动，开始消费指令队列");
    while (running_.load()) {
        LightTask task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this]() {
                return !running_.load() || !task_queue_.empty();
            });

            if (!running_.load() && task_queue_.empty()) {
                break;
            }

            task = task_queue_.front();
            task_queue_.pop_front();
        }

        std::string error;
        if (serial_->writeBytes(task.parameters, error)) {
            state_.markCommandSent(task.command);
            print_log("工作线程：指令发送成功，command=" + task.command);
        } else {
            state_.markCommandFailed(task.command, error);
            print_log("工作线程：指令发送失败，command=" + task.command + "，原因=" + error);
        }
    }
    print_log("工作线程：退出");
}

void Searchlight::waitInitialDeviceFrame() {
    print_log("初始化：等待设备主动上报串口数据，用于确认设备存在");
    const int step_ms = 50;
    int waited_ms = 0;
    while (waited_ms < config_.init_wait_ms) {
        if (online()) {
            print_log("初始化：已确认设备在线");
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(step_ms));
        waited_ms += step_ms;
    }

    state_.markNoDevice("初始化等待超时，未读到有效串口帧");
    print_log("初始化：未在 " + std::to_string(config_.init_wait_ms) +
              "ms 内读到有效串口帧，状态置为【无设备】；读线程会继续监听后续上线");
}

}  // namespace SearchlightControl

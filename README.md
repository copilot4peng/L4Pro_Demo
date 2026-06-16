# L4 Pro Searchlight Manager

这是一个 C++11 探照灯管理器示例工程，基于串口 UART 和硬件通信，使用生产者-消费者队列发送控制指令，并通过状态机维护设备在线状态、LED 状态、闪烁状态、亮度和伺服角度。

## 构建

```bash
cmake -S . -B build
cmake --build build -j 4
```

## 协议冒烟测试

```bash
./build/protocol_smoke_test
```

该测试会校验 LED 开关、亮度、闪烁频率、伺服回中和 `move_to(30, 20)` 生成的 HEX 是否与协议样例一致。

## 串口控制台

```bash
./build/searchlight_demo --port /dev/tty.usbserial-14220 --baud 115200 --raw
```

参数说明：

- `--port`：串口设备路径。
- `--baud`：波特率，协议固定为 `115200`。
- `--raw`：打印串口原始 HEX 数据。
- `--no-raw`：关闭串口原始 HEX 数据打印。

控制台命令：

- `status`：查看状态。
- `home`：伺服回中。
- `move <x> <y>`：移动到绝对角度。
- `led on|off`：LED 开关。
- `light <0-100>`：设置亮度。
- `flash on|off`：闪烁开关。
- `hz <1-50>`：设置闪烁频率。
- `quit`：退出。

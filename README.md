# r_audio_nextframe

## 项目介绍 | Introduction

r_audio_nextframe 是一个基于 C++ 和 FFmpeg 的音频处理项目，可以将音频文件转换为 MP3 格式。该程序处理输入音频文件并生成采样率为 48000 Hz 的输出文件。

r_audio_nextframe is a C++ project for audio processing that can convert audio files to MP3 format. The program processes input audio files and generates output files with a sample rate of 48000 Hz.

## 功能特性 | Features

- 将各种音频格式转换为 MP3
- 支持包括 MP3、WAV、AAC、FLAC 和 OGG 在内的输入格式
- 将音频重新采样到 48000 Hz 采样率
- 输出立体声音频
- 使用 FFmpeg 库进行音频处理
- 实现队列机制优化编码帧处理
- 支持通过UDP传输音频数据
- 提供UDP服务器接收和保存音频流

- Converts various audio formats to MP3
- Supports input formats including MP3, WAV, AAC, FLAC, and OGG
- Resamples audio to 48000 Hz sample rate
- Outputs stereo audio
- Uses FFmpeg libraries for audio processing
- Implements queue mechanism for optimized frame processing
- Supports audio data transmission via UDP
- Provides UDP server to receive and save audio streams

## 软件架构 | Software Architecture

项目的主要组件包括：

The project is structured with the following main components:

- `AudioProcessor`: 处理核心音频功能
- `FrameDecoder`: 解码音频帧
- `FrameEncoder`: 编码音频帧（当前配置为输出 MP3），实现队列机制存储编码后的帧
- `FrameReader`: 从各种来源读取音频帧
- `Resampler`: 提供音频重采样功能
- `UdpServer`: UDP服务器用于接收和保存音频流
- `main.cpp`: 应用程序入口点
- `udp_server_main.cpp`: UDP服务器应用程序入口点

- `AudioProcessor`: Handles core audio processing functionality
- `FrameDecoder`: Decodes audio frames
- `FrameEncoder`: Encodes audio frames (currently configured to output MP3), implements queue mechanism to store encoded frames
- `FrameReader`: Reads audio frames from various sources
- `Resampler`: Provides audio resampling capabilities
- `UdpServer`: UDP server for receiving and saving audio streams
- `main.cpp`: Entry point of the application
- `udp_server_main.cpp`: Entry point of the UDP server application

## 安装说明 | Installation

1. 克隆仓库
2. 确保已安装 C++ 编译器和 CMake
3. 安装 FFmpeg 库
4. 使用 CMake 构建项目：

1. Clone the repository
2. Ensure you have a C++ compiler and CMake installed
3. Install FFmpeg libraries
4. Build the project using CMake:

   ```
   mkdir build
   cd build
   cmake ..
   make
   ```

## 使用方法 | Usage

构建项目后，您可以使用音频文件作为参数运行可执行文件：

After building the project, you can run the executable with an audio file as an argument:

```
./r_audio_nextframe <input_audio_file> [udp_server_ip] [udp_server_port]
```

程序将处理输入文件并在同一目录中生成 MP3 输出文件，命名约定为 `<original_name>_48000.mp3`。同时，程序会将音频数据通过UDP发送到指定的服务器。

The program will process the input file and generate an MP3 output file in the same directory with the naming convention `<original_name>_48000.mp3`. Additionally, the program will send audio data via UDP to the specified server.

示例：
Example:

```
./r_audio_nextframe input.wav
```

这将在同一目录中生成名为 `input_48000.mp3` 的文件，并将音频数据发送到默认的UDP服务器（127.0.0.1:8080）。

This will generate a file named `input_48000.mp3` in the same directory and send audio data to the default UDP server (127.0.0.1:8080).

您也可以指定自定义的UDP服务器：

You can also specify a custom UDP server:

```
./r_audio_nextframe input.wav 192.168.1.100 9000
```

同时，您可以运行UDP服务器来接收和保存音频流：

Additionally, you can run the UDP server to receive and save audio streams:

```
./udp_server [port]
```

UDP服务器将在指定端口监听音频数据，并将接收到的数据保存为MP3文件。

The UDP server will listen for audio data on the specified port and save the received data as MP3 files.

## 实现细节 | Implementation Details

项目当前配置为始终输出 MP3 格式，无论输入格式如何。输出音频重新采样到 48000 Hz 立体声频道，并以 320 kbps 比特率编码。

The project is currently configured to always output MP3 format regardless of the input format. The output audio is resampled to 48000 Hz with stereo channels and encoded at 320 kbps bitrate.

## 贡献指南 | Contributing

1. Fork 此仓库
2. 为您的功能创建新分支
3. 提交您的更改
4. 推送到您的分支
5. 创建新的 Pull Request

1. Fork this repository
2. Create a new branch for your feature
3. Commit your changes
4. Push to your branch
5. Create a new Pull Request

## 许可证 | License

本项目采用 MIT 许可证。

This project is licensed under the MIT License.

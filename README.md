# Redis 源码学习

## Windows 环境使用教程

### 环境

- Windows 10
- CLion 2021.1.3
- Cygwin 3.2.0-1

### 操作步骤

1. [下载并安装 CLion](https://www.jetbrains.com/clion/)
2. [下载并安装 Cygwin](http://www.cygwin.com/)
3. 通过 Cygwin 安装: make, cmake, gdb, gcc-g++, gcc_core, binutils
4. 通过 CLion 导入本项目, 选择 Cmake 项目
5. 打开 CLion 的设置, 配置工具链为 Cygwin
6. 通过 git bash 执行 src/mkreleasehdr.sh 生成 release.h 文件
7. 构建项目
8. 在右上角选择 redis-server 并运行, 正常启动

### 已知的问题
 
1. redis-server 通过 run 模式启动会导致 exe 不能关闭, 端口会被占用, debug 模式正常
2. redis-cli 通过 run 模式启动在输入输出时会不正常, debug 模式正常

## MacOS 环境使用教程

### 环境

- MacOS Big Sur
- MacBook m1 pro

### 操作步骤

1. [下载并安装 CLion](https://www.jetbrains.com/clion/)
2. 通过 CLion 导入本项目, 选择 Cmake 项目
3. 执行 src/mkreleasehdr.sh 生成 release.h 文件
4. 构建项目
5. 在右上角选择 redis-server 并运行, 正常启动

### 已知的问题

1. redis-cli 在 run/debug 模式下在输入输出时会不正常

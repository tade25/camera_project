# camera project
基于i.MX6ULL平台的高性能V4L2摄像头采集项目，集成NXP PXP硬件加速，实现1280x720@30fps全链路零拷贝LCD 实时显示，支持标准v4l2-ctl工具动态调节画质参数

## 功能特性
- 标准V4L2子设备架构，支持OV5640 CSI摄像头自动探测绑定
- NXP PXP硬件转码引擎，YUYV→RGB565零CPU占用，彻底解决软件转码帧率瓶颈
- 物理地址直传零拷贝技术，全流程无内存复制，帧率较原始方案提升200%
- 多分辨率支持：640x480@30fps、1280x720@30fps两种主流分辨率无缝切换
- 实时画质调节：支持v4l2-ctl工具动态调整亮度、对比度、色彩饱和度
- 三线程异步架构：主线程 + 采集线程 + 处理线程，配合环形缓冲区，帧丢失率 < 0.1%
- 完善的异常处理：超时重试、资源自动回收、全流程错误码返回

## 硬件平台
- 主控：NXP i.MX6ULL(ARM Cortex-A7@800MHz)
- 摄像头：OV5640 CSI 500 万像素摄像头
- 显示：7寸RGB LCD显示屏(1024x600)
- 开发板：正点原子i.MX6ULL开发板

## 软件环境
- 主机系统：Ubuntu 18.04.6
- 交叉编译器：arm-linux-gnueabihf-gcc 4.9.0
- 内核版本：Linux 4.1.15
- 构建工具：CMake 3.16、GNU Make

## 目录结构
```txt
camera-project/
├── app/                    # 应用层代码
│   ├── common/             # 通用工具函数
│   ├── core/               # 核心业务逻辑
│   ├── device/             # 设备抽象层
│   ├── service/            # 业务服务层
│   └── main.c              # 程序入口
├── kernel/                 # 内核驱动代码
│   ├── driver/             # 驱动代码
│   ├── dts/                # 设备树文件
├── images/                 # 镜像文件
├── README.md               # 项目说明文档
└── CHANGELOG.md            # 完整更新日志
```

## 最新更新
### v0.4.3 (2026-05-26)
- 新增CHANGELOG.md, 并将README.md中的提交历史全部迁移到该文件
- 完善README.md
- 统一项目版本号体系，修正历史版本号错误

完整历史更新日志请查看 [CHANGELOG.md](CHANGELOG.md)

## 已知问题

## TODO

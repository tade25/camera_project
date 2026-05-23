# camera project

## v0.1.0(2026-04-21)
- 基础工程

## v0.2.0(2026-05-07)
- 新增实时fps统计与打印
- 集成PXP硬件转码, 替代原CPU软件实现的YUYV转RGB565, 解决帧率过低问题
- 改造驱动及应用层代码，通过物理地址直传实现零拷贝，进一步提升帧率
- 修复系统运行一段时间后画面出现垂直分割的问题, 640x480分辨率全流程30fps稳定运行

## v0.3.0(2026-05-16)
- dts新增csi与ov5640的v4l2子设备port/endpoint绑定
- 驱动重构为标准V4L2子设备架构, 实现异步子设备自动探测绑定
- 驱动支持640x480、1280x720两种主流分辨率
- 驱动完善资源管理与全流程异常错误处理
- 驱动补齐全套V4L2标准接口，支持帧格式、分辨率、帧率信息枚举与流参数配置
- 应用层深度适配底层新特性
- 应用层采用poll非阻塞采集模式，增加超时重试容错逻辑

## v0.3.1(2026-05-20)
- 完成应用层解耦，按common/core/device/service分层重构架构
- 引入主线程、采集线程、处理线程的三线程模型，结合环形缓冲区与条件变量，降低帧丢失概率
- 优化CMake构建配置，启用-Wall、-Wextra, 并清理存在的警告
- 去除PXP/LCD相关硬编码参数，提升代码可维护性
- 补充并修正README.md

## v0.3.2(2026-05-21)
- 修复imx6ull_stop_streaming中直接访问active_fb1/active_fb2但未判断是否为空的问题
- 修复my_imx6ull_csi_remove中使用csi_dev->dcic/mclk/axi但probe中未初始化的问题
- 修复ring_buffer_set时buffer满未释放互斥锁的问题
- 优化camera_capture函数中的超时重试机制修改为do while

## v0.3.3(2026-05-25)
- 删除应用层camera.c中的枚举格式，帧大小，帧率的逻辑
- 修复应用层camera.h中的CAMERA_HEIGHT的拼写错误
- 支持v4l2-ctl工具修改参数
- 标准化imx6ull_querycap中v4l2_capability返回的driver/card/bus_info
- csi驱动新增imx6ull_vidioc_queryctrl/imx6ull_vidioc_g_ext_ctrls/imx6ull_vidioc_s_ext_ctrls三个函数，支持查看并调整基础画质参数
- ov5640驱动新增图像参数调控功能，实现亮度、对比度寄存器配置调节，注册亮度/对比度/色彩饱和度控制接口，适配v4l2控制框架

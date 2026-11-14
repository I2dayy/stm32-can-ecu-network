# stm32-can-ecu-network
基于STM32F103C8T6的多ECU车载CAN通信系统（动力/车身/仪表）

用 3 块 STM32F103C8T6 模拟动力 / 车身 / 仪表 ECU，在 500 kbps CAN 总线上
交换报文，实现报文过滤、超时检测与故障恢复。

## 硬件
- STM32F103C8T6 最小系统板 ×3
- SN65HVD230 CAN 收发器模块 ×3
- 120Ω 终端电阻 ×2、CAT5e 双绞线
- 工具：ST-LINK、USB 转串口模块、Keil MDK5 + 标准固件库

## 进度
- [x] L1 bxCAN 回环自检（500 kbps 位定时、全接收过滤器）
- [ ] L2 两节点对发（收发器 + 120Ω 终端）
- [ ] L3 三节点 + 报文矩阵 + 硬件验收滤波
- [ ] L4 超时检测 / E2E 校验 / BUS-OFF 恢复
- [ ] L5 DBC + 上位机可视化

## 复现（L1）
1. `stm32f10x_conf.h` 取消注释 `#include "stm32f10x_can.h"`
2. Keil 打开工程编译烧录，串口波特率与 `Serial_Init()` 中的
   `USART_BaudRate` 保持一致
3. 串口应打印：`ID=0x100 DLC=8 Data=10 11 12 13 14 15 16 17`

## 说明
部分驱动代码来自江协科技《STM32 入门教程》；
CAN 控制器配置、帧收发逻辑及后续应用层为本项目编写。

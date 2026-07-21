# 项目记忆索引

## 项目框架
- [LBS-NEXT-AI 项目概述](./memory.jsonl) — 芯片、工具链、硬件、APP/Bootloader 布局
  - 构建方式 → [Build & Deploy](./memory.jsonl) — 编译命令、部署方式
  - 架构设计 → [架构与运行循环](./memory.jsonl) — 事件驱动调度、PikaPython 运行时
    - 使用协议 → [通信与协议](./memory.jsonl) — USB/蓝牙双通道、帧协议、Ymodem
    - 运行于 → [Flash 内存映射](./memory.jsonl) — 各分区地址
  - 目录结构 → [代码目录结构](./memory.jsonl) — 各目录职责
  - 编码规范 → [编码约定](./memory.jsonl) — 命名、注释、编码

## 历史记录
- 2026-07-16: 修复两个Bug (脚本sleep时监控停止 + 0xB6启动死机)
- 2026-07-15: PikaPython 脚本 Flash 持久化 — 完整实现
- 蓝牙/监控路径速查 — 已记录
- 搁置 bug: 流水灯少一个灯 — 软件路径排查无根因
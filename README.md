# CH340 WiFi Bridge

ESP32-S3 UART-TCP 双向桥接设备，通过 WiFi AP 模式将串口数据与 TCP 客户端互相转发。

## 功能

- WiFi AP 模式（SSID: `ESP32S3`，密码: `ESP32S3-123`）
- TCP 服务器监听端口 `8080`
- UART ↔ TCP 双向数据转发
- TCP_NODELAY 禁用 Nagle 算法，降低传输延迟

## 更新日志

| 日期 | 提交哈希 | 作者 | 修改内容 |
|------|---------|------|---------|
| 2026-04-18 | c3ab9ad | shimu-huang666 | 添加wifi连接 |

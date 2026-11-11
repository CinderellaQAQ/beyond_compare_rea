# BC3 MultiDetails v0.2.3

修复在部分电脑上首次显示底部多行 Text Details 时弹出
`An error occurred in the application` 的问题。

- 代理映射改为两阶段重绑：先让全部底部原生编辑器脱离旧代理，再一次性发布
  新映射与活动状态，最后统一重绑。
- 在 BC3 原生 `SetDataSource` 同步触发当前行和样式回调之前，代理已进入正确的
  活动状态，避免把局部行号误当成原始行号。
- 样式回调根据编辑器实际持有的数据源决定是否映射，不再依赖延后更新的状态。
- 取消最后一条选择时先用旧映射恢复原始行，再清空代理，避免丢失当前位置。
- 支持从正式 v0.2.0、v0.2.1 或 v0.2.2 安全原地升级。

已通过普通短文本首次绑定、三个不连续的 10,001 字符差异行、两个各 10,000 行
文件、共享横向滚动、底部点击回跳、10,000 字符完整复制、独立冷启动配置及
150% DPI 回归测试。

目标版本：Windows Beyond Compare 3.3.5 build 15075。

- 安装包 SHA-256：`b58baf7ec36ebad51f1fb831436a28d7d1e2f603cf4a5ba5c9abd58dd1726231`
- `version.dll` SHA-256：`61a032bbfc880edc80174dee3f7c09ab5d68feec1579ee9692e648dbc3d3188d`

安装包未进行商业代码签名，Windows 可能显示 SmartScreen 提示。安装包不包含
Beyond Compare 原始程序；使用者需自行安装并持有合法许可。

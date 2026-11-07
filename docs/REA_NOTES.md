# REA / Ghidra 复核记录

REA 6.8.0 使用 Ghidra 12.1.4 对精确目标执行了默认全程序分析。

- 目标 SHA-256：`1d822b3faabb8f65fc30076d32a95757a2c369ccb64ae54572e9f562280ae845`
- 格式/架构：PE32 / x86
- Ghidra 语言模型：`x86:LE:32:default:borlanddelphi`
- 映像基址：`0x400000`
- `.text`：`0x401000`–`0xaaae00`
- REA Evidence：`ev_e75754321ebc85fefdd7e5e9bb0653eb94b66d18cf96bfc8130eb27c79144cab`
- 默认分析结果：22,507 个过程、17,854 个已定义字符串

Delphi published-method 表与指令边界的交叉分析得到本扩展使用的入口：

- `TTextViewFrame.DetailsPanelResize`：VA `0xa661a8` / RVA `0x6661a8`
- `TTextViewFrame.EditorClick`：VA `0xa6f44c` / RVA `0x66f44c`
- detail editor `GetLeftX`：VA `0x65f058`
- detail editor `SetLeftX`：VA `0x65f068`
- core maximum-left-column：VA `0x65a2cc`

Ghidra 默认 Delphi 自动分析未把 `0xa6f44c` 单独建立为函数入口，因此函数级
请求不应被误读为“地址不是代码”。该地址位于已认证 `.text` 映射内；入口机器
码由独立反汇编及运行时挂钩前字节校验共同确认。扩展只有在入口字节完全一致
时才写入跳转。

本地复核快照保存在 `analysis/bc3-ghidra.snapshot.json`，其中包含分析机路径及
专有程序清单，因此不纳入公开仓库；可复现的 Delphi 方法表扫描器位于
`analysis/delphi_method_table.py`。

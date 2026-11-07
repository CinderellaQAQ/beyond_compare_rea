# BC3 MultiDetails 0.1.0 测试记录

测试目标为 Windows BC3 3.3.5 build 15075，`BCompare.exe` SHA-256：
`1d822b3faabb8f65fc30076d32a95757a2c369ccb64ae54572e9f562280ae845`。

## 已验证项目

- 干净构建无编译警告。
- 安装器成功安装内嵌 DLL，安装前后 `BCompare.exe` 哈希不变。
- 安装后的代理成功加载并记录 `BC3 MultiDetails active`。
- 单击替换、`Shift` 连选、`Ctrl` 不连续切换、`Ctrl+Shift` 区间追加。
- 六行选择时面板封顶并启用底部区域的纵向滚动。
- 字符级重要差异继续使用 BC3 原生红色高亮。
- 超长行共享横向翻页：底部偏移从 0 变为 1013；所有 L/R 条带同步，
  上方主编辑器的横向位置不变。
- 多标签页各自保存选择状态。
- 重复运行同一安装包可修复安装。
- 目标目录中有未知 `version.dll` 时返回冲突并保持原文件哈希不变。
- 错误或缺失的目标版本被拒绝。
- 卸载后扩展 DLL 被删除，`BCompare.exe` 哈希仍与安装前一致。

## 证据图

- `screenshots/bc3-windows-multidetails-text.png`
- `screenshots/bc3-installer-ctrl-shift-regression.png`
- `screenshots/bc3-shared-horizontal-after-fixed.png`
- `screenshots/bc3-installer-final.png`

GUI 功能测试在隔离 Wine 前缀中执行；应用二进制及界面行为均为目标 Win32
构建。发布安装包为原生 PE32 GUI 可执行文件。

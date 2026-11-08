# BC3 MultiDetails 0.2.0 测试记录

测试目标为 Windows BC3 3.3.5 build 15075，`BCompare.exe` SHA-256：
`1d822b3faabb8f65fc30076d32a95757a2c369ccb64ae54572e9f562280ae845`。

## 已验证项目

- 干净构建无编译警告。
- 安装器成功安装内嵌 DLL，安装前后 `BCompare.exe` 哈希不变。
- 安装后的代理成功加载并记录 `BC3 MultiDetails active`。
- 单击替换、`Shift` 连选、`Ctrl` 不连续切换、`Ctrl+Shift` 区间追加。
- 六行选择时面板封顶并启用底部区域的纵向滚动。
- 字符级重要差异继续使用 BC3 原生红色高亮。
- 7,060/7,062 字符超长行在底部偏移 2,046 与 12,276 像素处内容连续，
  左右 L/R 条带同步，且上方主编辑器横向位置不变。
- `Ctrl` 不连续选择第 2、5 行后，底部共用滚动条从 0 移到 1,013，
  两组 L/R 条带保持同一横向偏移。
- 底部任意 L/R 条带支持单击光标、拖动选区、`Shift` 扩选、键盘移动、
  `Ctrl+A` 与 `Ctrl+C`；整行复制短样本得到精确文本 `Version: 3.3.13`。
- 在底部第一组创建光标后，原生 Text Details 数据源从上方最后点击的 Status
  行切回 Version 行；上方正文自动定位到相同的比较行与字符列。
- 两个各 10,000 行、每个差异行 10,001 字符的压力样本中，同时选择
  R00002、R00005、R00008 三组 L/R 差异；共享横向滚动条从 0 移到 2,046，
  三组内容保持相同偏移且无错位。
- 在压力样本底部第一组偏移位置拖选列 313..327，上方自动定位到第 2 行
  第 328 列，复制结果为 `klmnop-R00002-`，未串到其他选中行。
- 多标签页各自保存选择状态。
- 重复运行同一安装包可修复安装。
- 0.1.1 DLL 与卸载器允许由 0.2.0 原地升级；其他未知文件仍拒绝覆盖。
- 目标目录中有未知 `version.dll` 时返回冲突并保持原文件哈希不变。
- 错误或缺失的目标版本被拒绝。
- 卸载后扩展 DLL 被删除，`BCompare.exe` 哈希仍与安装前一致。

## 证据图

- `screenshots/bc3-windows-multidetails-text.png`
- `screenshots/bc3-installer-ctrl-shift-regression.png`
- `screenshots/bc3-shared-horizontal-after-fixed.png`
- `screenshots/bc3-installer-final.png`
- `screenshots/bc3-v011-long-scroll-fixed.png`
- `screenshots/bc3-v011-multiselect-scroll.png`

本轮交互压力测试已通过截图与剪贴板内容交叉验证；发布物不包含测试环境中的
临时截图和 BC3 专有程序文件。

GUI 功能测试在隔离 Wine 前缀中执行；应用二进制及界面行为均为目标 Win32
构建。候选安装包为原生 PE32 GUI 可执行文件。

## 本地候选构建

- `dist/BC3MultiDetails-Setup-0.2.0.exe`：
  `eb6cf8676ff79596ac797ed7587fdedd68c8645e75f7061c2dbbdb38aa8c8e7b`
- `dist/version.dll`：
  `2f7488932c86a26d91078b1c26791e9e7a183eb6adaff8fb222fa84afd18f40c`

以上为本地候选产物，尚未上传 GitHub Release。

# BC3 MultiDetails 0.2.2 发布测试记录

测试目标为 Windows BC3 3.3.5 build 15075，`BCompare.exe` SHA-256：
`1d822b3faabb8f65fc30076d32a95757a2c369ccb64ae54572e9f562280ae845`。

## 已验证项目

- 干净构建无编译警告。
- 安装器成功安装内嵌 DLL，安装前后 `BCompare.exe` 哈希不变。
- 安装后的代理成功加载并记录 `BC3 MultiDetails active`。
- 单击替换、`Shift` 连选、`Ctrl` 不连续切换、`Ctrl+Shift` 区间追加。
- 已确定跨机器应用错误的直接根因：代理第二行的局部行号 `1` 虽已在数据源
  vtable 中映射到原始行 `8`，BC3 的整行样式回调仍把 `1` 交给
  `TextContent`；目标类别没有任何元素，最终在 `0090C26B` 触发 Delphi
  `0EEDFADE` 断言。修复后，单元格样式与整行样式两条入口均收到原始行 `8`，
  同一必现步骤无错误对话框。
- 已确定上方灰色边框穿过文字的根因：直接写入编辑器核心的当前行字段只改变
  逻辑行，没有更新 BC3 的视觉行和光标几何缓存。0.2.2 改用原生当前行设置器，
  并在 96 DPI 与 150% DPI 下确认边框位于字形上下边界。
- 0.2.2 在多行代理活动期间拦截原版单行 `CurrentChanged`，延迟重绑并恢复
  原生模型锁，防止原始坐标被再次写入较短的底部代理。
- 字符级重要差异继续由 BC3 原生 `TUiEditorWindow` 渲染，不使用位图缓存或
  自绘文本。
- 两个各 10,000 行的压力样本中，`Ctrl` 同时选择三个不连续比较项；底部
  原生多行数据源依次映射到原始项目 1、4、7。
- 点击底部第二、第三行后，上方正文分别定位到原始项目 4、7，无串行或错误
  对话框。
- 三条差异行每条包含 10,000 字；底部原生光标、`Shift+End` 选区和
  `Ctrl+C` 均可操作，剪贴板结果为 `CF_UNICODETEXT 10000`，无截断。
- 普通短行与两条 10,000 字符长差异混合 `Ctrl` 选择通过；三条 10,000 字符
  差异 1、4、7 的不连续选择也通过。
- 含中文文本、插入行及删除行的第二组比较在同一测试前缀中完成三行不连续
  选择，无应用错误。
- 底部原生共享横向滚动位置达到 8,379，明显越过旧的 4,096 字符截断点；
  左右两侧三条原生行保持相同横向位置且无错位。
- 原生多行模式已关闭 BC3 的单行约束并开启文本选择；离开该模式后恢复原始
  编辑器选项。
- 多标签页各自保存选择状态。
- 重复运行同一安装包可修复安装。
- 0.2.2 安装器允许精确匹配的 0.2.0/0.2.1 DLL 与卸载器原地升级；实测
  0.2.1→0.2.2 后 `BCompare.exe` 哈希不变，其他未知文件仍拒绝覆盖。
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

## 正式发布构建

- `dist/BC3MultiDetails-Setup-0.2.2.exe`：
  `395adf29bd5ad5e16496d38f26d186ad2d939f7f60c0de4d8193c9f10791fb67`
- `dist/version.dll`：
  `0e8d1b539ced77507b6de997b4ef5e4d6c184fdc73ae560c822996906715209e`

以上哈希用于校验 GitHub Release 中的正式发布物。

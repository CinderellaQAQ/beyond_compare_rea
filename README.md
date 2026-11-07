# BC3 MultiDetails

为 Windows 版 Beyond Compare 3.3.5（build 15075）增加可自定义多行的
Text Details 面板。

[下载 BC3MultiDetails-Setup-0.1.1.exe](https://github.com/CinderellaQAQ/beyond_compare_rea/releases/latest/download/BC3MultiDetails-Setup-0.1.1.exe)

## 效果

![多行 Text Details](screenshots/bc3-windows-multidetails-text.png)

选择不连续行后，每个比较对按 `L / R` 两条显示；底部唯一的横向滚动条会让
全部条带同步移动，上方正文区的横向位置保持不变。

![共享横向滚动条](screenshots/bc3-shared-horizontal-after-fixed.png)

## 使用方式

在 Text Compare 上方正文区中选择比较行：

- 单击：只保留当前行。
- `Shift` + 单击：从锚点到当前行连续选择。
- `Ctrl` + 单击：追加或取消一条不连续行。
- `Ctrl` + `Shift` + 单击：把一个连续区间追加到已有选择。

底部按比较顺序为每个选择显示一组 `L / R` 原生字符差异。底部唯一的横向
滚动条控制全部 L/R 行；上方左右正文区的横向位置不会被改变。选择较多时，
底部面板最高扩展到 360 像素，并提供自己的纵向滚动条。

## 目标版本下载

本扩展只支持 Windows 版 Beyond Compare 3.3.5（build 15075）。Scooter
Software 官网目前只公开提供 BC3 最终版 3.3.13，以下 3.3.5 安装包来自第三方
历史镜像，并非官方当前下载地址：

- [下载 BCompare-3.3.5.15075.exe](https://www3.btsoftware.com/pub/download/nt/BCompare-3.3.5.15075.exe)
- 安装包大小：`5,869,768` 字节
- 安装包 SHA-256：`19bf30d2dd518aae1dd3425d77e88c1700460fad9fe6b7e5f5821425798cbb6e`
- 安装后的 `BCompare.exe` SHA-256：`1d822b3faabb8f65fc30076d32a95757a2c369ccb64ae54572e9f562280ae845`

该镜像文件已与本项目逆向和测试所用安装包逐字节比对一致。由于来源为第三方
且软件版本较旧，请在下载后核对 SHA-256，并优先在隔离测试环境中使用。软件
许可仍由 Scooter Software 提供，用户应自行确保拥有合法使用资格。

## 安装

1. 关闭所有 `BCompare.exe` 进程。
2. 运行 `BC3MultiDetails-Setup-0.1.1.exe`。
3. 选择含有 `BCompare.exe` 的 Beyond Compare 3 安装目录，然后点击 **Install**。
4. 重新启动 Beyond Compare，打开 Text Compare 会话。

若已经安装本项目发布的 0.1.0，可直接运行 0.1.1 原地升级，无需先卸载。
升级器只会替换 SHA-256 精确匹配 0.1.0 发布物的 DLL 与卸载器；未知
`version.dll` 仍会被安全保留并中止安装。

安装器请求管理员权限是因为默认安装位置通常位于 Program Files。它不包含
Beyond Compare 的任何程序文件，也不会修改 `BCompare.exe`。

安装器只接受以下精确目标：

- 文件大小：`9,571,304` 字节
- SHA-256：`1d822b3faabb8f65fc30076d32a95757a2c369ccb64ae54572e9f562280ae845`

若目标目录已有其他 `version.dll`，安装会安全中止，不覆盖该文件。

## 0.1.1 修复

- 修复长差异行横向滚动一段距离后出现重复、错位或空白内容的问题。
- 缓存时显式同步左右原生 Text Details 编辑器，并按实际渲染位移测量列宽。
- 分段拼接只复制当前位图真实可用的像素，避免越界读取。
- 支持从本项目正式发布的 0.1.0 安全原地升级。

下图为 7,000 余字符差异行滚动 12 个底部视口后的结果；左右内容仍连续且
保持同一横向位置。

![0.1.1 超长行滚动修复](screenshots/bc3-v011-long-scroll-fixed.png)

安装包尚未进行商业代码签名；Windows 可能显示 SmartScreen 提示。请先核对
本文末尾的 SHA-256，再决定是否运行。

## 卸载

在 Windows“应用和功能/程序和功能”中卸载 **BC3 MultiDetails**。卸载器只在
`version.dll` 与本扩展的内嵌副本逐字节一致时才删除它，并保留原始
`BCompare.exe`。卸载器自身可能在下次重启时完成清理。

## 实现与安全边界

扩展使用应用目录中的 `version.dll` 代理转发 BC3 所需的六个 Windows 版本
API，并仅在目标文件大小及关键机器码均匹配时启用内联挂钩。多行面板复用
BC3 原生 Text Details 编辑器生成字符级高亮，再把选中行缓存为可同步滚动的
L/R 条带。

运行日志位于 `%TEMP%\BC3MultiDetails.log`。若验证失败，扩展保持停用并在日志
中记录原因。

仓库及安装包不分发 Scooter Software 的专有二进制文件。`artifacts/`、
`extracted/` 与 `runtime/` 仅是本地研究/测试输入，不属于发布物。

## 构建

需要 GNU Make 和 32 位 MinGW-w64 工具链（默认命令前缀
`i686-w64-mingw32`）。若仓库工作区存在 `runtime/mingw/root`，Makefile 会自动
使用该本地工具链：

```text
make all
```

也可以显式指定工具链，例如：

```text
make CC=/path/to/i686-w64-mingw32-gcc \
  WINDRES=/path/to/i686-w64-mingw32-windres all
```

输出：

- `dist/version.dll`
- `dist/BC3MultiDetails-Setup-0.1.1.exe`

本次最终构建 SHA-256：

- 安装包：`b14940e8015d0955eeacbb661d8fdcec699265290e7f151adae62c44480dfdde`
- 扩展 DLL：`207a64a00940f24e8560c97429d768cb0251d145ddec4a6597c98e664b9c91de`

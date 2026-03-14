# Portable Release

目标机完全不用安装任何环境时，交付物应该是一个已经打好的“绿色版目录”，而不是源码。

## 目录内容

绿色版目录至少包含：

- `windows_cache_scanner_gui.exe`
- `windows_cache_rules.json`
- Qt 运行库 DLL
- `platforms/qwindows.dll`

如果这些文件齐全，目标机通常不需要安装 Qt、CMake、Visual Studio 或其它开发环境。

## 已提供的自动构建方式

仓库里已经补了 GitHub Actions 工作流：

- `.github/workflows/build-windows-portable.yml`

它会在 Windows runner 上自动：

1. 安装 Qt 6
2. 配置并编译 GUI 程序
3. 用 `windeployqt` 收集运行时依赖
4. 输出一个可直接拷走的绿色版目录

## 使用方法

把这个项目推到 GitHub 仓库后，在 Actions 里手动运行：

- `Build Windows Portable`

跑完后，在构建产物里下载：

- `windows-cache-scanner-portable`

解压后，把整个目录复制到任意 Windows 电脑上，直接运行：

- `windows_cache_scanner_gui.exe`

## 说明

当前本地环境是 macOS，且没有 Windows Qt 编译链，所以这里不能直接产出真正的 `.exe`。
但这个工作流可以在 GitHub 的 Windows 构建机上生成免环境运行包。

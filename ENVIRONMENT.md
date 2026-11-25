# 环境搭建（新机器）

## 1. 必备软件

| 软件 | 要求 |
|---|---|
| Unreal Engine | **5.7**（Installed Build，非源码版）。当前路径硬编码为 `C:\Workspace\UE5\Engine\UE_5.7` |
| Visual Studio 2022 | 组件见 `Forever_UE/.vsconfig`（NativeDesktop + NativeGame + CoreEditor 工作负载、MSVC 14.38 与 14.44 工具集、Windows 11 SDK 22621、Clang）。VS 的“导入配置”可直接装 |
| Git + Git LFS | `git lfs install` 后再 clone；`.gitattributes` 里 `Content/3rdParty/*` 若干目录和 `Forever_Mod/Wxdj/Train/models/**/*.pt` 走 LFS |
| Python 3 | 测试汇总脚本、`Tools/VehicleRig`；Wxdj 训练另需其 `Train` 目录的依赖 |
| Node.js | 仅 `Tools/AvatarGen` 生成头像用 |
| Emscripten | 仅 `Tools/ModWeb` 用，装到 `Tools/ModWeb/.emsdk`（被忽略） |

## 2. 硬编码的绝对路径

仓库默认放在 `C:\Workspace\UE5\Projects\Forever`，引擎在 `C:\Workspace\UE5\Engine\UE_5.7`。放在别处要改：

- `Forever_UE/build.bat`（引擎、工程、MSBuild、Mod 根目录）
- `Forever_UE/Resource/Config/config.json` 和 `Forever_Mod/*/Resource/config_*.json`（`dll_paths`、资源路径等，形如 `C:/Workspace/UE5/Projects/Forever/...`）
- `Source/Test/run_tests.bat` 如引用了引擎路径也要改

## 3. 构建顺序

1. 生成 VS 工程：右键 `Forever_UE/Forever.uproject` → Generate Visual Studio project files（`Forever.sln` 由 UE 生成，被忽略）。
2. 运行 `Forever_UE/build.bat`，按顺序编译：
   1. `Source/Framework.sln`（Dependence / Core / Basic / Test，Release x64）。Basic 是运行期 DLL，由 `config.json` 的 `dll_paths` 加载，不链进 UE 模块。
   2. UE 模块 `ForeverEditor Win64 Development`（UBT；含 `ForeverTest` 测试模块）。
   3. 外部 mod：`Forever_Mod/{Empty,Test,Yuanshen,Wxdj}/Cpp/*.sln`（Release x64）。
3. 生成头像：`cd Forever_UE/Tools/AvatarGen && npm install && node generate.mjs`（输出 `Resource/Avatars`，被忽略，没有则头像缺失）。
4. 用编辑器打开 `Forever.uproject`，进 `/Game/Levels/World` 或从菜单关卡开始。

## 4. 测试

```
Source\Test\run_tests.bat [quick|full] [core|ue|all]
```

`core` 管线先跑 ModTest(给 mod 作者用的 mod 自测程序，这里对 config 里的全部 mod dll 跑一遍)，再跑 CoreTest。详见 `Forever_UE/Source/Test/README.md`。注意：

- 改了 Core 必须编整个 `Framework.sln`；只编 `Source/Test/Core/Test.vcxproj` 不会重编 `Core.lib`，会测到旧代码。
- 改了 `Source/Dependence` 头文件，四个外部 mod 都要重编，否则 lint 报 DLL 过期，且 vtable/结构体错位会读越界。
- 跳时间、perform、commute、ride、changes 套件需要带窗口渲染，`-nullrhi` 只能跑 boot/components/saveload。

## 5. 常见坑

- mod DLL 不得修改 Core 持有的 STL 容器，也不得按值跨边界传 `std::string`（>15 字符才暴露）。
- 引擎是 Installed Build，mod 不能带 UE C++；机场飞机等原生功能在主程序里。
- 被 git 忽略、需要本机生成的东西：`Binaries/ Intermediate/ Saved/ DerivedDataCache/`、`x64/`、各 `.dll/.lib/.exe`、`Resource/Avatars/`、`Forever.sln`。

# CoreTest

## 职责
无画面的 Core 自动化测试程序(`x64/Release/CoreTest.exe`)，用法与套件说明见 `Source/Test/README.md`。

## 文件分工
- `main.cpp`：参数解析、套件表、子进程分发、汇总与退出码。
- `test_framework.*`：`TestRun`(Check/Metric/Note，写 json/txt 报告，子进程导出/父进程导入)、FNV 哈希、`ReadProcessMemory`/`RecordMemory`(进程私有内存 MB 与相对上次记录的增量，各套件在关键点调用)。
- `test_options.h`：命令行选项与 quick/full 默认值(地图 256/1024、3/24 个月、种子 {11,12}/{11..14})。
- `process_util.*`：自身路径、起子进程(重定向输出+超时)、崩溃过滤器(打印模块+RVA，配合 .map 解析)。
- `world_build.*`：`LoadTestConfig`(注入顶层 seed 并给每个 mod 条目注入派生 `--seed`)、`BuildWorld`(与 UE 同顺序，逐阶段计时/漏包计数/签名)、`ComputeFingerprint`。
- `child_world.*`：世界/套件子进程的入口与父进程驱动、指纹比较。
- `suite_mods.cpp`：只查 mod 接进 Core 之后的事(id 全局唯一、Registry 认得、计数器复位、Config 的 mod 参数)；对 mod 本身的测试在 `Source/Test/Mod`(ModTest，给 mod 作者用，不依赖 Core)。
- `suite_*.cpp`：各套件；在子进程里跑的套件主体在 `FindSuiteBody`(suite_random.cpp)里登记。

## 关键设计
- 多种子在子进程里跑：Registry/mod 计数器/DLL 静态状态不会复位，同进程第二个世界无法比较；崩溃也只标记该种子失败。
- 每日推进用 Core `WorldStep`，与 UE 的 StepWorld 是同一份实现。
- 世界不析构(Core 对象互持裸指针，进程随后退出)。

## 注意
- 含 C++ 转义的文件不要用 shell heredoc 写(转义会被破坏)，用 Write/Edit。
- `suite_green.cpp`的套件父进程自己派生一份config(只换路网/地形为MapEditor的demo)，再用`RunSuiteInChildren`起子进程，所以子进程读的是派生config。
- 新增套件：写 `XxxSuiteBody` + `RunXxxSuite`，在 suites.h、`FindSuiteBody`、main.cpp 套件表、vcxproj/filters 里各登记一处。

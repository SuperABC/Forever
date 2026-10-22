# ForeverMemoryReport — 内存泄漏怎么查

## 内置的快照
`ForeverMemoryReport::Log(world, tag)` 往 `Saved/Logs/Forever.log` 打一行：

`ForeverMemory[menu]: used physical 3456.7 MB (peak …), used virtual …, UObjects 123456, actors 789 (citizens 0, buildings 0, vehicles 0, transit 0, assets 0).`

- 进入主菜单关卡时（`AForeverMenuController::BeginPlay`）先强制 GC 再自动打一行，tag=`menu`。
- 控制台命令 `Forever.MemoryReport` 随时手动打一行，tag=`console`。

## 查泄漏的步骤（反复进出几轮）
1. 开游戏进菜单（第 0 行基线）→ 点开始游戏 → 玩一会儿（走几步、让市民/建筑流式生成一下）→ Tab → **退出游戏** → 回到菜单（又一行）。重复 4~5 轮。
2. 看每次回到菜单那一行：
   - 市民/建筑/载具/资产 Actor 数应当全是 0（世界已经销毁）；不是 0 说明有 Actor 被静态容器或 UPROPERTY 引用拿住没销毁。
   - `UObjects` 和 `used physical` 第一轮会涨（一次性缓存：着色器、资产、mod 注册、Registry 单例），**从第二轮起应当基本持平**；每轮稳定地涨几百 MB/几万个 UObject 就是泄漏。
3. 用同一个存档或同一份 config 对比，别每轮换配置/换随机种子的话数量波动会大。

## 定位是哪一类在泄漏
- UObject 级：游戏里按 `` ` `` 开控制台：`obj list class=StaticMeshComponent`（换成别的类）看数量；`obj list` 看总数分布；`obj gc` 手动 GC。回菜单后某类数量不降就是它。
- 内存分布：`memreport -full`（输出到 `Saved/Profiling/MemReports/`），回菜单前后各一次对比。`stat memory` / `stat memoryplatform` 看实时。
- 分配来源（最准）：用 `-llm` 启动 + `stat llm`，或 Unreal Insights 的 Memory Insights（`-trace=memory`）抓一次完整的「开始→退出→再开始」，在 Insights 里看退出之后仍然存活的分配和调用栈。
- Core 里的 C++ 泄漏（`new` 没 `delete`）：Core 静态链接进 Forever 模块，用的是 UE 的 FMemory，所以反映在 `used physical` 里；`EndPlay` 里会同步 delete `map/populace/society/story/industry/traffic/player`，这几个对象各自的析构函数负责释放自己拥有的子对象。顶层析构我静态看过没发现漏项，更深层（每个类的成员是否都被释放）没有逐个审，用上面的数据说话。

## 已知不会被"退出游戏"销毁的东西（设计如此）
- `Registry` 单例（函数内 static）和已加载的 mod DLL：进程生命周期内常驻，不会卸载（工厂里存的函数指针指向 DLL 代码段）。但每次开局 `Registry::RefreshMods()` 会补加载 config 里新增的 DLL 并刷新启用表，所以回菜单后改勾选、新增 DLL 路径再开始游戏都会生效；从配置里移除的 DLL 保持加载但被禁用。
- `Config` 静态状态、Basic 等 mod 里的 `static int count` 命名计数器：不重置，只会让新建物品的名字继续往后编号，不会撞名。

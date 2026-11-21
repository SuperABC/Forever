# random(带种子的随机)

`GetRandom`/`GetRandomNormal`(utility.h)改为从"当前种子流"取数，让世界生成可以控制变量、多次对比。

## 结构
- `RandomStream`：xoshiro256**，POD、无堆分配(Core分配，各DLL里的GetRandom也直接操作它，所以不能有std容器)。
- `RandomHost{current, unscopedDraws}`：当前流的共享槽。**每个DLL静态链接各自一份Dependence**，看不到Core的全局，所以Core加载DLL时通过DLL导出的`BindRandomHost(RandomHost*)`把自己的host交给它(每个mod DLL入口cpp写一行`FOREVER_MOD_BIND_RANDOM`；没导出的DLL退回不可控随机，不崩)。
- `RandomScope`：局部对象，构造时把当前流设成某条流，析构自动还原(栈式，可嵌套；传空指针=什么也不做)。
- `RandomRegistry`(宿主Core那份用)：mod流按mod id、Core域流按域名；`Reset/SetGlobalSeed/SetModSeed/GetModStream/GetDomainStream/Snapshot/Restore/ExtractSeed/BindInstance...`。

## 谁来设作用域
- 带mod id的：各Factory在Create时`BindInstance(instance, id)`，宿主包装类(Terrain/Roadnet/Zone/Building/Name/App/Puzzle)调mod虚函数前`RandomScope scope(RandomRegistry::GetInstanceStream(mod))`；static函数指针入口(`BuildingFactory::RandomAcreage/Assign/...`、`ZoneFactory::Assign`)在Factory里按id取流。mod代码里的`GetRandom(n)`一个字不用改。
- Core域级入口：函数开头`CORE_RANDOM_SCOPE("core.map")`(map/populace/society/polit/traffic/weather/story；政治域选举/党首投票/抽党员/平票裁决用`core.polit`)。UE层`ForeverTrafficFrameworkComponent`里选线路/车位也用`core.traffic`。
- 漏包的随机会退回不可控随机并计入`UnscopedDraws()`；开局最后一个生成阶段、EconSim结尾都会打出这个数(应为0)。新增mod虚函数里要用GetRandom时，记得宿主调用点包作用域。

## 种子
- `<concept>_mods`条目`--seed N` → 该mod流；顶层`"seed"` → Core各域(`hash(seed,域名)`)。不给=`random_device`，实际种子写日志`Random: <key> seed=N`。
- `--seed`由Core在mod构造前从参数串里剥掉(`ExtractSeed`，用`ModArgs::Remove`，其余参数原样留给mod，见`mod_args.md`)，mod自己的参数里读不到它。
- 存档：`world.random`段存每条流的seed+state，读档最后覆盖，见`Core/common/save.md`。
- `puzzle_common.h`的`PuzzleRng`有自己的`seed`参数(显式传才用它)，缺省取种子时走GetRandom，因此自动受puzzle mod流控制。Wxdj战斗里的`mt19937`是战斗内部随机，不受此约束。

## 已知限制
同种子也不保证整局逐字节一致：遍历顺序依赖指针地址的容器(`map/set`的键里带`Room*`/`Citizen*`等)会让结果随内存布局变化。已修的：`Map::FlushPendingBuildingRoadAccess`按Road首次出现顺序遍历(此前遍历`unordered_map<Road*,…>`，建筑接路节点的id和导航邻接表顺序每次不同；EconSim的`[NavHash]`行可对比)；`Organization::ReconcileJobs`改用(房间名, 类型)作键(此前招聘的空缺顺序随`Room*`地址变，同种子各类岗位已招人数不同)；修后EconSim同种子3次，stdout和3个月报表完全一致。EconSim没覆盖的域(交通、天气、UE层`TMap<指针,…>`等)没验证；新写代码时，遍历顺序会影响结果的容器不要用指针作键。

# 全项目 Review（2026-11-25）

初版基于提交 `7731158`；第二轮修复、第三轮（ModTest 拆分）、第四轮（引用检查）、第五轮（A7–A11）已随提交 `3500c67`、`1d86cff` 入库；第六轮到第十七轮也已入库（`021e707`、`3a402f1`、`de8e1f9`）；第十八轮（异常系统重构，含 C9）、第十九轮（C11）、第二十轮（B10、删 terrainshot）、第二十一轮（A5）、第二十二轮（A4）、第二十三轮（A3）、第二十四轮（B3）和第二十五轮（B7）还没提交。**已修的条目已经从清单里删掉**，下面 A–G 只剩还没处理的（`[ ]`）；修了什么见各轮小结。

**怎么读这份清单**

- 编号：A=Core 感知具体 mod，B=UE 感知具体 mod，C=bug，D=设计问题，E=违反 `CONVENTIONS.md`，F=文档/注释遗留，G=现有测试失败。编号和初版保持一致，所以有空缺：A3–A11、B1、B2、B3、B4、B5、B6、B7、B8、B10、C1、C2、C3、C4、C5、C6、C7、C8、C9、C10、C11、D1、D3、D4、D5、D6、D7、D8、D9、D10、F2、F3、G1、G3 和整个 E 节（E1–E4）已修并删掉（D9 里 `VehicleMod::drivable`/`capacity` 你决定留着），B9 你决定保持现状、不再关注，D2 你认为不算问题，F1 你决定不管，都删掉了；D4 没有按我最初建议的分发表改，改成了"种类标签 + `switch`"，见第十轮，文中别处提到这些编号时指的就是它们。
- 可信度：
  - **已确认**：读过对应代码，行号核对过。
  - **潜伏**：代码确实有问题，但当前没有调用路径会触发。
  - **推测**：有依据但没有实际复现/回退验证。
- 行号对应第二十五轮结束时的工作区。

**覆盖范围**

全项目约 12.5 万行代码、384 份 md。

- mod 感知（A、B）：对 Core/Dependence/Forever 三层的字符串字面量和标识符做了全量扫描，再逐处读上下文核对，这部分比较全。
- 找 bug（C）：重点精读 + 模式扫描，不是逐行通读。精读过的：`Core/common` 全部、`traffic.cpp`/`vehicle.cpp`、`room.cpp`、`economy.cpp`、`market.cpp`、`election.cpp`、`script.cpp` 的非词汇表部分、`map.cpp` 的前 1200 行和寻路部分、`ForeverFrameworkActor.cpp` 的前 1200 行、`ForeverModSubsystem.cpp`。Basic 各 mod、大部分 UI、存档编解码、`building.cpp`、`route.cpp` 主体只做了扫描，没有精读。

---

## 第二十五轮：B7 按载具/武器命名的类——只改了三处，类名不动

B 节（UE 感知具体 mod）到这里清空了。

**类名为什么不动**

- `APlaneElement`、`ABoatElement`、两个飞机轮子类、`AForeverRocketProjectile` 都保留原名。
- 它们和 `ABusController` 是同一种情况：是某种载具或武器自己的实现，由 mod 给的蓝图路径间接选中，主程序里没有任何地方按名字或 mod id 去挑它们。你在 B3 里已经认可了这种做法。
- 我初版建议的"按能力改名"不做了：改类名要给三个蓝图加类重定向，换来的只是名字。

**改了的三处**

- **`BoardTruck` 改名 `BoardTaskVehicle`**：它做的是"员工走到组织的车、上车出发"，什么种类的车都走这里，和卡车无关。一条日志里的 truck 也改成了 vehicle。纯改名。
- **轮子动画类不再认识飞机类**：`UVehicleWheelAnimInstance` 是所有载具共用的，原来里面直接转成 `APlaneElement` 去读螺旋桨。现在载具基类 `AVehicleElement` 多了一个虚函数"有没有轮子之外一直转的骨骼"，默认没有；飞机覆盖它，把螺旋桨的骨骼名、轴、角度交出来。动画类只认基类。以后别的载具有类似的旋转部件，也是覆盖这个函数，不用改动画类。
- **一处注释**：`ForeverExplosionEffect.h` 里拿 Test 插件的资产路径举例，换成了中性的示例路径。

**没动的**

- 键位表里三个只给飞机用的动作（加减动力、自由环视）保留。只要飞机类在主程序里，这就是必然的。

**验证**

- `build.bat` 全量通过（含 UE 模块和四个外部 mod）。
- 这轮只改了 UE 模块，Dependence、Core、Basic 没动，所以没有重跑 ModTest 和 CoreTest。
- 没有进 UE 编辑器验证：螺旋桨还转不转、转的方向对不对，要在 PIE 里看一眼；UE 管线仍然没跑过。

---

## 第二十四轮：B3 线路车换不换物理由载具蓝图自己声明

UE 里不再有 `"bus"` 这个字符串，交通组件也不再引用 `ABusController`。

**判据换了**

- **载具蓝图多了一项"线路控制器类"**（`AVehicleElement::lineControllerClass`，蓝图里显示为 Line Controller Class，默认空）。每种载具选自己的控制器，公交选 `BusController`。
- **线路车换不换物理只看这一项**：蓝图是可驾驶载具、并且指定了线路控制器，才在玩家附近换成物理车并交给那个控制器开。不看线路、站点、载具是什么种类，也不看线路走不走路网。
- **`ABusController` 保留**，仍然是公交自己的控制器。它现在只通过蓝图被选中，C++ 里没有任何地方直接引用它。
- **"测试运行"选线路**：原来是"所有站点类型为 bus 的线路"，现在是"使用这种载具的线路"。车的蓝图没指定线路控制器时拒绝并打警告。

**接管机制改成了通用名字**

- 代码里约 120 处按公交起的名字都换了（文档里另有 20 多处跟着改），例如 `FBusState` → `FLineVehicleState`、`BusToPhysics` → `HandoffToPhysics`、`busPhysicsLodLevel` → `linePhysicsLodLevel`。讲通用机制的注释和文档里的"公交"也改成了"线路车"，只在举例时保留"公交"。
- **两个可调参数改了名**：`busPhysicsLodLevel`、`busPhysicsEnterScale` 改成 `linePhysicsLodLevel`、`linePhysicsEnterScale`，分类从 Traffic|Bus 改成 Traffic|Line。我在 Content 和 Config 里没搜到任何资产改过这两个值，所以没有加属性重定向；如果你在某个蓝图里改过而我没搜到，那个值会回到默认（4 和 0.9）。
- `ABusController` 和它的文件名不变，它本来就是公交专用的。

**需要你做的（不做公交就不切物理）**

- 在编辑器里打开 `BP_BusElement`，类默认值的 Line 分类下，把 Line Controller Class 选成 `BusController`。
- 只设在 `BP_BusElement` 上，不要设在 `BP_VehicleElement`（轿车）上：公交蓝图是从轿车蓝图派生的，设在轿车上轿车自己也会带上。
- 没设的时候，世界生成时日志里会有一条"载具蓝图 … 没有指定线路控制器"。飞机蓝图也会有这一条，那是正常的。

**这轮没解决的**

- **交接内容还是公路专用的**：时刻表交给控制器的是"一圈站点的路网节点 + 当前第几段"。站点没接上路网的线路给不出来，这种线路车即使指定了控制器也保持运动学，并打一条警告。要支持不走路网的物理线路车，得把"怎么从时刻表接手"挪到控制器自己身上，等第一个这样的载具真要做时再抽。
- **`BP_BusElement` 的父类没动**：它继承自轿车蓝图而不是 `AVehicleElement`，这要在编辑器里改。载具制作说明里补了"父类要直接选 `VehicleElement`"。

**验证**

- `build.bat` 全量通过（含 UE 模块和四个外部 mod），改名之后又完整编了一遍。
- 这轮只改了 UE 模块，Dependence、Core、Basic 没动，所以没有重跑 ModTest 和 CoreTest。
- 没有进 UE 编辑器验证：公交切物理、"测试运行"的实际表现都没看过；UE 管线仍然没跑过。

---

## 第二十三轮：A3 载具查不到种类时不再当成 "car"

- **去掉兜底**：`Vehicle` 的种类就是 mod 注册的那个字符串。mod 没声明种类就是空串，这样的车不匹配任何车位，也不会被按种类的逻辑挑中，各处表现一致。
- **注册时报出来**：`VehicleFactory::RegisterVehicle` 发现种类为空时打一条警告，指名是哪个 mod。
- **ModTest 里拦住**：载具的字段检查加了一条"声明了种类"，mod 作者自测时就会失败。
- 现有 7 个载具 mod 都声明了种类，行为没有任何变化；不改存档格式，不改任何 mod 的接口。

**还剩的写死种类名**

- Core 里现在只剩 `Traffic::AssignVehicles` 里的 `"car"` 和 `"truck"` 两个种类名，那是 A2。

**测试**

- fault 套件加了一组：用测试自己的工厂登记两个假的载具 mod，一个不声明种类、一个声明成别的名字。检查前者注册时报一次警告、车的种类是空串，后者种类原样保留。

**验证**

- `build.bat` 全量通过（含 UE 模块和四个外部 mod）。
- ModTest 跑两遍，都是 0 失败：按 `config.json` 跑 1373 项通过；扫两个目录下全部 mod 1419 项通过。
- CoreTest quick：7489 通过、8 失败（都是 polit，即 G2）。报告在 `Forever_UE/Saved/TestReports/review_20261126_a3/core`。
- 没有进 UE 编辑器验证；UE 管线仍然没跑过。

---

## 第二十二轮：A4 Core 不再按名字跳过占位 mod

Core、Dependence、UE 三层的代码里现在搜不到任何 `<concept>_empty` 的名字了。

**三处特判都删了，换成不认名字的规则**

- **站点**：直接删。占位站点对"想贴哪些路"的回答本来就是"不贴"，行为不变。
- **线路**：删掉认名字的那行，加了一条通用规则——建完之后既没有线、也没有轨道的线路直接丢掉，打一条日志。对占位线路行为不变；真正的线路 mod 如果什么都没排出来，以前会留下一个空的线路对象，现在也不留了。
- **手机应用**：应用 mod 加了一个成员 `showOnDesktop`（默认 true），由 mod 自己声明要不要上桌面。占位应用把它设成 false。Core 只看这个标记。

**需要你知道的**

- **应用 mod 的基类多了一个成员**，没有改任何现有函数。四个外部 mod 已随 `build.bat` 重编。
- **线路那条规则对真实线路也生效，而且测试里已经发生了**。绿化、路网预设、高处地块这三个套件用的是手工做的小地图，上面没有任何车站，公交、火车、飞机三个线路 mod 在那里一条线都排不出来。以前这些世界里会留着 3 个空的线路对象，现在不留了（6 个测试世界各丢掉 3 个，日志里各有一条说明）。正常生成的世界里没有线路被丢掉，econ 摘要和上一轮相同。我认为这是对的——空线路没有任何用处——但它是一个行为变化。
- 平时用的 `config.json` 没有启用这三个占位 mod，所以平时的游戏不受影响。受影响的是 `Forever_Mod` 下三份示例配置，它们启用了这三个占位 mod；改完之后表现和以前一样。
- `empty_mods.md` 里补了一条原则：占位 mod 不起作用要靠自己，不要指望 Core 按名字跳过。

**测试**

- fault 套件子进程的配置里启用了占位的应用、线路、站点三个 mod，检查：占位应用没有桌面图标、其它应用照常有；线路表里没有空线路；世界正常生成。

**验证**

- `build.bat` 全量通过（含 UE 模块和四个外部 mod）。
- ModTest 跑两遍，都是 0 失败：按 `config.json` 跑 1373 项通过；扫两个目录下全部 mod 1419 项通过。
- CoreTest quick：7485 通过、8 失败（都是 polit，即 G2）。报告在 `Forever_UE/Saved/TestReports/review_20261126_a4/core`。
- 没有进 UE 编辑器验证；UE 管线仍然没跑过。手机桌面在 UE 里的实际显示没有看过。

---

## 第二十一轮：A5 脚本 mod 可以为空；"拿不到 mod 就跳过"的地方都打警告

**A5：Core 不再依赖 `script_empty`**

- `Script` 背后的脚本 mod 现在可以没有。mod 名留空时不去工厂创建，指针为空，匹配出的动作原样执行。
- mod 名非空但那个 mod 没注册或没启用，仍然是数据错误。点了名的 mod 拿不到是配置问题，不会悄悄变成"没有 mod"。
- 三处兜底代码（`MakeScript`、电梯、园区/建筑/房间）和常量 `kDefaultScriptModId` 删掉了。
- 9 处默认值从 `"script_empty"` 改成空串（Dependence 6 处、Core 3 处）。
- Basic 里 46 处显式写的 `"script_empty"` 去掉了，依赖表里对它的 43 条依赖声明也去掉了。Yuanshen 的 1 处同样处理。
- ModTest 跟着改：脚本 mod 留空不再算作引用；"有名字的门必须填脚本 mod"这条检查删了。
- 这是只对脚本这个 concept 做的决定，写进了 `CONVENTIONS.md` 第 5 节和 `script.md`：脚本的内容来自脚本文件，mod 只是可选的 C++ 钩子；别的 concept 的 mod 仍然必须存在。

**"拿不到 mod 就跳过"的地方都打警告**

| 哪里 | 现在的警告 |
|---|---|
| 产品 | 说明原因：mod 拿不到，或者价格关系不成立 |
| 仓库、工坊 | mod 拿不到，哪间房因此没有仓库/产线 |
| 载具 | mod 拿不到，哪辆车没建出来 |
| 物品 | mod 拿不到，没有创建物品 |
| 武器 | mod 的创建函数没给出实例 |

- 产品、仓库、工坊、载具按 mod 去重：同一个 mod 缺失只报一次，不会每个房间、每辆车各刷一行。
- 读档时丢弃载具和物品原来就有警告，没动。
- 核对过：正常跑完 16 个测试套件，这些警告一条都不会出现。枚举武器和产品时工厂只给出已启用的 id，所以禁用一个 mod 不会触发警告。

**需要你知道的**

- **旧存档读不了**：存档里每条脚本记着它的 mod 类型，原来是 `script_empty`，现在是空串，读档比对不上。
- **`script_empty` 这个 mod 还在**，只是 Core 和 Basic 都不再需要它。`Forever_Mod` 里几份示例配置的 `main_story` 仍然用它当主线的脚本 mod，那是配置自己的选择。
- **主线脚本没有跟着改**：`main_story` 留空时仍然是"没有配置主线"，`Story::Init` 会跳过，配置界面也要求必须选一个。主线的系统变量池和全局变量池是存变量用的，不在这次范围内。
- **有名字但没有脚本文件的门**仍然会建一个空 Script（只存名字和标签）。它现在不需要任何 mod 了，所以不再有问题；要不要改成不建，我没有动。
- `Tools` 下单独编译的四个程序没有重编。

**测试**

- fault 套件的子进程改用一份去掉了 `script_empty` 的配置：世界照样生成出来，载具的脚本都存在且都没有 mod。
- 新增检查：没有 mod 的 `Script` 能创建、动作原样放行；点名一个不存在的脚本 mod 仍然是数据错误；5 种"拿不到就跳过"各报一次警告、去重有效。
- 模拟结果没有变：econ 的 4 份摘要和上一轮逐字节相同。

**验证**

- `build.bat` 全量通过（含 UE 模块和四个外部 mod）。
- ModTest 跑两遍，都是 0 失败：按 `config.json` 跑 1373 项通过；扫两个目录下全部 mod 1419 项通过。比上一轮各少 57、58 项，正好是去掉的依赖声明（Basic 43 条依赖、14 个只依赖 `script_empty` 的条目，全量扫描多 Yuanshen 的 1 条）。
- CoreTest quick：7481 通过、8 失败（都是 polit，即 G2）。报告在 `Forever_UE/Saved/TestReports/review_20261126_a5/core`。
- 没有进 UE 编辑器验证；UE 管线仍然没跑过。

---

## 第二十轮：B10 注释里不再点名具体 mod；删掉 terrainshot

**B10**

- UE 层代码注释里点名具体 mod 的地方重新扫了一遍，一共 11 处、9 个文件（初版只列了 4 处），全部改成了不指向具体 mod 的写法：
  - 拿具体 mod 当例子的 7 处（Mohar、`/Yuanshen/kong.kong`、`asset_pistol`、`weapon_pistol`、`weapon:weapon_rocket`）：换成一般性的描述或 `xxx` 占位。
  - 指着某个 mod 的脚本解释代码的 4 处（`vehicle_basic.script`、`vehicle_bus.script`、`job_factory_worker.script`、`job_shop_saler.script`）：去掉脚本文件名，直接说行为。
  - 默认地图尺寸旁边那条"宽×高/(512×512) 必须大于 1 才会生成山"：改成"地图太小时有的地形 mod 可能不生成，各 mod 的门槛见它们自己的文档"。
- 改完后在 UE 层的 `.h`/`.cpp` 里搜这些名字，已经没有了。
- **没有处理的**：UE 层的 `.md` 文档里还有大量提到具体 mod 的地方。B10 说的是代码注释，文档我没有统计也没有动。

**删掉 terrainshot**

- 按你的决定，把 UE 测试模块里的 terrainshot 截图套件删了：两个函数、一个阶段状态、四个成员、两个只有它用到的头文件引用，以及 `Test/README.md` 里的条目。
- 地形材质的画面现在只能在 PIE 里目测。

**验证**

- `build.bat` 全量通过。两处改动一处全是注释，一处是删测试代码，没有重跑 ModTest 和 CoreTest。
- UE 管线（包括不需要窗口的 boot、components、saveload）这几轮一次都没跑过。

---

## 第十九轮：C11 的七个小问题

七条全部按之前说的修法改了。C 节到这里清空。

**1. 房间里放同名物品**
- `Room::AddAsset` 改成返回 `bool`：已经有同名物品时拒绝并打警告，不再覆盖。
- 三个调用方跟着处理了放不进去的情况：玩家丢东西返回失败、读档时丢掉放不进去的那件、UE 层把物品放回原处失败时记一条错误。

**2. 配置里的 `seed`**
- 只接受 0 到 2^64-1 的整数。负数、超范围的数、不是数字的都打警告并当作没配置。
- 数字写法和字符串写法两条分支行为一致。

**3. 生成 NPC 时判空**
- `ApplySpawnNpc` 接入经济系统前加了对 `player` 的判断。

**4. 建筑重名**
- `CreateBuildingAt` 在布局之前就查重名。重名时这栋建筑还没往地图里登记任何东西，直接删掉即可。

**5. 销毁线路上的车**
- `Route` 加了 `RemoveVehicle`：从车辆列表、停靠状态、相位修正、还没取走的进站/离站事件里都拿掉。
- `Traffic::DetachVehicle` 里补了这一步。
- 线路的排班是按"第几辆/共几辆"算相位的，所以拿掉一辆后，同一条线路上剩下的车会重新均匀排开，位置会跳一下。

**6. 事件取字段的接口**
- `Event::GetLocalValue` 从"按值返回"改成"把值交给调用方给的回调"。值在事件这一侧算出来、这一侧销毁，调用方在回调里自己拷一份。
- 34 个实现、112 处 `return` 用脚本批量改的。返回类型从"一对值"变成了 `bool`，漏改任何一处都编译不过，所以这次没有 C8 那种"编译通过但行为变了"的风险。

**7. 按文件名找资源**
- 资源目录的先后顺序现在单独记着（原来存在无序表里，连写回 `config.json` 的顺序都不稳定）。几个目录里有同名文件时，配置里排在前面的目录优先。
- 找脚本、找资源文件、列 layout、写回配置都按这个顺序。扫描时发现同名脚本或同名 layout 会打警告，说明用了哪个。
- 按文件名找资源文件改成查索引，不再每次递归扫盘。索引在扫描目录时顺手建好。

**需要你知道的**

- **第 7 条有一个行为变化**：一局游戏进行当中新放进资源目录的文件，要等下一次开始新游戏或读档才找得到。脚本和 layout 原来就是这样。
- **第 7 条顺带改了 layout**：原来几个目录里有同名 layout 时用哪个也是不确定的，现在同样是排在前面的目录优先，后面的丢掉并打警告。
- **第 4 条没有加测试**。建筑名由 mod 生成、保证不重复，测试里造不出重名的建筑。这条只靠读代码确认。
- **第 3 条没有加测试**。它是防御性的判空，正常流程走不到。
- `Event` 的虚函数变了，四个外部 mod 已随 `build.bat` 重编；`Tools` 下单独编译的四个程序没有重编。

**测试**

- world 套件加了两条：房间拒绝同名物品；销毁线路上的车之后线路不再认识它、交通照常能走一步。
- fault 套件加了三组：`seed` 的 9 种写法、两个资源目录有同名文件时谁优先（含调换顺序、扫描后新放文件）、事件通过回调交出字段。
- 这一轮不应该改变模拟结果：econ 的 4 份摘要和上一轮逐字节相同，polit 的指标也没变。

**验证**

- `build.bat` 全量通过（含 UE 模块和四个外部 mod）。
- ModTest 跑两遍，都是 0 失败：按 `config.json` 跑 1430 项通过；扫两个目录下全部 mod 1477 项通过。
- CoreTest quick：7469 通过、8 失败（都是 polit，即 G2）。报告在 `Forever_UE/Saved/TestReports/review_20261126_c11/core`。
- 没有进 UE 编辑器验证。

---

## 第十八轮：异常系统重构（含 C9）

按之前给你看过的设计全部实现了：三类异常、三层接法、UE 层回菜单。规则写进了 `CONVENTIONS.md` 第 14 节，细节在 `Dependence/common/error.md` 和 `Forever/Framework/CoreGuard.md`。

**一、异常类：10 个换成 3 个**

- `DataException`（数据写错了）、`ModException`（mod 的问题）、`InternalException`（程序自己的 bug）。原来的 10 个类全部删掉。
- 140 处抛出点逐个归了类：数据错误 109 处、内部错误 25 处、mod 错误 6 处。大部分是数据错误，这符合实际——Core 抛异常基本都是因为读到了坏的脚本、layout 或配置。
- 不是这三类的异常（mod 抛的 `std::runtime_error` 之类）：抛出时在某个 mod 里就算 mod 错误，否则算内部错误。

**二、出错时知道"当时在做什么"**

- 新增 `ErrorScope`：进入一段有名字的工作时放一个，正常返回什么都不做；异常穿过它时把自己记进报告。
- Core 里调 mod 的 61 处改成了 `MOD_CALL(mod, mod->Xxx(...))`，23 个工厂里调 mod 函数指针的 35 处改成了 `MOD_ID_CALL`。所以任何从 mod 里出来的异常，报告里都有 mod 的 id，不用在每个调用点写 try/catch。
- 读脚本、读 layout、读 config、世界生成的每个阶段、每帧各个域的 Tick 都登记了。
- 一份报告长这样：类别、消息、抛出位置（文件:行）、当时在哪个阶段 > 哪个 mod > 哪个文件。

**三、三层接法**

1. **可以整体跳过的操作**（只接数据错误，打警告，跳过这一个）：一条脚本指令、一个里程碑的触发条件、一段对话、一次 `start_puzzle`。
2. **有失败返回值的加载**（接住变成错误文字）：存档各段、`Config::ReadConfig`、菜单里的存档/读档/开始游戏/读写配置。
3. **会话边界**（UE 层，新文件 `CoreGuard`）：接住后这一局结束，回菜单并在菜单上显示原因。世界生成失败以前是直接退出游戏，现在也走这条路。

**四、UE 层包了哪些入口**

- 26 个文件、91 处。用的是两行宏 `CORE_GUARD_BEGIN` / `CORE_GUARD_END`，不用改函数结构。
- `AForeverFrameworkActor` 的 `BeginPlay`、`Tick`、`EndPlay`。
- 所有调 Core 的文件里的 Tick 一族（24 处）。
- `UForeverStoryFrameworkComponent` 的全部函数（52 处）：它是脚本事件的总入口。
- `UPuzzleWidget`、`UPhoneWidget`：它们每帧直接跑 mod 的界面代码。
- 线程池里的行人寻路任务。
- 保护可以嵌套，只有最外层接异常。里层如果接了，外层会带着半更新的世界把剩下的代码跑完。

**五、顺手清掉的**

- C9 的三处全部解决：世界生成只接 `ExceptionBase`、`AddResourcePath` 扫描目录会抛、`ReadConfig` 读到类型不对的元素会崩。
- 空的 `catch {}` 从 8 处减到 1 处，剩下的那处在 `ErrorScope` 自己的析构函数里（异常展开途中不能再抛，有注释）。原来的 8 处里有 6 处是"用 `stoi`/`stod` 试着解析，抛了就算了"，换成了新加的 `TryParseInt`/`TryParseDouble`/`Time::TryParse`（不抛异常，语义和原来逐个输入一致，有测试）。
- `ExceptionBase::GetDetailedInfo()` 删掉了。它是一个按值返回字符串的虚函数，宿主对 mod 抛出的异常调它，就是 C8 那种"一侧分配、另一侧释放"。

**六、测试**

- CoreTest 新增 `fault` 套件，74 项，逐条验证上面的规则：三类异常的归类、上下文的顺序和过期清理、只接数据错误的那层、坏的 config/脚本/layout/表达式。
- `Forever_Mod/Test` 新增一个故障注入用的小游戏 mod `puzzle_fault`（默认不启用），能按参数在 mod dll 里抛三种异常。测试确认宿主都能接住、归类正确、指名到这个 mod，并且之后世界仍能销毁、mod dll 仍能全部卸掉。
- 这次重构不应该改变任何模拟结果：econ 的 4 份摘要和上一轮逐字节相同，polit 的指标也没变。

**需要你知道的**

- **UE 层的回菜单流程我没有实际触发过。** 接住异常 → 撤掉黑屏 → 切回菜单 → 菜单显示原因，这四步只做了编译验证。要验证它，需要在编辑器里用一个会出错的脚本或 mod 跑一次。最简单的办法：在 `config.json` 的 `puzzle_mods` 里启用 `puzzle_fault`，脚本里 `start_puzzle` 它并把参数设成 `std`。
- **输入回调、UI 按钮回调、碰撞回调没有逐个包。** 它们进脚本都经过 `UForeverStoryFrameworkComponent`（已全部包住），直接调的 Core 接口基本是不抛异常的取值函数。以后新写"引擎直接调用、里面又调脚本/表达式/layout/mod"的函数，要自己加那两行宏，这条写进了 `CONVENTIONS.md`。
- **CoreTest 验证不了宿主和 mod 分配器不同的问题。** CoreTest 是普通程序，和 mod 用的是同一个堆；只有在 UE 里两边的分配器才不同。所以"mod 抛的异常消息完整传到宿主"这条测试，在 UE 里是否同样安全，靠的是代码规则（只读不拷贝、不按值接、不保存异常对象），不是测试。
- **接不住的还是接不住**：空指针解引用、除零、栈溢出不是 C++ 异常。
- 接住异常后世界是半更新的，销毁它时（各域的析构函数）理论上仍可能出问题。我没有办法系统地验证每一种中途失败后的销毁。
- `puzzle_fault` 会出现在游戏的 mod 面板里（因为 `Forever_Mod/Test` 的 dll 在 `dll_paths` 里），默认是不勾选的。
- C9 原文第三条说"数组里出现非字符串元素时会抛"，不准确：数字会被转成字符串，只有对象和数组才会抛。
- `CONVENTIONS.md` 第 4 节原来说各 dll"各自静态链接一份 CRT"，和工程设置不符（12 个工程全是 `/MD`）。随后按你的要求改成了实际情况：UE 模块把全局 `new`/`delete` 换成了引擎的 `FMemory`，mod dll 走 C 运行时的堆，`Dependence.lib` 是静态库所以同一份源码在两边各有一份编译产物。这三点我都核对过（12 个工程文件、引擎源码 `ModuleBoilerplate.h` 和 `PerModuleInline.inl`），和 `general_geometry.md`、`change.md` 里早先记下的两次崩溃现场一致。几条规则本身没有改。
- `Tools` 下单独编译的 RoadnetViz、JunctionTest、ModHost、ModWeb 没有重编。

**我中途出过的两个错（都已修）**

- 批量加 `MOD_CALL` 的脚本有个缺陷，把 `station.cpp` 里一行代码和下一行注释吞掉了一段。对照原文件逐处核对后只有这一处，已恢复。
- `ReadConfig` 里登记文件的那个 `ErrorScope` 一开始放在了接异常的范围外面，结果报告里没有文件名。是 `fault` 套件查出来的，已挪进去，并把这条注意事项写进了 `error.md`。

**验证**

- `build.bat` 全量通过（含 UE 模块和四个外部 mod）。
- ModTest 跑两遍，都是 0 失败：按 `config.json` 跑 1430 项通过；扫两个目录下全部 mod 1477 项通过。（比上一轮各多 9 项，是新加的 `puzzle_fault`。）
- CoreTest quick：7448 通过、8 失败（都是 polit，即 G2）。报告在 `Forever_UE/Saved/TestReports/review_20261126_ex/core`。
- 没有进 UE 编辑器验证。

---

## 第十七轮：C4、C7、C8

**C4 游戏时钟不再随帧率变**

- `Player::Tick` 把每帧不足 1 毫秒的零头记在新成员 `pendingMilliseconds` 里，累加到下一帧。时钟仍然只走整毫秒。
- 零头不进存档（不到 1 毫秒）。

**C7 销毁车辆时把指向它的引用都解开**

- `Traffic` 加了私有函数 `DetachVehicle`：取消这辆车的任务行程和通勤行程、让出它占着的车位、清掉它预订的车位、摘掉它的资产。`DestroyVehicle` 和 `CreateVehicle` 的同名覆盖分支在 `delete` 之前都调它。
- **和我原来的建议不同的一点**：原建议是在 `~Vehicle` 里释放车位。我改成放在 `Traffic` 里，因为整个世界销毁时 `~Traffic` 会 `delete` 所有车，析构函数里再去碰房间和车位，就要求地图一定晚于交通销毁。UE 里现在确实是这个顺序，但这种隐含要求容易被以后的改动打破。

**C8 `Container` 不再按值跨 DLL 传 `ValueType`**

- `SetValue` 改成收 `const ValueType&`，和原建议一致。
- **`GetValue` 没有按原建议改成"按引用输出参数"，那个建议是错的**：宿主往 mod 传进来的变量里赋一个长字符串，缓冲区仍然是宿主分配、mod 释放，问题原样还在。
- 实际改法：`GetValue` 换成 `FindValue(name)`，返回容器自己持有的那个值的 `const ValueType*`，没有这个变量返回 `nullptr`。调用方只读；要留着就自己拷一份，拷贝发生在调用方那一侧。
- 13 处调用点跟着改了（`expression.cpp` 3 处、`economy.cpp` 2 处、Basic 的商店/餐馆/视频公司 5 处、UE 层 3 处）。

**改 C8 时我出过一个错，已修**

- 商店和餐馆各有一行 `offer.capacity = ToInt(value)` 我漏加了 `*`。指针会被隐式转成 `bool` 再构造出 `ValueType`，所以能编译过，结果恒为 1：每家店每种货只能卖 1 份。
- 编译通过，所有测试的通过数也没变。是对比 econ 套件的摘要才发现的：消费税从 18.7 万掉到 2.4 万。
- 修法：补上 `*`；另外在 `utility.h` 里把 `ToInt`/`ToDouble`/`ToBool`/`ToString` 的指针版本声明成 `= delete`，以后忘写 `*` 是编译错误。加了之后全量编译没有别处报错，说明没有第三处漏改。
- 修完后 econ 的 4 份摘要和改动前逐字节相同，polit 的指标也回到改动前的值。

**新增的测试（都在 world 套件）**

- 时钟零头：2400 帧、每帧 0.4167 毫秒，合计应走 1000 毫秒。改之前这种情况下时钟完全不走。
- 销毁车辆：先让一个市民开始开车通勤（车停在车位里，并预订了目的车位），再销毁这辆车，检查原车位空出来、预订清掉、通勤行程和资产都不再指向它。两个种子都走到了真实通勤行程这条分支。

**需要你知道的**

- `Container` 的虚函数表变了，四个外部 mod 已随 `build.bat` 重编；`Tools` 下单独编译的 RoadnetViz、JunctionTest、ModHost、ModWeb 没有重编。
- 顺带发现两个相关的小问题，没有改，记在 C11 里：销毁线路上的车时 `Route` 里还留着指针；`Event::GetLocalValue` 和 C8 是同一种写法（目前没有 mod 调用）。
- C4 的改动我只用测试验证了，没有在 UE 里看实际的时钟速度。

**验证**

- `build.bat` 全量通过（含四个外部 mod）。
- ModTest 跑两遍，都是 0 失败：按 `config.json` 跑 1421 项通过；扫两个目录下全部 mod 1468 项通过。
- CoreTest quick：7370 通过、8 失败（都是 polit，即 G2）。报告在 `Forever_UE/Saved/TestReports/review_20261126_c478b/core`。
- 时钟零头的测试是全量跑完之后加的，只重编了测试并单独重跑 world 套件：108 项全过。报告在同目录的 `world_only`。
- 没有进 UE 编辑器验证。

---

## 第十六轮：G1、G3 解决（行人图 + 不接公路的车位）

CoreTest quick 从 22 项失败降到 8 项，剩下的 8 项全是 G2（polit）。

**第一步：你把孤立房间接进了行人图**

- 你改了 6 个 layout（机场 2 个、港口 1 个、火车站 3 个），补了行人导航节点和连接。
- 没接进行人图的房间从 42 个降到 0，G1 里行人相关的 10 项全过。"反向路线不是正向的倒序"和三条路线缓存检查原来失败，就是因为孤立房间当起点时会算出一条凭空的路线。

**第二步：车位分成"接公路"和"不接公路"**

- 剩下 4 项（route 2、perform 2）的原因：测试从全地图所有车位里随机抽，其中 20 个车位的节点不在车行图里——两个港口各 8 个船位，两个机场各 2 个机位。两个种子共 52 条失败路线，每一条的起点或终点都落在这 20 个车位上。
- 做法是按车位声明，不按载具种类推（种类是 mod 自己起的字符串，Core 不认识）：
  - `ParkingSlotSpec` 加成员 `offRoad`（默认 false），Core 抄到 `ParkingSlot::offRoad`，**进存档**。
  - `BuildingMod` 加一个非虚重载 `AssignParking(level, slot, spec)`，直接收完整的 `ParkingSlotSpec`。原来五个参数的 `AssignParking` 签名没动，内部改成填好 spec 再转给它。
  - 港口的船位、机场的机位改用这个重载，声明 `stocked = true`、`offRoad = true`。
- 测试跟着改：
  - route、perform 只在 `offRoad == false` 的车位之间抽样。
  - route 新增检查"接公路的车位都在车行图里"：以后哪个停车场的 layout 漏画了车行连接，这条会失败并按房间列出来。
  - route 新增检查"`offRoad` 的车位不在车行图里"：防止标错。
  - save 新增检查"车位的几何、可停种类、`offRoad` 存读一致"。
  - route 的车辆部分每条失败路线会记一行（起点房间、终点房间、状态码）。

**需要你知道的**

- **旧存档读不了了**：每个车位多存一个字节，按约定没升版本号。（第十五轮删 `lodMaterialPath` 已经让旧存档失效，这次是同一批。）
- `ParkingSlotSpec` 的对象布局变了，`build.bat` 里的四个外部 mod 已经一起重编；`Tools` 下单独编译的 RoadnetViz、JunctionTest、ModHost、ModWeb 没有重编。
- `BuildingMod::AssignStockedParking` 在港口和机场改用新重载之后没有调用方了，随后按你的决定删掉。要开局就停着车的车位，现在的写法是 `AssignParking(level, slot, spec)` 并把 `spec.stocked` 设为 true。提到它的注释和文档（`building_mod.md`、`room.md`、`vehicle_boat.md`、`Test/README.md`、`Tools/VehicleRig/README.md`）一起改了。
- Core 只抄 `offRoad` 这个标记，自己没有任何逻辑读它，目前唯一的使用方是测试。寻路本身没有改：往 `offRoad` 车位规划公路路线仍然是返回失败，和以前一样。

**验证**

以下是删掉 `AssignStockedParking` 之后最后一次的结果：

- `build.bat` 全量通过（含四个外部 mod）。
- ModTest 跑两遍，都是 0 失败：按 `config.json` 跑是 1421 项通过；扫 `x64/Release` 和 `Forever_Mod` 两个目录下全部 mod 是 1468 项通过。
- CoreTest quick：7352 通过、8 失败（都是 polit，即 G2）；route 66 项、perform 36 项、save 90 项全过。两个种子各有 20 个 `offRoad` 车位，存读一致，开局都停了船或飞机。报告在 `Forever_UE/Saved/TestReports/review_20261126_stocked/core`。
- 没有进 UE 编辑器验证：没看市民实际走进机场/港口/火车站，也没试开船开飞机。这次没有改 UE 层代码。

---

## 第十五轮：D9 删掉没人用的代码

`VehicleMod::drivable`/`capacity` 按你的决定留着，其余四处删了。

**删了什么**

- `UForeverBuildingFrameworkComponent::ResolveLodMaterial` 和成员 `defaultLodMaterial`：没有调用方。
- `Traffic::Tick` 的 `crossedDay`、`post` 两个参数：函数里没用，现在只收当前时间。
- `Map::FindPath` 的 `coords` 参数：函数里早就不用它了，节点坐标来自编译好的寻路图。
- `Config::IsDllActive`、`Config::GetActiveDllPaths`：没有调用方。

对应文档一起改了：`ForeverBuildingFrameworkComponent.md`、`traffic.md`、`map.md`、`config.md`。

**顺手改正的一句文档**

- `map.md` 原来说车辆寻路"不传 coords，是纯 Dijkstra"。实际上行人图和车辆图走同一段代码：图里所有节点都有坐标就用 A*，否则才是 Dijkstra。已按代码改。

**需要你知道的**

- 删掉 `ResolveLodMaterial` 之后，`BuildingMod::lodMaterial` 这个字段就完全没人读了。随后按你的决定一起删了：
  - `BuildingMod::lodMaterial`、`Building::lodMaterialPath`、`Building::GetLodMaterialPath()`。
  - 存档里 Building 的这一个字符串（读、写两处）。
  - Basic 里 6 个建筑 mod 的注释"lodMaterial 留空 -> 用渲染层默认灰色"。
  - UE 层的材质缓存 `lodMaterialCache` 改名 `materialCache`：它缓存的是墙/地/顶/空隙材质，和 LOD 已经无关。
- **旧存档读不了了。** 存档里少了一个字符串，按约定没有升版本号、没有写迁移，所以读旧存档不会报"版本不对"，而是从建筑这一段开始读错位。
- `BuildingMod` 少了一个成员，对象布局变了。`build.bat` 里的四个外部 mod 已经一起重编；`Tools` 下单独编译的程序（RoadnetViz、JunctionTest、ModHost、ModWeb）没有重编，用之前要重编一次。

**验证**

- `build.bat` 全量通过。
- ModTest：1421 / 1468 项全过。
- CoreTest quick：7332 通过、22 失败，失败的 22 项和上一轮逐条相同。报告在 `Forever_UE/Saved/TestReports/review_20261125_d9/core`。
- 没有进 UE 编辑器验证。改动在 UE 层只是删掉一个没人调用的函数和一个没人读的成员，编译通过即可说明没有别处引用。
- 删 `lodMaterial` 之后重新验证：`build.bat` 全量通过；ModTest 1421 / 1468 项全过；CoreTest quick 里 save 套件 88 项全过（存档读写改动由它覆盖）。同样没有进 UE 编辑器验证。

---

## 第十四轮：F2 文档里引用的名字逐个过了一遍

只改了文档，没有动代码，所以这一轮不需要编译和跑测试。

**做法**

- 重扫全部 388 份 md：反引号里的标识符和文件名，在工作区的代码、配置、资源文件名里一个都找不到的，列出来。
- 收紧扫描条件后剩 194 个名字、283 处。这 194 个我逐个看了上下文，拿不准的回代码里查。
- 其中 43 个是真过时，改了 35 份文档、80 处。改完重扫，这 43 个全部消失，没有新增。
- 剩下 151 个名字、211 处都是误报，分类见下面。
- 之后按你的决定处理了 F 节另外两条：F1（文档里的日期和变更记录）不管；F3 把"待办"里提到的不存在的名字删了——`story.md` 的两条待办（`AddTalk`/`GetHistory`/`AddMessage`/`PopMessages`、`AddScript`/`RemoveScript`）整条删掉，`ForeverWeaponComponent.md` 里只删了 `ArrestPlayerChange`/`ReleasePlayerChange` 这两个名字，通缉/警察/反抗小游戏的待办本身还留着。

**真过时的（已改）**

- 改名或删掉的函数：
  - `EnsureMapGenerated`（9 处，分布在 `map.md`、`map_save.md` 和 Forever 层 5 份文档）。它早就拆成 `BuildMap*` 和 `Generate*Mesh` 两组阶段，由 `BuildGenerationStages` 排顺序。
  - `EnsureModsRegistered`、`EnsureSpawned`、`LookupTerrain`、`Citizen::ResumeWalk`、`StartTestNavigation`、`BuildFarSection`、`BuildFloorMesh`、`MakeLiteralExpression`、`OrganizationMod::ComponentRequirements`、`Door::DoorAccess`、`batchesPerDay`。
- 不存在的文件：`story/condition.h`（现在是 `expression.h`）、两处 `test.json`（现在是 `test.script`）、`USectionSpeakingWidget.h`、Wxdj 的 `nn_weights.js` 和 `ai/transformer/` 下的几个路径（训练代码已经搬到 `Train/`）。
- 过时的资产名和模板名：路面资产 `default_1_1`、地形贴图 `T_Plain_D`、layout 模板 `preset_single_room_*`、`preset_train_station_*`、`preset_tshape_double_fg`。
- 两个党派 mod 文档里的政策名，改成代码里的 `PolicyId::k…`。
- 用已经不存在的东西举例：`Forever_Mod/README.md` 的 `PengzhanBuilding` 换成占位写法 `XxxBuilding`；`random.md` 里提到已移除的 EconSim。

**顺手发现的内容错误（不只是名字）**

查名字时读到的上下文和代码对不上，一并按代码改了：

- `building_airport.md` 里留着一份旧的火车站说明（全图唯一一座、带地下室），和 `building_train_station.md` 以及代码矛盾。火车站的部分改成指向 `building_train_station.md`。
- `building_industry_high.md` 的 `Layout()` 一段数字全是旧的（楼体占比、是否有地下室、层高、模板名），按代码重写。
- `ForeverBuildingFrameworkComponent.md` 说碰撞盒在 `Init()` 里建，实际是近处 LOD 的 `BuildTriggers` 操作里建。
- `ForeverRoadnetFrameworkComponent.md` 说所有路共用一个路面资产、只建一个 ISM，实际按车道配置有多种资产。
- `Tools/ModWeb/README.md` 说打包出的 html 约 300 KB，实际约 14 MB。

**误报的类别（没动）**

- UE 引擎和 Win32 自带的头文件、类名。
- 占位和简称：`ModName`、`RegisterX`、用 `IndustryBuilding` 指高/中/低三个类。
- 故意写出来的旧名字：说明"已经删掉"或"不要这样做"的地方，比如 `BindRandomHost`、`SetConfig`、`ApplyArgs`、`DoorMod`。
- 生成物和被 git 忽略的文件：`forever_mods.html`、`furniture.json`、`modtest.json`、`ys.script`。
- 资产、骨骼、材质的名字，以及路面资产 `default_1_0_1_1_0_1` 这种代码里拼出来的名字。
- 标成"旧做法"的段落里的变量名，以及"待办"里还没实现的名字（F3，随后已删）。

**需要你知道的**

- **这次扫描只能发现"名字已经不存在"的过时。** 名字都还在、但描述的行为已经变了的文档，扫描发现不了。上面五处内容错误是查名字时碰巧读到的，别的文档里很可能还有。
- `battle.md` 原来写着一个 C++ 和网页 JS 的对拍程序 `ai_test.cpp`，仓库里没有这个文件。我把那句改成"对拍程序没有进仓库"。如果它在你别的目录里，告诉我路径我再改回去。

---

## 第十三轮：D8 配置里的路径只有一种写法

**做法**

- `Config` 加了 `NormalizePath`：转成绝对路径，去掉 `.`/`..` 和多余的分隔符，正斜杠，结尾不带斜杠，盘符大写。
- 添加和移除 dll 目录、资源目录、剧情脚本这六个入口，一进来就先规范化。同一个目录不管怎么写，都只算一个根。
- 不只是 `resource_paths`：`dll_paths` 和 `story_scripts` 有同样的问题，一起处理了。

**为什么用词法规范化，不用 `canonical`**

- `canonical` 会解析目录连接（junction），把经过它的路径换成真实目标。打包产物里 `Resource/` 就是 junction，换掉之后写进配置的就是开发机上的路径了。
- Windows 自己解析 `..` 也是按字面处理的，所以词法规范化前后指的是同一个目录，不改变含义。

**需要你知道的**

- **`config.json` 我没有手改**。里面那两条带 `..` 的资源路径还在。读进来时已经按规范写法处理，游戏下次保存配置时会写回规范写法。
- 大小写只统一了盘符。路径其余部分大小写不同，仍然算两个写法。

**验证**

- `build.bat` 全量通过。
- ModTest：1421 / 1468 项全过。
- CoreTest quick：7332 通过、22 失败。失败的 22 项和上一轮逐条相同；多出的 5 项通过是 mods 套件里新加的检查：
  - 配置里的 dll 目录和资源目录都是规范写法（当前 `config.json` 那两条带 `..` 的路径读进来后已经是规范的）。
  - 用另一种写法（带 `..`、反斜杠、结尾斜杠、盘符小写）再加一次同一个目录，不会多出一个根；用另一种写法也能移除。
- 没有进 UE 编辑器验证。配置界面添加、移除目录走的就是这几个函数。

---

## 第十二轮：D1 concept 清单只留一份

**原来**：23 个 concept 的清单在 Core 里手写了 7 份（成员、取 Factory 的函数、注册、灌参数、查询、卸载用的遍历、存档），ModTest 里还有 2 份。加一个 concept 要改所有这些地方，漏一处编译不报错。

**现在**

- `Registry` 里只有一份清单：`ForEachConcept`，每个 concept 一行，给出名字、Factory、创建和销毁函数。
- 注册、灌参数、查询、数活实例、注销，以及 `save.cpp` 的逐 concept 存取，全都从这一份遍历。
- 导出函数名不再重复写：注册时按 concept 名去 Dependence 的 `mod_concepts` 表里查。原来注册那份清单里把 `RegisterModTerrains` 这类名字又写了一遍。
- Core 里从 7 份变成 3 份（成员、取 Factory 的函数、`ForEachConcept`），三份挨在 [registry.h](Forever_UE/Source/Core/common/registry.h) 里。
- **启动自检**：`Registry` 构造时核对这份清单和 `mod_concepts` 表一一对上，对不上抛异常。CoreTest 的 mods 套件另有一条检查。
- `Config` 里"单选 concept"的名单（路网、取名）原来写了两遍，合成了一处。

**以后新增 concept 要改的地方**

1. `Dependence/common/mod_concepts.cpp` 的导出符号表加一行。
2. `registry.h`：`ForEachConcept` 加一行，加取 Factory 的函数和成员。
3. `Test/Mod/mod_checks.cpp`：两处各加一行（ModTest 启动时也会核对）。

漏了 `ForEachConcept` 那一行，一启动就报错；别的地方不用再动。

**没动的**：ModTest 里那 2 份。它本来就有启动核对，而且它不依赖 Core，用不了 `Registry` 的清单。

**为什么没用宏**：最初的建议是用宏从一张表生成全部，能只剩一份。但成员和函数名由宏拼出来，编辑器里跳转和搜索都不好用，所以没这么做。

**验证**

- `build.bat` 全量通过。
- ModTest：1421 / 1468 项全过。
- CoreTest quick：7327 通过、22 失败。失败的 22 项和上一轮逐条相同；通过数多 1 是新加的清单检查。存档（save）和卸载（unload）两个套件全过，它们直接依赖这份清单。
- 自检实测：临时从 `ForEachConcept` 里拿掉 `Parties` 一行重编，CoreTest 一启动就报 `Registry::ForEachConcept does not match Dependence/common/mod_concepts: missing concept 'Parties'`；还原后恢复正常。
- 没有进 UE 编辑器验证。这是纯整理，UE 用到的接口没变。

---

## 第十一轮：D3 mod dll 只在一局期间留在进程里

**效果**

- 菜单界面上——刚打开程序，或者打完一局退回来——进程里没有任何 mod dll。可以随意添加、去掉、替换 dll，编辑器和打包版一样。
- 开局时按当时的配置加载；一局结束、七个域都销毁之后注销并卸载。

**做法**

- 23 个 Factory 各加了 `UnregisterExcept`：注册项里的函数指针指向 dll 的代码，卸载前必须先注销。
- `Registry::UnloadMods`：先数 23 个 Factory 里还活着的 mod 实例。**只留下有活实例的 dll**（连同它的注册项），其余照常卸载，并打警告写明是哪个 mod id、几个实例、留下了哪个 dll。
- UE 的 `AForeverFrameworkActor::EndPlay` 在各域销毁之后调它。
- 菜单里读档前核对 mod 状态版本会把 dll 加载进来；校验失败、不进游戏时也卸载回去。游戏进行中发起的读档不卸载。
- 读档校验那条路原来只刷新参数，现在改成先确保 dll 已加载（平时它们是卸载的）。
- `UPuzzleWidget` 持有 mod 里创建的小游戏实例，原来关卡结束时如果小游戏还开着就不会销毁。加了 `AbortPuzzle`，`EndPlay` 卸载前调，Widget 销毁时再兜底一次。

**测试：CoreTest 新增 `unload` 套件**

它是 CoreTest 里唯一会销毁世界的地方（以前从不销毁，Core 的销毁路径没被测过）。对已生成的世界连做 3 轮"销毁 → 查泄漏 → 卸载 → 重新加载 → 再生成"：

- 销毁后没有活着的 `Script`，23 个 Factory 里没有活着的 mod 实例。
- 每个 dll 都卸载，而且确实不在进程里；没有 mod id 还留着注册。
- 卸载重载后，同一个种子生成的世界和第一次一致。
- 相邻两轮销毁后的内存不持续上涨。
- 最后故意留一个取名 mod 的实例不销毁，查保护：只有它所在的 dll 被留下，别的照常卸载，实例还能用、还能销毁，销毁后再卸一次全部走干净。

**测试查出来的一个 Core 问题（已修）**

- 第一次跑时，卸载重载后同种子的世界和第一次**不一样**：节点 id 不同。
- 原因是节点 id 计数器 `Node::count` 是进程级的静态变量，新开一局不复位，第二局的节点 id 接着上一局往后编。这和卸载无关，游戏里连开两局本来就有。
- 现在 `Map::InitTerrains` 开头把它复位。修完后 3 轮生成的世界逐项一致，说明没有别的跨局残留影响生成。

**UE 侧的注意事项写进了 `CONVENTIONS.md` 第 13 节**

- 活得比关卡长的对象（子系统、静态变量、菜单 Widget）不能持有 mod 实例、Core 对象、mod 返回的字符串指针。
- 经 Factory 创建、由 UE 对象持有的东西，必须在 `EndPlay` 卸载之前销毁，不能指望析构顺序。
- 菜单里的代码不要碰 `Registry`；确实要用，用之前加载、不进游戏就卸载。游戏进行中绝不能卸载。
- 会碰 Core 的异步工作要在卸载之前跑完。
- 我按这几条排查了现有 UE 代码：6 个子系统都不持有 Core 指针，唯一需要改的是上面的 `UPuzzleWidget`。

**做不到的**

- mod 的 UE 内容（打包版的 `.pak`、编辑器里挂载的 Content 目录）挂载之后不卸载。去掉一个带内容的 mod，dll 会走，pak 文件仍被占用到退出程序；不退出程序替换同一个 mod 的 pak 不会生效。

**验证**

- `build.bat` 全量通过（Framework、UE 模块、四个外部 mod）。
- ModTest：1421 / 1468 项全过。
- CoreTest quick：7326 通过、22 失败。失败的 22 项和上一轮逐条相同；通过数多的 98 项是新的 `unload` 套件，全过。
- 内存数字（种子 11）：生成后约 87 MB，销毁卸载后回到约 8–12 MB，每轮涨约 1 MB。
- **UE 这一侧没有在游戏里验证**。我只能读代码排查。需要你"进游戏 → 退回菜单 → 再进游戏"试几轮，并在 `Forever.log` 里搜 `still alive after the game ended`：出现就说明有东西没跟着一局结束销毁，对应的 dll 没被卸载。

---

## 第十轮：D4 Change 按种类分派

**经过**

- 我最初建议改成"按类型名查表、各组件自己登记处理函数"，你说不改、按现有逻辑继续扩展。
- 之后你问 C++ 里怎么避免几十上百次比较，选了"类型标签 + `switch`"，并要求能用上的地方都改。

**做法**

- [change.h](Forever_UE/Source/Dependence/story/change.h) 加了 `enum class ChangeKind`，65 个子类各一个。
- 每个子类带一个静态常量 `Kind`，构造时传给基类（128 个构造函数都改了）。标签和类在构造函数里绑死。
- 执行处从 `if (dynamic_cast…)` 链换成 `switch (change->GetKind())`，进了分支再 `static_cast`。分支体原样没动。
- 只认一两种的地方用新加的 `ChangeCast<XChange>(change)`，用法和 `dynamic_cast` 一样，但只比较一个整数。

**改到的地方**

| 位置 | 原来 | 现在 |
|---|---|---|
| UE `AForeverFrameworkActor::ApplyChange` | 39 种类型的 `dynamic_cast` 链 | 37 个 `case`，前后两小段 `add_option`/`remove_option` 用 `ChangeCast` |
| Core `Map`/`Player`/`Populace`/`Traffic` 的 `ApplyChange` | 共 13 个 `dynamic_cast` | 共 13 个 `case` |
| Core `Story::ApplyChange`、`script_mod.h` 的 `FindLabel` | 各 1 个 `dynamic_cast` | `ChangeCast` |
| Core `Script::BuildChanges`（解析） | 43 个分支的字符串比较链 | 先把名字换成种类，再 `switch`，45 个 `case` |
| 类型名 | 65 个类各自一个虚函数 `GetType()` | 只剩一张名字表，`GetType()` 查表，不再是虚函数 |

**以后加一种 Change**（写在 `change.md` 里）

1. `change.h` 枚举末尾加一个值，加类。
2. `change.cpp` 名字表末尾加名字，写构造函数。
3. `Script::BuildChanges` 加一个 `case` 解析。
4. 执行处加一个 `case`。

**需要你知道的**

- **名字表的顺序要和枚举一致**。个数有 `static_assert` 核对，顺序没有编译期检查，靠"新种类都加在两边的末尾"保证。
- **枚举数值不能写进存档**，中间插入新种类后面的都会变。目前没有地方持久化它。
- 脚本里写了不认识的类型名，报错从 `Change type not implemented yet` 变成了 `Unknown change type`；有类但还没写解析的仍报前者。

**验证**

- `build.bat` 全量通过（Framework、UE 模块、四个外部 mod），一次编过。
- ModTest：1421 / 1468 项全过。
- CoreTest quick：7228 通过、22 失败，失败项连同数字和上一轮逐条相同。世界生成会解析并执行大量脚本，所以这覆盖了解析链和 Core 各域的执行。
- 另写脚本核对了机械转换的结果：
  - 枚举顺序、名字表顺序、每个类的 `Kind`，和改之前每个类 `GetType()` 返回的名字逐一对得上（65 个）。
  - 128 个构造函数都把 `Kind` 传给了基类。
  - 每个 `case` 里 `static_cast` 的类和 `case` 的种类一致（UE 37 个、Core 13 个），解析链每个 `case` 里 `new` 的类也一致。
- **UE 的执行链没有在游戏里跑过**。转换是机械的，上面的核对能排除"标签和类对不上"，但排除不了我没想到的问题。需要你进游戏把常用的脚本动作过一遍。

---

## 第九轮：D5 Roadnet/Name 的启用方式和其它 Factory 统一

**先更正**：D5 原来写的是"只有 Roadnet 特殊"，这是我写错了。`RoadnetFactory` 和 `NameFactory` 的实现逐行相同，Core 里选用它们的代码也是同一个模式。真正的不一致是这两个和另外 21 个 Factory 之间。

**原来的问题**

- "是否启用"有两份记录：除了配置给的参数表，这两个 Factory 各自多存一张启用标记表，由 Core 每次开局手动先清空再填，读档路径里也各有一份（一共 4 处）。
- `CheckRegistered`/`GetRegisteredIds` 不看是否启用，和另外 21 个的语义不同。
- 配置里一个都没列时"退化选第一个被注册的"，而"第一个"取自哈希表的遍历顺序，选到哪个没有保证。
- 缺的时候两边行为不一样：路网打警告后跳过，取名抛异常。

**改法**

- 两个 Factory 去掉 `SetConfig`、`GetRoadnet`/`GetName` 和启用标记表，改成和另外 21 个一样：`Create`、`CheckRegistered`、`GetRegisteredIds` 都按"已注册且在配置里"。
- "用哪一个"放到 `Config`：新增 `GetUniqueConceptMod`，返回配置里启用的那一个；列了多个时取第一个并打警告。
- `Map::InitRoadnet` 和 `Populace::InitNames` 都调它，各自的"清空再填"和读档路径里的 4 行手动设置都删了。
- 去掉"选第一个被注册的"兜底。
- 缺的时候两边统一：一个都没启用，或者启用的那个没有 dll 提供，都直接抛异常。

**行为上的变化**

- 没有启用任何路网 mod 时，以前是打警告后生成一张没有路的地图，现在直接报错退出。配置界面开局前本来就会拦住这种配置，直接改 `config.json` 才会碰到。
- 脚本依赖一个没启用的路网或取名 mod 时，`Registry::CheckModRegistered` 现在会如实返回 false。

**验证**

- `build.bat` 全量通过（Framework、UE 模块、四个外部 mod）。
- ModTest：按 `config.json` 1421 项全过，全量 1468 项全过。
- CoreTest quick：7228 通过、22 失败，失败项连同数字和上一轮逐条相同。读档套件（save）全过，说明删掉读档路径里的手动设置没有影响。
- 报错路径，用改过的配置跑 world 套件：
  - `roadnet_mods` 清空：`map.roadnet: exception No roadnet mod enabled.`
  - `name_mods` 清空：`populace: exception No name mod enabled.`
  - 路网配成一个不存在的 id：`Roadnet mod 'roadnet_does_not_exist' is enabled in the config but no loaded dll provides it.`
- 配置里列两个路网：world 套件 88 项全过。不过这条路上配置在到达 `GetUniqueConceptMod` 之前已经被收口成一个了，"列了多个取第一个并打警告"那个分支本身没有被触发到。
- 没有进 UE 编辑器验证。缺路网时游戏里的表现（异常被 `BeginPlay` 的 try/catch 接住后打日志退出）走的是取名原来就有的那条路，我没有实际看过。

---

## 第八轮：D6 `debugf` 重做

**做法**

- `debugf` 多了一个级别参数：`debugf(DebugLevel::Warning, "...\n", ...)`，级别是 `Log`/`Warning`/`Error`。
- 它只负责格式化，文本交给一个可以替换的输出函数。`OutputDebugString` 去掉了。
- 默认输出函数写标准错误，前面带 `[Log]`/`[Warning]`/`[Error]`。CoreTest、ModTest 这类控制台程序不用做任何设置就能看到。
- UE 的 Forever 模块启动时换成自己的输出函数，按级别打成 `UE_LOG(LogForeverCore, ...)`，进 `Forever.log`。
- 139 处调用全部改了：100 处 Warning、38 处 Log、1 处 Error。消息文本里原来的 `Warning: `/`Log: ` 前缀去掉了。
- `Config` 里 7 处和表达式解析里 1 处 `std::cerr` 也改成了 `debugf`，现在只有一条路。

**mod 的 dll**

- 每个 mod dll 有自己的一份 `debugf`，所以宿主加载 dll 时要把输出函数交过去。
- 原来只绑随机数的 `FOREVER_MOD_BIND_RANDOM`（导出 `BindRandomHost`）删掉了，换成 `FOREVER_MOD_BIND_HOST`（导出 `BindModHost`），一次把随机流和输出函数都交过去。定义挪到了 [mod_concepts.h](Forever_UE/Source/Dependence/common/mod_concepts.h)。
- Basic 和 `Forever_Mod` 下四个 mod 的入口文件各改了两行（include 和宏名）。
- 以后宿主还有别的东西要交给 mod，往 `ModHostBinding` 末尾加字段，不用再加导出函数。

**CoreTest 里实际是什么样**

- CoreTest 的大多数套件是在子进程里跑的，子进程的输出重定向到 `<报告目录>/child/*.log`。所以 Core 和 mod 的日志在这些文件里，不在控制台上。
- 这次完整跑一遍：子进程日志里有 3775 行 Log、24 行 Warning；主进程的标准错误只有 7 行。改之前这些文件里一行 Core 的日志都没有。
- 24 行 Warning 都是有意义的，比如 `TrainRoute manual: gap link ... bends more than 20 deg` 和预设/绿化 id 找不到时的退化提示。

**需要你留意的**

- **Log 量**：第一次跑时 3775 行 Log 里有 2319 行是 `Vehicle X parked at Y`，每辆开局停好的车一行。按你的决定删掉了这两处（`Traffic::InitStockedVehicles` 和 `CreateOwnedVehicle` 里），各自的汇总行（停了多少辆）还在。删完重跑，Log 降到 1456 行，Warning 仍是 24 行，测试结果不变。
- **日志类别名**：本来想叫 `LogCore`，和引擎自带的类别重名，编不过，改成了 `LogForeverCore`。
- **绑定只在加载时做一次**：宿主之后再换输出函数，已经加载的 dll 拿到的还是旧的。目前只有模块启动时换一次，没有问题。

**顺带修的**

- `ModTest.md` 的"关键设计"末尾有三条说明，是我第四轮改文档时连带删掉的（已随你的提交入库）。这次补回来了。

**验证**

- `build.bat` 全量通过（Framework、UE 模块、四个外部 mod）。中途失败过两次，都是我的问题：一处漏了 include，一处是上面说的类别重名。
- ModTest：按 `config.json` 1421 项全过，全量 1468 项全过。mod 的日志出现在标准错误里，带级别前缀。
- CoreTest quick：7228 通过、22 失败，失败项连同数字和上一轮逐条相同。
- ModWeb 重新构建通过（它自带一个 `debugf` 替身，签名跟着改了）。
- **没有进 UE 编辑器验证**。需要你看一眼 `Forever.log` 里有没有 `LogForeverCore` 的行，以及 mod 里打的日志（比如 Basic 生成地形时的那几条）是不是也在。

---

## 第七轮：D7 扫描 mod 时不再加载无关的 dll

**做法**

- 新增 `ProbeModDll`（[mod_concepts.h](Forever_UE/Source/Dependence/common/mod_concepts.h)）：只读文件、解析 PE 的导出表和导入表，不加载、不执行。
- `Config::AddDllPath` 和 ModTest 的扫描都先用它预筛，只有导出了 `GetMod<Concepts>` 的 dll 才 `LoadLibrary`。
- 无关 dll 的 `DllMain` 和全局构造不会再因为"探测一下"而被执行。mod 自己的 dll 仍然要加载，取 id 列表必须调它的函数。

**顺带换掉的一个约定**

- 跳过调试版 dll 原来靠"路径里有没有 `Debug` 这一段目录名"。现在看导入表里有没有调试版 CRT（`MSVCP140D.dll`、`VCRUNTIME140D.dll`、`ucrtbased.dll`）。
- 目录名规则删掉了，Debug 版 dll 放在任何目录名下都能挡住。
- 看不出来的情况：静态链接调试 CRT（`/MTd`）的 dll。本工程的 mod 统一用动态 CRT。

**验证**

- `build.bat` 全量通过。
- 造了一个"被加载就在旁边写一个标记文件"的测试 dll（不是 mod），先确认它被真的 `LoadLibrary` 时确实会写标记，然后：
  - ModTest 扫描放着它的目录：只测了目录里的 mod，没有标记文件。
  - CoreTest 的 mods 套件把这个目录加进 `dll_paths`：发现的 mod 从 146 个变成 148 个（目录里放了一份 Wxdj），没有标记文件。说明 `Config::AddDllPath` 扫了这个目录，但没加载它。
- 调试版检测：造了一个导出 `GetModTerrains`、用 `/MDd` 编的假 mod，以及系统里真的 `msvcp140d.dll`。两个都被判成调试版、没有被加载；命令行点名时报 `it is a Debug build`。
- 目录名不再起作用：把 Release 版的 `Wxdj.dll` 放进名叫 `Debug` 的目录，照常被识别和测试。
- 其它输入：点名一个非 mod 的 dll 报 `exports none of the GetMod<Concepts> functions`；点名一个不是 dll 的文件或 32 位 dll 报 `not a readable 64-bit dll`。
- ModTest：按 `config.json` 1421 项全过，全量 1468 项全过，和上一轮一样。
- CoreTest quick：7228 通过、22 失败，失败项连同数字和上一轮逐条相同。
- 没有进 UE 编辑器验证。配置界面里"添加 mod 目录"走的就是 `Config::AddDllPath`，逻辑和上面 CoreTest 验证的是同一段。

---

## 第六轮：B1、B2、B4、B5、B6（B9 不再关注）

按你的 6 点指示改的，条目已从 B 节删掉。

| 条目 | 做了什么 |
|---|---|
| B1 开局装备 | UE 的 `PossessedBy` 不再发装备。同一套装备改由 `Forever_Mod/Test/Resource/test.script` 在 `game_start` 里用 `grant_asset` 发 |
| B2 `TestNavigate` | 去掉 `green_park` 的类型过滤，任意带活动圆的绿化区里随机一个 |
| B4 线路调试网格 | `bus`/`train`/`plane`/`other` 四组成员换成按 `stationType` 建的两个 `TMap`，颜色按类型名哈希取 |
| B5 `vehicle_command` | 入口改名成 `StartVehicleLineRun`/`StopVehicleLineRun`，脚本词汇的说明改成中性的 |
| B6 触发盒 | 所有载具都按自己网格的包围盒算，不再看是不是 `car` |
| B9 | 你决定就这么放，不再关注 |

**B1 为此加的东西**

- `grant_asset` 多了一个可选字段 `slot`：指定放到哪（`"leftShoulder"`、`"rightShoulder"`、`"back"`、手或容器路径）。原来它只会按"背上→背包→手"的顺序自动放，没法把武器挂到肩膀上。
- `test.script` 里的顺序是：先给背包，手枪挂左肩、步枪挂右肩，其余不写 `slot` 自动进背包。必须排在 `control` 前面，占有角色时才会激活肩膀上的武器。

**行为上的变化**

- B1：只有主线脚本列表里有 `test.script` 时才有开局装备。用别的主线（比如五行斗诀）开局，玩家身上是空的，要装备得在那份脚本里自己发。
- B1：`test.script` 引用了 Basic 的 14 个资产 id，但我没给它加 `mod_dependences`。某个资产 mod 没启用时那一项只打一条警告，和原来"创建不出来就跳过"的行为一致。
- B4：调试线的颜色变了（原来公交绿、火车蓝、飞机红），现在是哈希出来的。
- B6：轿车的触发盒从固定的 250×250×120 变成"车身包围盒外扩一圈"，前后方向会长一些。

**B5 没解决的部分**

- B5 原来写的是"随 B3 一起改"。这次只改了入口的名字和说明，里面仍然是公交那一套（`CollectBusLoops`、`ABusController`）。真正让任意线路都能用，要等 B3。

**验证**

- `build.bat` 全量通过（Framework、UE 模块、四个外部 mod）。
- ModTest：按 `config.json` 1421 项全过，全量 1468 项全过。
- CoreTest quick：7228 通过、22 失败，比上一轮多 2 项失败。**这 2 项不是这轮改动引起的**：你在提交 `1d86cff` 里改了 `small.roadnet`，地图变了。我把寻路套件指向改之前的那份地图重跑，结果和上一轮完全一样（39 个孤立房间、18/300）。详见 G 节。
- 没有进 UE 编辑器验证。这一轮几乎全是 UE 侧的改动，下面几件事都需要进游戏看：
  - B1：开局后左肩手枪、右肩步枪、背包里的东西是否和原来一样；读档后不会重复发。
  - B4：线路调试线能否正常显示、颜色是否分得开。
  - B6：轿车旁边的"上车"选项触发范围是否合适。

---

## 第五轮：A7–A11

按你的 5 点指示改的，条目已从 A 节删掉。

| 条目 | 做了什么 |
|---|---|
| A7 主线脚本兜底 | 删掉 `Config::GetMainStoryScriptPath()`。没配 `story_scripts` 就是没有主线脚本，存档清单和 UE 的依赖校验也不再兜底 |
| A8 平原材质 | Core 里 `plain` 不再声明材质（`defined=false`），三个 UE 资产路径从 Core 删掉。UE 的 `TerrainMaterialBuilder` 加了兜底：所有没声明材质的地形都用 `/Game/Asset/Terrain/Plain` 那套贴图 |
| A9 Megascans 路径 | `terrain_mod.h` 的 `MakeMegascansLayer` 删掉。Basic 没有现成的地形公共头，海滩、山、海、河四个 mod 各自写完整路径 |
| A10 `isConstruction` | 回调只表示"这格是平的"，不保证在 z=0，高度用已有的 `getHeight` 查。虚函数签名没变。棋盘格和井字路网自己加了 z=0 判断；`Map::IsGroundConstruction` 删掉 |
| A11 注释 | Core 里 17 处点名具体 mod 的注释换成中性描述 |

**行为上的变化**

- A7：`config.json` 没有 `story_scripts` 时，以前会去读名为 `test` 的脚本，现在主线是空的。
- A8：没调 `SetMaterial` 的 mod 地形以前画成灰色并报 Error，现在用平原的贴图、只打一条 Log。
- A10：第三方路网 mod 如果依赖"`isConstruction` 为真就一定在 z=0"，现在需要自己判断高度。`Forever_Mod` 下的四个 mod 都不用这个回调。

**没动的**

- A11 里"商店"这个词：`Economy` 自己有 `ShopItem` 这个概念，我只把明显当例子举的几处换成了"卖方""零售"，结构体名没改。
- `phone.h` 注释里的 `app_empty`：它描述的是代码里真实存在的特判（A4），代码没改，注释就没动。

**验证**

- `build.bat` 全量通过（Framework、UE 模块、四个外部 mod）。
- ModTest：按 `config.json` 1421 项全过，全量 1468 项全过。
- CoreTest quick：7232 通过、20 失败，失败的 20 项和之前逐条相同（G 节）。通过数没变，plateau 套件改过的那条检查仍然通过。
- 没有进 UE 编辑器验证。A8 的兜底材质需要进编辑器看一眼平原显示是否正常；A7 需要确认开局流程不受影响。

---

## 第四轮："引用了就必须声明依赖"——试过，已撤掉

**结论**

- 你提了"引用了某个 mod 就必须声明依赖"，我在 ModTest 里做成了硬性检查。
- 做完发现引用方向不等于依赖方向：楼用 `stationMod` 点名站点、房间用 `storageType`/`manufactureTypes` 点名仓库和工坊，但语义上是站点依赖楼、仓库和工坊依赖房间。
- 你决定去掉这项检查。现在 ModTest 不查依赖声明是否完整，谁依赖谁由作者按语义写。
- 依赖表里站点 → 楼、仓库/工坊 → 房间这 7 条我中途删过，已经原样恢复。

**我中途说错的一句**

- 我说过"启用了房间却没启用它点名的工坊，生成世界时会找不到 mod"。这是没查代码就下的结论，是错的。
- 实际上宿主缺了被托管的 mod，Core 只是跳过：[traffic.cpp:375](Forever_UE/Source/Core/traffic/traffic.cpp:375)、[industry.cpp:53](Forever_UE/Source/Core/industry/industry.cpp:53)。

**这一轮留下来的东西**

- **引用存在性检查变准了**（`referenced mods are provided by a loaded dll`）。
  - 原来只看少数几个字段，而且不管 Core 会不会真的用到。
  - 现在覆盖脚本、房间/组件、站点、仓库/工坊、武器/产品/弹药、载具、岗位、内部建筑、配方。
  - 园区和建筑的引用是在 `Layout` 里填的，ModTest 拿 8 块样例地块各调一次。
  - 脚本只在 Core 真的会建 Script 时才算。第三轮说 Wxdj 的建筑引用了 `script_empty`，其实它没有 `milestoneNames`，不算。
- **依赖成环检查**（`GetModDependencies has no cycle`）。成环的 mod 都启用后在配置界面里互相锁死，这条规则原来只写在 `basic.md` 里。这项是我自己加的，你已确认保留。
- **Basic 依赖表补了 17 个 mod 的声明**（[basic.cpp](Forever_UE/Source/Basic/basic.cpp)）。这些是强制检查那一版跑出来的真漏项，按 `basic.md` 原有的标准本来就该有：
  - 会建脚本但没声明 `script_empty`：3 个住宅园区、`room_residence`、`organization_manufacture`、`asset_burger`、`asset_grenade`、5 个武器资产、5 个武器。禁用 `script_empty` 时它们会让 `Script` 构造抛异常。
  - `building_train_station` 用了占位房间槽，补了 `room_empty`。
- **默认 ScriptMod 的 id 收成了一个常量**：`Dependence/story/script_mod.h` 的 `kDefaultScriptModId`，Core 原来写死的三处 `"script_empty"` 改用它。A5 本身没有解决。

**验证**

- Framework 编译通过。这一步只动了 ModTest 和 `basic.cpp`；UE 模块和外部 mod 在上一步的全量 `build.bat` 里编过，之后没有再改它们依赖的头文件。
- ModTest 按 `config.json`：1421 项全过，核对了 137 处引用。
- ModTest 全量（5 个 dll、150 个 mod）：1468 项全过，核对了 142 处引用。
- 单测 Wxdj、不带 `--with`：3 项失败（依赖和引用的 `room_empty`、`component_empty` 没有 dll 提供），符合预期。
- 成环检查在强制检查那一版验证过：临时给 Yuanshen 加了一条互相依赖，ModTest 报出 `building_yuanshen -> script_yuanshen -> building_yuanshen`，已还原。
- CoreTest quick：7232 通过、20 失败，失败的 20 项和之前逐条相同（G 节）。
- 没有进 UE 编辑器验证。

**没有测试兜底的事**

- 依赖声明写得全不全。改 mod 时要对着 `basic.md` 的判定标准自己核对。
- 可以只启用农场房间而不启用农场工坊：房间照常生成但不生产，没有任何提示。

---

## 第三轮：ModTest 拆分和 mod id 唯一性

按你的两点指示做的：

**1. mod id 必须全局唯一，由测试保证**

- 规则写进了 `CONVENTIONS.md` 第 5 节：所有 mod 的 id 全局唯一，不管属于哪个 concept、由哪个 dll 提供；不支持不同 concept 共用一个 id。
- 所以 D10 不再是问题（只用 id 做键是成立的），已从清单里删掉。
- 两处检查：ModTest 对它加载的全部 dll 查重名；CoreTest 的 mods 套件对 `config.json` 的 `dll_paths` 再查一次。

**2. 把对 mod 的测试拆成 ModTest**

- 新工程 [Source/Test/Mod](Forever_UE/Source/Test/Mod/ModTest.md)，产出 `x64/Release/ModTest.exe`，已加进 `Framework.sln`。**只链接 `Dependence.lib`，include 路径里没有 Core**，mod 作者不需要 Core 和 UE。
- 从 CoreTest 搬过去的：`UnitTest`、漏包随机、实例名唯一、各 concept 的字段检查、房间家具布局检查。新加的：id 合法性（非空、不含空白）、id 重名、`GetModDependencies` 声明的依赖是否存在、dll 是否导出 `BindRandomHost`。
- CoreTest 的 mods 套件只留"mod 接进 Core 之后"的检查：id 全局唯一、`Registry` 认得每个 mod、计数器复位、`Config` 的 mod 参数。
- 为了让不依赖 Core 的 ModTest 能加载 mod，把 23 个 concept 的导出符号表从 `Core/common/loader.cpp` 挪到了 `Dependence/common/mod_concepts.h/.cpp`，Core 这边转 include，行为不变。
- `run_tests.bat` 的 `core` 管线现在先跑 ModTest 再跑 CoreTest。
- 用法：
  - mod 作者：`ModTest MyMod.dll --with Basic.dll --with Empty.dll`（`--with` 带上被引用/依赖的 mod 所在的 dll）。
  - 你这边全量：`ModTest x64\Release ..\Forever_Mod`（重名检查覆盖全部 mod）。

**验证**

- `build.bat` 全量通过（Framework 含 ModTest、UE 模块、四个外部 mod）。
- ModTest 实跑了五种情况，行为都符合预期：
  - 按 `config.json`：3 个 dll、146 个 mod，1252 项全过。
  - 全量（Basic + `Forever_Mod` 下全部）：5 个 dll、150 个 mod，1293 项全过。
  - 单测 Wxdj、不带 `--with`：报 3 项失败（依赖的 `room_empty`、`component_empty` 和引用的 `script_empty` 没有 dll 提供）。带上 Basic 和 Empty 后全过。
  - 把 `Wxdj.dll` 复制一份改名后一起测：两个 id 都报重名，并列出是哪两个 dll。
  - 给一个不是 mod 的 dll：报"不是 mod dll"。
- `run_tests.bat quick core`：ModTest 1252 项全过；CoreTest 7232 通过、20 失败，失败的 20 项和之前逐条相同（G 节）。通过数比上一轮少，是因为逐 mod 的检查搬去了 ModTest。

**顺手修的一个小问题**：`run_tests.bat` 用 `%DATE%` 截子串拼报告目录名，在这台机器的区域格式下会拼出 `周三022611_171703` 这种名字。改成从 PowerShell 取时间戳了。

**需要你知道的**

- **给作者的 SDK 还没有打包脚本**。我假定作者除了 Dependence 的头文件和 lib，还能拿到 `ModTest.exe` 以及 `Basic.dll`/`Empty.dll`（用来 `--with`）。如果你不打算发 Basic/Empty 的 dll，那"引用/依赖的 mod 必须存在"这类检查对作者就会报失败，需要另外定策略。
- **站点的一条检查变弱了**：原来是"不是公交站就必须有端口"，这要求测试认识 `"bus"`。现在只查"声明了端口就必须有 `stationType`"。
- **家具布局检查**：模型名在内置尺寸表（Basic 的 `room_furniture_catalog.h`）里的才查占地重叠；mod 自带的模型只查锚点在不在房间里，并记一条 note，不判失败。"大房间里家具太少"只对用了内置家具的 mod 才算问题。
- ModTest 不检查"字段里引用了某个 mod，但 `GetModDependencies` 没声明依赖它"。第四轮试过做成硬性要求，后来撤掉了，原因见上一节。（当时说 Wxdj 的建筑漏声明 `script_empty` 是我看错了：它没有 `milestoneNames`，Core 不会给它建脚本，不算引用。）
- `Dependence/common/random.h` 里有一句我第二轮写的注释（"两个 concept 注册了同名 id 会共用一条流"），内容仍然属实，这轮没再动它。

---

## 第二轮修复小结（按你的 5 点指示）

| 指示 | 做了什么 | 状态 |
|---|---|---|
| 1. E 板块全部修复 | E1–E4 全修。其中 include 分组实际扫出 55 个文件不合规，重排了 52 个 | 已修，已编译 |
| 2. mod id 格式只是弱约定 | 去掉了三处假定格式的代码（A6、`CheckModIdentity` 的前缀检查、CoreTest 的 `_empty` 后缀判断），改了 `CONVENTIONS.md` 和相关文档。见下面"mod id 格式"一节 | 已修，CoreTest 通过 |
| 3. C1/C2/C3/C5/C10 | 全修 | 已修，已编译；UE 里的实际表现没跑 |
| 4. B8 | 删掉了 `ForeverModSubsystem` 里重复的 Factory 校验，函数改名 `MountModContent` | 已修，已编译 |
| 5. 刷新本文件 | 本次 | — |

**验证**

- `build.bat` 全量通过：`Framework.sln`、UE 模块 `ForeverEditor`、四个外部 mod 均 0 错误。
- CoreTest quick（第二轮当时）：7940 通过，20 失败。失败的 20 项和修复前逐条相同（见 G 节，都是旧问题）；通过数多 1 是新加的计数器复位测试。presets、save、random 三个和这次改动直接相关的套件全过。
- **没有做的验证**：没有开编辑器/PIE。C1（公交切物理的初速度和停站时间）、C3（改脚本后重开 PIE 生效）、C5（第二次开局名字从头编）、C10（存档覆盖）在 UE 里的实际表现都没跑过，只有编译和 CoreTest 能覆盖的部分是验证过的。

### mod id 格式（指示 2）

规则：`<concept>_<name>` 只是弱约定，mod id 可以是任意字符串，任何实现都不能假定 id 的格式。已写进 `CONVENTIONS.md` 第 5 节。

改掉的三处格式假定：

- [map.cpp:512](Forever_UE/Source/Core/map/map.cpp:512)：`PlacePresets` 原来用 `rfind("zone_")` 判断预设是不是园区，改成 `zoneFactory.CheckRegistered(id)`（即 A6）。
- [mod_test.cpp](Forever_UE/Source/Dependence/common/mod_test.cpp)：`CheckModIdentity`（每个 mod 的通用单元测试）原来要求 type 以 `<concept>_` 开头，去掉了这条检查，只要求 type 和 name 非空。
- 测试里原来按 `_empty` 后缀识别占位模板，改成按提供它的 dll（`Empty.dll`）识别。这段代码第三轮随逐 mod 的检查搬到了 [ModTest 的 main.cpp](Forever_UE/Source/Test/Mod/main.cpp)。

没改、需要你知道的两类：

- **按完整 id 精确比较的特判**（`station_empty`、`route_empty`、`app_empty`、`script_empty`）不算"假定格式"，但属于感知具体 mod，仍在 A4、A5 里。
- **"id 跨 concept 不重名"**：第三轮按你的决定定成了硬规则，由测试保证（原 D10）。

---

## A. Core 感知具体 mod

### - [ ] A1 每日需求的服务类型写死 `"food"`/`"video"`（已确认）
- 位置：[economy.cpp:32-36](Forever_UE/Source/Core/society/economy.cpp:32)、309-311
- 问题：`kMealServiceType="food"`、`kVideoServiceType="video"`，以及视频需求的财富阈值 `kVideoWealthLow/High`（2000 元/5000 元）都是 Core 常量。"市民需要什么服务、需要多少"是具体 mod（餐厅/视频公司）的业务。
- 影响：换一套组织 mod（没有 food/video 服务）后，Core 仍然只会产生这两种需求；新增一种服务类型必须改 Core。
- 建议：需求由 mod 声明。代码里的注释已经写了"先写死，以后由 scheduler 自定义"——比如让 `SchedulerMod` 或一个新的需求声明接口返回"(服务类型, 份数, 预算规则)"列表，Core 只做撮合。

### - [ ] A2 `AssignVehicles` 写死载具种类 `"car"`/`"truck"`（已确认）
- 位置：[traffic.cpp:565-566](Forever_UE/Source/Core/traffic/traffic.cpp:565)、585-586；常量在 32-35 行
- 问题：成年市民 20% 概率分一辆 `"car"`，组织随机 1-5 次分 `"truck"`。种类字符串是 Basic 的 `vehicle_sedan`/`vehicle_truck` 注册的，Core 不该知道。
- 附带：成年年龄用自己的 `kAdultAge = 18`，而经济/选举用的是可调政策 `GovernmentPolicy::adultAge`，两处会不一致。
- 建议：改成由 mod 声明"谁拥有哪类车、概率多少"（例如 `OrganizationMod`/`SchedulerMod` 上的字段，或车辆 mod 注册时带"适用所有者"），Core 只负责找车位和建资产。

---

## B. UE（Forever 模块）感知具体 mod

---

## C. Bug

---

## D. 设计问题

---

## F. 文档和注释

### 第一轮改了什么（已随 `29b82f6` 提交）

- 重写 `Source/Core/README.md`、`Forever/Framework/ForeverFrameworkComponent.md`。
- `CONVENTIONS.md`：第 3 节新增"Core 和 Forever(UE) 层不感知任何具体 mod"；补 `Source/Test`、Yuanshen；修正不存在的示例名和 `class.h` 分组。
- `Dependence/README.md`：concept 表补 Weapon；参数传递流程改成 `Registry::ReloadModArgs`。
- 批量修正过时引用（`MergeTemp`、`<concept>_basic.*`、`EconSim`、`GlobalAllocate` 等），按实际代码改写建筑 LOD 队列等段落，约 20 处代码注释。

### 第二轮改了什么（未提交）

- `CONVENTIONS.md` 第 5 节：把我第一轮写的"所有 mod id 一律是 `<concept>_<name>`"改成"mod id 可以是任意字符串，任何实现都不能假定格式"。
- 去掉各处"id 必须是…""不带/带 concept 前缀"的说法：`mod_test.h/.md`、`random.h`、`green_factory.md`、`party_factory.md`、`basic.md`、`Basic/README.md`、`script.md`、`registry.h/.md`、`config.md`、`loader.md`、`VehicleRig/README.md`、`Test/README.md`。
- 跟着代码改的文档：`route.md`、`route_mod.md`、`traffic.md`、`traffic_save.md`（C1/C2）；`script.md`（C3）；`save.md`（C5）；`ForeverSaveSubsystem.md`（C10）；`ForeverModSubsystem.md` 重写、`ForeverConfigBridgeSubsystem.md`、`loader.md`、`Dependence/README.md`、`Forever_Mod/README.md`（B8）；`ForeverFrameworkActor.md`、`ForeverTrafficFrameworkComponent.md`、`map.md`、`Core/README.md`。
- 新文档：`terrain_coverage.md`、`bus_stop_name.md`。

---

## G. 现有测试失败（不是这几轮改动引起的）

CoreTest quick（`--suite all --quick`）现在只剩 8 项失败，全在 polit 套件。最新报告在 `Forever_UE/Saved/TestReports/review_20261126_a3/core`。

前五轮是 20 项；第六轮起因为 `small.roadnet` 改了地图变成 22 项；第十六轮解决了 route 的 12 项（G1）和 perform 的 2 项（G3）。

### - [ ] G2 polit 套件 8 项失败（推测与小地图配置有关）
- 只出现在 seed 12：两个党派创建时都没拿到会议室，后面的党代会、会议室冲突测试跟着失败。
- 11-24 的旧报告里 polit 是全过的。中间隔了提交 `7731158`（改了 `config.json`：小地图）。我没有回退验证，是推测。
- 值得看的点：地图上没有会议室房间时党派就没有会议室，这本身是个脆弱点（会议室只出现在 `ResidenceHighBuilding` 的一种布局里）。

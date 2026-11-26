# 待办

全项目还没做的事都记在这里，代码文档里不再写待办(见 `CONVENTIONS.md` 第 1 节)。2026-11-26 从各代码文档原来的"待办"小节、代码里的 TODO 注释和原 `REVIEW.md` 汇总整理；截至这一天做过的功能都已经验证过。

- 每条后面的括号是相关的代码文档，现状和设计去那里看。
- 做完一条就删一条；新的待办直接加在对应的节里。

---

## 一、架构

### Core 不感知具体 mod

- [ ] **每日需求的服务类型写死 `"food"`/`"video"`**（原 REVIEW A1，已确认）
  - 位置：[economy.cpp:34-38](Forever_UE/Source/Core/society/economy.cpp:34)、284、311-313。
  - 问题：`kMealServiceType="food"`、`kVideoServiceType="video"`，以及视频需求的财富阈值 `kVideoWealthLow/High`（2000 元/5000 元）都是 Core 常量。"市民需要什么服务、需要多少"是餐厅、视频公司这类 mod 的业务。换一套组织 mod 后 Core 仍然只会产生这两种需求，新增一种服务类型必须改 Core。
  - 建议：需求由 mod 声明，比如让 `SchedulerMod` 或一个新的需求声明接口返回"(服务类型, 份数, 预算规则)"列表，Core 只做撮合。
- [ ] **`AssignVehicles` 写死载具种类 `"car"`/`"truck"`**（原 REVIEW A2，已确认）
  - 位置：[traffic.cpp:567-568](Forever_UE/Source/Core/traffic/traffic.cpp:567)、587-588；成年年龄常量在 35 行。
  - 问题：成年市民 20% 概率分一辆 `"car"`，组织随机 1-5 次分 `"truck"`。这是 Core 里仅剩的两个载具种类名。另外成年年龄用的是自己的 `kAdultAge = 18`，经济和选举用的是可调政策 `GovernmentPolicy::adultAge`，两处会不一致。
  - 建议：由 mod 声明"谁拥有哪类车、概率多少"（比如 `OrganizationMod`/`SchedulerMod` 上的字段，或车辆 mod 注册时带"适用所有者"），Core 只负责找车位和建资产。

### 市民

- [ ] **把动态属性（饱食度、瞌睡值）应用到所有市民**（`Core/populace/citizen.md`）
  - 现在只有被玩家操控过的市民才有这两项；瞌睡值更窄，只有正在操控的那个才扣。别的市民不会饿、不会困。
  - 市民"吃饭"现在只是经济系统每天按户扣钱，和饱食度互不相关；"睡着"只是一个标记，没有动作，不影响日程和对话。
  - 要做的是：全体市民都有这两项，由他们自己的日程去吃饭、睡觉来恢复。届时要重新考虑每帧给几千人算一遍的开销（现在的做法就是为了躲开它），以及它们和经济需求、日程的关系。
  - 一并考虑：瞌睡值掉到 0 的强制睡觉触发跳时间时，不检查是否正在对话。
- [ ] **通用的自由游走**：没有工作或日程的市民站在分配到的房间里不动（`citizen.md`、`populace.md`、`CitizenElement.md`）。
- [ ] **下班行为**：`scheduler_basic` 是占位，没有真正的下班行为（`Basic/populace/scheduler_basic.md`）。
- [ ] **离城后的空位**：离城空出的房间和岗位不会有新人迁入（`Core/society/emigration.md`）。
- [ ] **父母、兄弟姐妹"是谁"的原始数据不存档**，只保留了反推出的关系结果（`populace.md`）。
- [ ] **取名**：同一轮生成内不查重（`name.md`、`name_chinese.md`）；只有中文取名算法（`populace.md`）。

### 玩家

- [ ] **任意市民可被操控**：角色外观（骨骼网格、动画蓝图）仍是构造函数默认值，没有按被操控的市民设置（`ForeverCharacter.md`）；`ForeverPlayerState` 还没有和市民共用的数据结构（`ForeverPlayerState.md`）。
- [ ] **武器状态不按角色持久化**（`ForeverWeaponComponent.md`）。
- [ ] **玩家在餐厅购买 food 服务**（Core 接口和界面），玩家家庭按钱选超市汉堡或餐厅（`organization_restaurant.md`）。
- [ ] **全局设置字典**（`GetGlobalSettings` 一类）还没实现（`Core/common/config.md`）。

### 剧情脚本

- [ ] **没实现的 Change 和事件**：还有 19 种 Change 没有 JSON 解析分支，13 种事件没有字段级逻辑。被点名实现时，解析分支和某个域/UE 层的执行分支要一起补（`change.md`、`event.md`、`script.md`）。
- [ ] **嵌套 Option 里的 Change 本体没有回收机制**（`change.md`、`dialog.md`）。
- [ ] **分支选项的嵌套内容不能引用 `local.` 变量**；暂无脚本用到（`ForeverStoryFrameworkComponent.md`）。
- [ ] 多剧情共享同一份里程碑缓存时的命名冲突机制，真需要时再加（`script.md`）。
- [ ] 表达式语法等剧情作者反馈缺什么再扩展（`expression.md`）。
- [ ] `implement` 的请求/响应 JSON 字段命名没有统一约定；`society`/`story`/`traffic` 三个域指针只存不用（`Core/common/implement.md`）。

---

## 二、功能缺口

### 地图与建筑

- [ ] **电梯**：开关门动画、载客的市民调度没做（`Core/map/building.md`）。
- [ ] **门**：没有真正的玩家门禁判定（`CanPass` 的玩家分支恒按 Open）；市民寻路不按门禁、不按门的开合绕路（`door.md`、`DoorComponent.md`）。门的骨骼网格大多数建筑共用同一对测试资产，之后按尺寸和用途分（`DoorComponent.md`）。
- [ ] **窗户 mesh**：窗户资产有问题已删掉显示，现在门窗只是几何缺口（`building.md`、`ForeverBuildingFrameworkComponent.md`）。
- [ ] **外墙/内墙材质区分**：现在统一一份 `wallMaterial`（同上两处）。
- [ ] **园区**：内部剩余空间不会自动再分配建筑（现在是 mod 显式指定的固定列表）；建筑自己的出入口逻辑（`ConnectZoneAccessPoint` 只服务园区）（`zone.md`、`zone_mod.md`）。
- [ ] **园区围墙**：长度铺不满一节时的退化兜底渲染（`zone_mod.md`、`ForeverZoneFrameworkComponent.md`、[ForeverZoneFrameworkComponent.cpp:332](Forever_UE/Source/Forever/Framework/ForeverZoneFrameworkComponent.cpp:332)）；园区内部道路没有模型（`ForeverZoneFrameworkComponent.md`）。
- [ ] **路灯**：园区/建筑宿主的 `LayoutLamps`；灯多时按距离流式生成、回收 Actor，遍历改空间网格；昼夜亮度渐变、天气联动；近处真实光源数量上限（`lamp.md`、`ForeverLampFrameworkComponent.md`、`LampElement.md`）。
- [ ] **绿化**：地面不单独铺材质、没有地形压平（`green.md`）；`green_park` 的长椅、路灯等装饰还没放（`green_park.md`）。
- [ ] **桥墩**：先建的线路/路网不为后建的让位；桥下不判建筑，桥墩可能落在地块上（`pier.md`）。
- [ ] **路网**：`roadnet_checkboard` 和 `roadnet_jing` 重复的建路/行道树逻辑没抽成共享代码（`roadnet_checkboard.md`）；`jing` 若要全程跟随真实地形，放射臂非山体采样点的 Z 要换成真实高度（`roadnet_jing.md`）。
- [ ] **行道树**：没有避开建筑出入口；运行时增删只有组件接口，没有接到脚本指令（`ForeverVegetationFrameworkComponent.md`）。
- [ ] **地形**：`clipGridCell` 只支持矩形 hatch；没做抗平铺（`ForeverTerrainFrameworkComponent.md`）。
- [ ] **元神 mod 的楼内模板**没有行人/车行导航块，居民走不进楼（`Forever_Mod/Yuanshen/.../building_yuanshen.md`）。
- [ ] **建筑 LOD 按 mod 配置**：切换距离和每帧队列上限现在是组件级固定值，不能按 mod 配置；距离只看水平距离（`ForeverBuildingFrameworkComponent.md`）。
- [ ] **工坊产量分摊**：`StartProduce()` 的 `overallRatio` 按整个工坊统一限制，多个产线争同一种紧缺原料时需要重新设计（`manufacture.md`）。

### 交通

- [ ] **火车**：轨道与轨道、轨道与机场天空线的交叉不处理；隧道内没有专用轨道资产（`route_train.md`、`route_mod.md`、`ForeverTrafficFrameworkComponent.md`）。
- [ ] **机场**：互联线路只用前两条跑道，没有专用起飞/降落跑道（`route_air.md`）。
- [ ] **公交**：井字路网没有 4 车道大路，所以没有公交（`route_bus.md`）。
- [ ] **线路**：寻路图没有动态加线路的入口，路网线路不支持路网动态变化（`route.md`）。
- [ ] **站点挂载方式**只有"挂建筑"和"贴路"两种（`station_mod.md`）。
- [ ] **线路车不能驾驶**（`VehicleMod::drivable` 预留）（`TransitVehicleElement.md`）。
- [ ] **车辆控制器**：起步朝向和首段路径差很大时靠打满方向盘宽转弯，可能蹭墙；不处理倒车掉头；不做避障（`VehicleCommandController.md`）。
- [ ] **园区车行门**按玩家距离加载，不按车辆的流式距离，车在范围外撞门时门可能还没生成（`ForeverDoorFrameworkComponent.md`）。
- [ ] **飞机**：起落架收放、襟翼、油量、碰撞损毁没做；轮子没刷权重时不随转动画（`PlaneElement.md`）。

### 物品、武器、战斗

- [ ] **烟雾弹落地效果**没做（`asset_smoke_grenade.md`、`ForeverThrowableComponent.md`）。
- [ ] **物品网格**：除手雷、烟雾弹、汉堡外，其余物品掉在地上仍是 Cube 占位，等美术资产到了在各自的 `AssetMod` 里填 `meshPath`（`ForeverAssetFrameworkComponent.md`）。
- [ ] **伤害**：攻击者那边没有"造成伤害"事件；载具不受伤害；`area_damage` 只测中心到中心一条线的遮挡，没有部分遮挡比例，也没有 `ignore` 字段（手雷会炸到自己）；爆炸没有冲击力（`ForeverDamage.md`、`asset_grenade.md`、`ForeverRocketProjectile.md`）。
- [ ] **火箭弹**弹道沿镜头方向、从枪口出发，没有向瞄准点修正（`ForeverRocketProjectile.md`、`weapon_rpg.md`）。
- [ ] **通缉、警察、反抗小游戏**：需要先补"任意事件广播给剧情脚本"的通用机制（`ForeverWeaponComponent.md`）。
- [ ] **`Player::TakeDamage` 没有调用方**，等有会反击的市民或敌人再接（`ForeverWeaponComponent.md`）。
- [ ] **准心**：换成贴图资产，并按武器的 `baseSpread` 动态张开（`ForeverHUD.md`、`ForeverWeaponComponent.md`）。
- [ ] **爆炸特效**：焦痕贴花、爆炸音效没做；火球加噪声湍流、烟加柔边贴图（`ForeverExplosionEffect.md`）。
- [ ] **投掷列表**：只能鼠标点选，没有键盘/手柄选择；显示的是资产 id，没有中文名（`ForeverThrowableComponent.md`、`ThrowListWidget.md`）。

### 五行斗诀（Wxdj）

- [ ] **UE 的 PvE 入口没接**：给玩家/敌人传特性列表、`SetCritBase`、敌人 `SetNoAccumulate(true)`；`ai.cpp` 按权重维数加载 PvE 权重并调用 PvE 特征。PvE 训练侧暂时移除了，之后重做；接入需要另行批准（`battle.md`、`puzzle_wxdj.md`）。
- [ ] **重训**：规则或系数改动后按"从零重训"流程重训 PvP 模型并重新导出；AI 目前看不到玩家的五行属性（`battle.md`）。
- [ ] **强度评估**：火之高兴、安如磐石、天动万象三个家族的第 20 式（`battle.md`）。
- [ ] **剧情按胜负分支**：`Wxdj.script` 还没有按 `puzzle_result` 的胜负分支（`puzzle_wxdj.md`）。
- [ ] 玩家只有 `xiaohua.txt` 一套招式，没有随剧情成长或换招式表；战报面板不能滚轮回看（`puzzle_wxdj.md`）。

### 界面与输入

- [ ] **游戏内改键并持久化**；Move/Look 的模拟量组合按键配置化（`ForeverKeyBindingSubsystem.md`）。
- [ ] **识别模式**：更多信息行（年龄、职业等）；白天曝光材质（`ForeverIdentifyComponent.md`）。
- [ ] **天空**：没有月光和星空（`ForeverSkyFrameworkComponent.md`）。
- [ ] **天气**：雨雪音效、闪电、雷声没做（`ForeverWeatherFrameworkComponent.md`）。
- [ ] 对话框加防误触的最短展示时长；mod 参数对话框美化（`SectionSpeakingWidget.md`、`ModArgsDialogWidget.md`）。
- [ ] `ForeverRoomFrameworkComponent` 现在没有逻辑，按需求落地时再收进来（`ForeverRoomFrameworkComponent.md`）。

---

## 三、占位内容

这些是先放进去跑通用的测试值，不是最终设计。

- [ ] **产业数值**：三个工坊每天的目标批数远小于全城日需求，缺口靠进口补；三种产品的批大小/用量/价格、五个仓库的容量也都是占位值（`Basic/industry/*.md`、`Core/industry/industry.md`）。
- [ ] **Mohar 建筑的二楼模板**：`fu2` 目前和一楼 `fg1` 内容一样，等二楼房间范围对上模型再改（`Basic/map/building_*.md`，共 15 个建筑）。
- [ ] **商店仓库里的 10 个汉堡**是物品流式加载和捡起的测试内容，和仓库库存数字无关，捡走不扣库存、不付钱（`room_warehouse.md`）。

---

## 四、已知限制

不打算马上改，记在这里免得被当成遗漏。

- 读档失败时已经建出来的部分对象没有释放；读档失败本来就会退回重新生成世界（`map_save.md`）。
- `Map::InitTerrains` 依赖 `Config::ReadConfig` 先跑过，这个顺序只是约定，代码里没有强制（`map.md`）。
- 跳时间"按小时步进 + 午夜对齐"的步进器在 UE 层，测试里有一份等价实现，两处要保持一致（`world_step.md`）。
- 同一个 Release 版 dll 在两个 `dll_paths` 目录里都有时，注册顺序决定用哪份；建议只保留一份构建产物（`config.md`）。
- mod 的 UE 内容（pak、Content 目录）挂载后不卸载：去掉一个带内容的 mod，pak 仍被占用到退出程序（`ForeverModSubsystem.md`）。
- `IsModLocked` 每次调用都重算整个依赖闭包，蓝图逐行调用、行数多时可以缓存（`ForeverConfigBridgeSubsystem.md`）。
- 市民寻路不感知家具，靠布局器保证房间中央和门前留空；房间分区之间没有隔墙（`room.md`）。
- 路口 mesh 没有圆角/斜切，开口挖掉整条路宽；小路是不跟随地形的直线（`ForeverRoadnetFrameworkComponent.md`）。
- `ACitizenElement` 构造函数里的 `bRunPhysicsWithNoController=true` 可能是遗留，涉及行为风险没改（`CitizenElement.md`）。
- `World.umap` 里一旦手动放了真实的 `AForeverFrameworkActor`，就要删掉 `ForeverPlayerGameMode` 里的动态兜底生成（`ForeverFrameworkActor.md`、`ForeverPlayerGameMode.md`）。
- Factory 没有"探测阶段/正式生效阶段"两段式注册的隔离（`Dependence/README.md`）。

---

## 五、测试与工具

- [ ] **ModTest 的 SDK**：给 mod 作者的包（Dependence 头文件、`Dependence.lib`、`ModTest.exe`、`Basic.dll`/`Empty.dll`）还没有打包脚本（`ModTest.md`）。
- [ ] **ModTest 查不到的**：写在 mod 代码判断条件里的 id（比如组织里的 `if (roomType == "room_shop")`）；依赖声明是否完整（`ModTest.md`）。
- [ ] **逐 concept 的深度单元测试**，替换通用宏（`mod_test.md`）。
- [ ] **无头测试**：车辆流式压测、带画面的 GPU 采样。
- [ ] **字段检查**：组织、路线、路网、地块、脚本、名字、组件、应用、谜题没有可校验的静态字段，靠 UnitTest 和 world 套件覆盖（`Test/README.md`）。
- [ ] **异常穿过 mod dll 边界**这一段没有自动化覆盖（`puzzle_fault` 已删）（`CONVENTIONS.md` 第 14 节、`Test/README.md`）。

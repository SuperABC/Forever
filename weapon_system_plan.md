# 武器系统（实时射击）落地计划

基于 `origin/main`（截至 764cd05 "Download 3D assets"）的真实代码核对，供带去有UE环境的session里实现。本文档只做设计，不含任何实现代码——所有落地细节（编译、动画蓝图、资产导入、手感调优）留给你在真实环境里做。

## 一、现状核对（这份计划的所有假设都基于这些真实存在的代码）

已确认存在、可以直接复用的机制：

- **Script→Milestone引擎已完整可用**：`Core/story/{script,milestone,story}.h/cpp`。`Script::MatchEvent(event, context, post)`做事件匹配，`Story::ApplyChange(change, context)`按`dynamic_cast`分派执行（目前只有`SetValueChange`分支真正生效，其余分支打"未实现"日志——这次要用到的几个Change类型如果落在"未实现"分支里，需要顺手把对应分支补上，见下文"需要补的执行分支"）。
- **`self./system./local.`三前缀路由已实现**：`Dependence/story/expression.h`的`ScriptContext{self, system, local}`，`Script`自己是`Container`承担`self.`，`Story::systemScript`承担`system.`，触发事件本身承担`local.`。
- **`JobMod`已经有`scriptModName`+`milestoneNames`两个字段**（`Dependence/society/job_mod.h`），配套注释明确写着"将来由外部广播一个Event、走Script::MatchEvent的正常触发器匹配流程触发"——警察这个职业类型的剧情脚本挂载点已经现成，不用另外设计。
- **`AssetMod`/`PuzzleMod`还是阶段3骨架**（`Dependence/player/asset_mod.h`、`puzzle_mod.h`，都只有`GetType()`/`GetName()`两个纯虚方法），真正的字段和行为都还没补——这次要做的`WeaponMod`会依赖`AssetMod`补上"外观+容量"字段之后的样子，见下节设计决策。
- **`Player`（Core）目前只有全局时钟**（`Core/player/player.h`），生命值/背包等字段都还没迁移。

已确认存在、和武器系统直接相关的Change/Event词汇（`Dependence/story/change.h`、`event.h`，一律是"字段是string表达式，构造函数+Set/Get"这一套写法）：

| 用途 | 类型 | 关键字段 |
|---|---|---|
| 背包里选中物品点"使用"的触发事件（**武器不走这条**，见第二节更正） | `UseAssetEvent` | `asset`(资产类型id) |
| 给予/移除物品（可以拿来当弹药） | `GiveObjectChange`/`RemoveObjectChange` | `object`(类型id), `num`, `force`(移除专用) |
| 改变通缉值 | `ChangeWantedChange` | 具体字段需要读一下当前定义，按同类型（`ChangeTimeChange`等）推断是一个表达式字段 |
| 通缉值变化的结果通知事件 | `WantedChangeEvent` | 需要读一下确认字段 |
| 玩家受伤/治愈 | `PlayerInjuredChange`/`PlayerInjuredEvent`, `PlayerCuredChange`/`PlayerCuredEvent` | 具体字段需要读一下 |
| 玩家被捕/释放（**只有Event，没有对应Change，是个不对称的小缺口，见下文**） | `PlayerArrestedEvent`/`PlayerReleasedEvent` | — |
| 启动2D小游戏 / 小游戏结果 | `StartPuzzleChange`(`puzzle`类型id) / `PuzzleResultEvent`(`result`字符串) | 这就是之前聊的"战斗/反抗逮捕做成2D小游戏"的现成机制 |
| NPC死亡 | `CitizenDeceaseEvent` | — |
| 切换控制的NPC（"npc控制切换"） | `ChangeControlChange` | `name`(市民姓名表达式) |
| NPC移动到某地 | `NPCNavigateChange` | 警察追捕用得上 |

**结论**：武器系统需要新增的Story层vocabulary几乎是零——现成的`ChangeWantedChange`/`WantedChangeEvent`+`StartPuzzleChange`/`PuzzleResultEvent`+`NPCNavigateChange`+`CitizenDeceaseEvent`拼起来，就能把"开枪→目击→通缉→追捕→反抗小游戏→逮捕"整条链路走完，具体拼法见第五节（`UseAssetEvent`不适用于武器，见第二节更正）。唯一建议新增的是补齐`PlayerArrestedEvent`对应的`ArrestPlayerChange`（和`ReleasePlayerChange`），理由见第六节。

## 二、关键设计决策：Weapon 和 Asset 的关系

你之前提到"资产是可以放背包/地上/桌子的东西，mod指定外观和容量，也许可以直接做成枪"——这个思路是对的，但既然确定要做实时射击（不是纯离散小游戏式战斗），建议**不要把武器直接塞进Asset**，而是让`WeaponMod`和`AssetMod`按同一个id**双注册**：

- `AssetMod`（等你们真去补它的阶段4字段时）负责"这东西长什么样、占多大容量、能不能放地上/桌子"——这部分武器和一把椅子没有本质区别。
- `WeaponMod`只负责"这个资产如果被当成武器使用时，战斗怎么算"——伤害、后坐力、弹匣、动画集这些战斗专属字段和行为。

两者靠**同一个字符串id**关联（比如"pistol_01"同时在`AssetFactory`和`WeaponFactory`里注册），不是继承、不是互相持有指针。理由：
1. 不是所有Asset都是武器（椅子、手机、家具不需要`WeaponMod`），不是所有武器概念上都必须"能被拾取"（理论上还是建议都可拾取，但架构上不强耦合更干净）。
2. 复用你已经在用的"跨concept按id字符串关联"模式（`JobMod::occupantName`按名字关联Citizen，不用指针）。
**更正**：`UseAssetEvent`专门对应"玩家在背包界面选中物品、点击使用"这条交互路径，武器不走这条——武器有自己专门的装备UI（"装备"这个操作本身也不需要广播任何Event，纯粹是UI/状态层面"当前手持哪把武器"的切换，剧情系统不需要知道）。所以武器开火本身**默认不broadcast任何Event**，也不需要新发明一个"WeaponFireEvent"去补这个位置——真正需要通知Story的只是开火造成的后果（见第五节的"低频/叙事阈值路径"：杀死NPC、涨通缉值），"开火"这个动作本身对剧情系统是透明的，除非以后你确实需要某条剧情对"第一次开枪"之类的动作本身做反应，那时候再单独评估要不要新增一个专门的Event，这次不预先加。

## 三、WeaponMod 字段与虚方法设计

参照`BuildingMod`（`Dependence/map/building_mod.h`）和`JobMod`的写法风格：公开数据字段直接是public成员，需要mod override的行为是虚方法，默认实现放`Basic/player/weapon_basic.*`。

**挂载方式简化（这次先这么做）**：不挂在手部骨骼上、不依赖任何持枪动画——角色保持默认待机姿势（双臂自然下垂），武器Mesh挂在一个基本不动的固定点上（躯干根部骨骼，或者干脆直接挂在角色的Root/胶囊体组件上），再叠加一个每把武器自己的局部偏移量，把枪摆到肩膀前面这个视觉位置。所以`gripSocketName`这个字段现阶段填的是这个固定挂载点（留空就表示直接挂Root），新增一个`gripOffset`字段做每把武器自己的位置/朝向微调。

以后要补真实持枪动画时，只需要把`gripSocketName`从"固定躯干/Root挂载点"换成"手部Socket"，`gripOffset`这个字段的语义完全不变（手部Socket挂载同样需要按武器微调贴合），其余所有字段（射击参数、弹药、伤害计算）不用动——因为开火判定/枪口朝向计算都应该读武器Mesh当前的实际世界坐标，不应该硬编码"挂在哪"这个假设，挂载点换了这些逻辑自动跟着对。

```
class WeaponMod {
public:
    WeaponMod() = default;
    virtual ~WeaponMod() = default;

    virtual const char* GetType() const = 0;
    virtual const char* GetName() = 0;
    virtual void ApplyArgs(const std::string& args) {}   // 与其余concept一致的config.json参数入口

    // ---- 资产关联 ----
    // 对应AssetFactory里同一个id注册的AssetMod实例，武器只存id不存指针（同JobMod::occupantName模式）。
    // MVP测试阶段不接AssetMod/背包，这个字段先留空不用，见"MVP测试简化"一节。
    std::string assetId;

    // ---- 资产/动画引用（纯数据） ----
    std::string firstPersonMeshPath;      // MVP阶段必须直接填（不经过AssetMod），见"MVP测试简化"一节的下载链接
    std::string animationSetId;           // 指向一组Montage(Idle/Aim/Fire/Reload/Equip/Unequip)，不是单个动画路径
    std::string muzzleFlashEffectPath;
    std::string impactEffectPath;
    std::string shellEjectEffectPath;
    std::string fireSoundPath;
    std::string reloadSoundPath;
    std::string emptySoundPath;
    std::string gripSocketName;    // 现阶段填固定挂载点（躯干骨骼名，留空=直接挂Root）；以后换成手部Socket名
    // gripOffset: 位置+旋转的局部偏移，挂载到gripSocketName之后叠加，把枪摆到目标视觉位置/贴合手部姿势。
    // 用Core已有的偏移量/Transform表示方式（落地时对齐Dependence层现成的几何类型，不重新发明）。

    // ---- 射击参数（纯数据） ----
    float damage = 0.f;
    float fireRate = 0.f;                 // 两次开火最小间隔，秒
    enum class FireModeType { Single, Auto, Burst } fireMode = FireModeType::Single;
    float maxRange = 0.f;
    float baseSpread = 0.f;
    float aimingSpreadMultiplier = 1.f;
    float movingSpreadMultiplier = 1.f;
    float recoilVertical = 0.f;
    float recoilHorizontalRange = 0.f;
    float recoilRecoverySpeed = 0.f;

    // ---- 弹药（弹药本身建议就是一种Object，走已有的GiveObjectChange/RemoveObjectChange） ----
    int magazineCapacity = 0;             // MVP阶段依然强制生效——弹匣打光必须换弹才能继续开火
    std::string ammoObjectId;             // 对应GiveObjectChange/RemoveObjectChange的object字段——MVP不用，先留空
    int maxReserveAmmo = 0;               // MVP不用（备弹视为无限），先留空/忽略
    float reloadDuration = 0.f;

    // ---- 操控 ----
    float weight = 0.f;
    float aimDownSightTime = 0.f;
    float equipTime = 0.f;

    // ---- 行为点：默认实现走上面的纯数据字段算，特殊武器可以override ----
    // 命中判定怎么算——默认hitscan+baseSpread；特殊武器（霰弹枪多发、追踪弹等）override这个。
    virtual void Fire(/* 具体签名等落地时按Forever层的碰撞查询接口定 */) {}
    // 单发伤害怎么算——默认按distance衰减；特殊武器可以做部位倍率/穿透。
    virtual float ComputeDamage(float distance) const { return damage; }
    // 后坐力怎么施加——默认按上面几个字段直接算；特殊武器可以自定义曲线。
    virtual void ApplyRecoil(/* 输出到相机偏移的接口，落地时定 */) {}

    // ---- Mod归属元数据（配合你们的mod版本管理，存档要按WeaponId字符串引用） ----
    std::string sourceModId;
};
```

**注意**：`Fire()`/`ApplyRecoil()`的具体签名故意没有定死——这两个方法要接UE的碰撞查询/相机接口，属于"Dependence层不能看到UE类型"这条既有约束（参照`BuildingMod::Layout`用`Quad`/`Road*`这种Core类型做参数、不用任何UE类型的做法），到时候在UE环境里对着Forever层实际的碰撞查询API来定具体参数类型，这里先占位。

### MVP测试简化：切枪方式、弹药、要下载的资产

这次先做一个能测试的最小版本，明确跳过背包和UI：

- **切枪方式**：不经过背包/装备UI，直接用数字键切换。Forever层维护一个写死的"测试武器列表"（比如配置1→"pistol_01"、2→"rifle_01"这种数字键到`WeaponId`的映射），按键时销毁/隐藏当前挂载的武器组件、创建并挂载新的。复用`ForeverCharacter.h`里已经在用的Enhanced Input模式（参照`StartSprint`/`SwitchControlledCitizen`的绑定方式），新增一个"切换武器"的`UInputAction`，在`SetupPlayerInputComponent`里绑定数字键。
- **弹药**：`magazineCapacity`照常生效——弹匣打光了必须`Reload()`才能继续开火，换弹动画/耗时(`reloadDuration`)照常走。但`Reload()`这次**不检查、不消耗**任何备弹（`maxReserveAmmo`/`ammoObjectId`两个字段MVP阶段不使用），无条件把弹匣填满，相当于备弹无限——这样能测试"打光了必须换弹"这个核心手感，又不用先接`GiveObjectChange`/`RemoveObjectChange`和还没做的Asset/背包系统。等以后背包做好了，把`Reload()`里的"无条件填满"换成"检查+扣减`ammoObjectId`对应的Object数量，不够就不能换"，`WeaponMod`的其余字段和行为完全不用动。

**要下载导入的资产**——只需要武器的静态模型（不需要动画，见前面"挂载方式简化"），推荐两个免费来源，选一个先测试用：

1. [Low Poly FPS Weapons Pack Lite](https://justcreate3d.itch.io/low-poly-fps-weapons-pack-lite)（itch.io）——免费试用包，低模，适合先跑通逻辑，下载后按UE正常的Content Browser导入流程（拖FBX进去）即可。
2. [Fab Free Content](https://www.unrealengine.com/fabfreecontent)（Epic官方，每两周免费放一批，License是"UE-Only"，你们本来就是UE项目直接能用）——如果想要模型质量更好一点，去这个页面翻当前在架的免费武器包。

导入之后把实际的资产路径填进`WeaponMod::firstPersonMeshPath`（走`ApplyArgs`或者具体子类构造函数里写死都行，跟其余concept的mod参数传递方式一致）。

## 四、WeaponFactory 设计

完全照抄`BuildingFactory`的注册表模式（裸函数指针，不用`std::function`，理由见`Dependence/README.md`"关键设计"一节——跨DLL析构安全）：

```
class WeaponFactory {
public:
    using CreateFunc = WeaponMod*(*)();
    using DestroyFunc = void(*)(WeaponMod*);

    virtual void RegisterWeapon(const std::string& id, CreateFunc creator, DestroyFunc deleter);
    virtual void CleanTemp();
    virtual WeaponMod* CreateWeapon(const std::string& id);
    virtual void DestroyWeapon(WeaponMod* instance);
    virtual bool CheckRegistered(const std::string& id) const;
    virtual std::vector<std::string> GetRegisteredIds() const;
    virtual void SetModArgs(const std::unordered_map<std::string, std::string>& argsById);

private:
    // 同BuildingFactory：registries/liveInstances/configuredArgs三张表
};
```

`Basic/player/weapon_basic.h/cpp`给一份纯数据驱动的默认`WeaponMod`实现（`Fire`按`baseSpread`做单发hitscan，`ComputeDamage`按`maxRange`线性衰减），特殊武器的mod可以override构造函数设不同字段或者override虚方法。

## 五、高频/低频两条路径——这是整个设计里最重要的一条

**不要每次开火/每次命中都调`Script::MatchEvent`**——那是对`mainScript`当前所有`actives`里程碑做一遍完整匹配，一场火拼可能一分钟几百次调用，属于"每帧/每次交互都要走一遍全量Milestone匹配"的性能陷阱。参照`JobMod::DailyPlan`/`ExecNode`注释里明确写的"这次调度机制完全是C++直接构造Change*，和milestone/JSON没有任何关系"这个先例，武器系统也要分两层：

- **高频路径（每次开火/每次命中）**：Forever层武器组件直接调`WeaponMod::Fire()`→碰撞查询→`ComputeDamage()`→**直接**修改目标（Player或Citizen）的生命值状态，走纯C++调用，不经过Story。
- **低频/叙事阈值路径（少数真正值得让剧情系统知道的时刻）**：只有下面这几类情况才触发Story（装备武器、开火动作本身都**不**广播任何Event，见第二节更正）：
  - 命中导致NPC死亡 → 广播已有的`CitizenDeceaseEvent`。
  - 命中/开火被目击、需要涨通缉值 → **直接调`Story::ApplyChange(&changeWantedChange, context)`**（不经过Milestone匹配，因为"该不该涨、涨多少"是武器系统C++自己算的规则，不是剧情作者要控制的事），改完之后**广播`WantedChangeEvent`**通知关心"通缉值变化"的milestone（比如警察JobMod挂的脚本）作出反应。

这样一场火拼里，Story系统只在"开始使用武器""死了人""通缉值变化"这几个真正的叙事节点上被触碰，其余全部是纯C++的高频路径。

## 六、发现的一个vocabulary小缺口

`PlayerArrestedEvent`/`PlayerReleasedEvent`只有Event，没有配对的Change——对比其余"玩家状态"类型全部是Change+Event成对出现（`PlayerInjuredChange`/`PlayerInjuredEvent`、`PlayerCuredChange`/`PlayerCuredEvent`、`PlayerIllChange`/`PlayerIllEvent`、`PlayerRecoverChange`/`PlayerRecoverEvent`），这一对不对称。建议按同样命名习惯补上`ArrestPlayerChange`/`ReleasePlayerChange`（纯数据类，字段留空或者一个可选的"关押时长"表达式字段，参照`change.h`里现成的写法风格），这样"警察抓到玩家"这个动作可以统一走"C++直接`ApplyChange`一个Change→再广播对应Event通知milestone"这套模式，和通缉值改动是同一套路数，不用为"逮捕"单开一条不一致的路径。

`Story::ApplyChange`目前的`dynamic_cast`分派只有`SetValueChange`分支真正执行，这次要用到的`ChangeWantedChange`、（如果补的话）`ArrestPlayerChange`都需要顺手在`Story::ApplyChange`里把对应分支从"未实现"日志改成真正执行——这个改动量很小，但要记得做，不然武器系统调了`ApplyChange`也是空转。

## 七、警察抓捕闭环——端到端链路

把前面几节拼起来，完整走一遍"开枪→通缉→追捕→反抗→逮捕"：

1. 玩家开火命中一个NPC（高频路径，纯C++，`WeaponMod::Fire`+`ComputeDamage`直接改目标状态）。
2. 武器系统C++判定"这次命中构成犯罪且被目击" → 直接调`Story::ApplyChange`执行`ChangeWantedChange`（改`system.`通缉值）→ 广播`WantedChangeEvent`。
3. 警察这个职业类型的`JobMod`子类，`scriptModName`+`milestoneNames`指向一份专属`.script`文件，里面的milestone监听`WantedChangeEvent`（条件比如`system.wanted_level >= 某阈值`），匹配后触发的`changes`用已有的`NPCNavigateChange`让警察NPC导航去玩家附近——**注意**：真正沿路追逐的驾驶/寻路行为本身不是milestone该管的（那是"特别复杂的逻辑"，按你最初定的原则该走C++），milestone这层只负责"决定要不要追、追去哪"，具体怎么追由警察`JobMod::ExecNode`的C++实现接手。
4. 警察`JobMod::ExecNode`在C++里做距离判定，抓到玩家时**直接**`new StartPuzzleChange("resist_arrest")`写进`this->changes`（完全复用`JobMod`已有的"直接构造Change*写进自己changes字段"机制，不需要新设计）。
5. 玩家在小游戏里的结果通过已有的`PuzzleResultEvent(result)`广播回来，milestone（警察脚本或主线脚本均可）匹配`local.result == "escaped"`/`"caught"`两个分支。
6. `caught`分支：执行（补齐后的）`ArrestPlayerChange`，触发对应`PlayerArrestedEvent`广播，任何关心"玩家被捕"的其他milestone（比如某条主线剧情）借此作出反应。

这条链路里，**剧情作者需要碰的只有第3步和第5-6步的json milestone**（监听哪个Event、通缉阈值多少、小游戏结果怎么分支），第1、2、4步全部是C++（武器判定、涨通缉值的规则、追逐/抓捕的具体逻辑）——正好对应你最初定的"剧情作者写json，复杂逻辑走C++ mod"这条顶层原则。

## 八、落地文件清单

跟着仓库现有的目录/命名约定：

```
Forever_UE/Source/Dependence/player/weapon_mod.h/.cpp/.md     # 新增，本文档第三节
Forever_UE/Source/Dependence/player/weapon_factory.h/.cpp     # 新增，本文档第四节
Forever_UE/Source/Basic/player/weapon_basic.h/.cpp            # 新增，默认纯数据实现
Forever_UE/Source/Dependence/story/change.h/.cpp              # 补ArrestPlayerChange/ReleasePlayerChange（可选，建议做）
Forever_UE/Source/Core/story/story.cpp                        # Story::ApplyChange补ChangeWantedChange(以及补上的Arrest类)执行分支
Forever_UE/Source/Forever/...                                  # UE侧武器组件/输入绑定（含新增的切枪InputAction）/碰撞查询——留给你在UE环境里根据Forever层现有的Character/Framework代码风格接
```

## 九、明确排除在这次计划外的东西

以下留给你在真实UE环境里处理，这份计划不覆盖：
- 具体的弹道/后坐力数值调优——不用我判断，等做出来你自己试了自然会调。
- 按第三节"MVP测试简化"里的链接下载武器模型、导入UE Content Browser——动画素材不需要（这次不做动画），导入流程你来做。
- 装备武器的专用UI和背包/弹药真实扣减——这次MVP明确跳过，直接数字键切枪+备弹无限，见第三节"MVP测试简化"，等你补完Asset架构再回来把`Reload()`换成真实扣减。
- `Fire()`/`ApplyRecoil()`的具体函数签名——需要对着Forever层实际的碰撞查询/相机API来定，这份计划只给了占位；我已经看过`ForeverCharacter.h`（有first/third person双摄像机、Enhanced Input），落地时会尽量对齐这套已有的写法，但最终签名还是要在真实编译环境里跑一遍才能确定没错。
- 可选的"武器俯仰角跟随摄像机pitch"这种低成本代替瞄准动画的技巧——不是必须的，想要更好的射击手感时再加，加不加都不影响其余设计。
- 编译报错的修复——这个环境没有UE编译器，代码只能靠我读代码保证内部一致，实际编译大概率会有一些小错误需要你在真实环境里改。

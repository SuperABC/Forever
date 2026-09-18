# populace.h / populace.cpp

## 进入populace域（本轮迁移概览）

这一轮迁移一共做了4件强相关的事，这份文档记录后两件（`Citizen`/`Populace`本身，见
`citizen.md`记`Citizen`）；前两件分别记在各自的文档里：

1. `Source/Basic/map/`下四个占位类改名：`ZoneBasic`→`ResidenceZone`、`BuildingBasic`→
   `ResidenceBuilding`、`RoomBasic`→`ResidenceRoom`、`ComponentBasic`→
   `ResidenceComponent`（纯改名，`ResidenceRoom`同时第一次填了`isResidential`/
   `residentialCapacity`真实数据）。见`Source/Basic/README.md`。
2. `Room`新增4类占位能力字段（residential/workspace/storage/manufacture）。见
   `Source/Core/map/room.md`。
3. `Citizen`类（本轮新建，见`citizen.md`）。
4. `Populace`类（本轮新建，本文档）+ `Map::ComputeAccommodationTarget()`/`Map::Checkin()`
   （见`Source/Core/map/map.md`"人口初始化对接"一节）+ Forever层的流式场景实例化（见
   `Source/Forever/Element/CitizenElement.md`/`Source/Forever/Framework/
   ForeverPopulaceFrameworkComponent.md`）。

## 职责

`Populace`：**和`Map`平级的顶层Core类，不是挂在`Map`底下的工具函数集合**——这是这次设计
唯一被用户明确纠正过的地方：`Populace`不知道`Map`的存在，自己管理`Citizen*`的生命周期，
`Map`只知道"有一个`Populace`可以读"，通过`Map::Checkin(const Populace&)`把外部传入的
citizens分配进自己的住宅room。两者都由`AForeverFrameworkActor`平级持有（`Map* map`旁边
`Populace* populace`），和老工程`GlobalBase`同时持有`map`/`populace`两个顶层对象、由它做
两者之间的编排是同一个分工：
```cpp
int accomodation = map->InitContents();      // 老工程
populace->Init(accomodation, player, implement);
map->Checkin(populace, player);
```
```cpp
populace = new Populace();                    // 这次
populace->Init(map->ComputeAccommodationTarget());
map->Checkin(*populace);
```

## 关键设计

### 年表模拟算法——完整照抄老工程

`Populace::Init(int accommodation)`先对`accommodation`做一次老工程同款的扰动
（`target = accommodation * exp(GetRandom(1000)/1000.0f - 0.5f)`，`expf`），再调私有的
`GenerateCitizens(target)`——完整照抄老工程`Populace::GenerateCitizens`
（`E:\Projects\Forever_UE\Source\Core\populace\populace.cpp:659-859`）的算法，逐行对照
过，包括几个容易出偏差的细节：

- **100男100女起始种子**：出生年`GetRandom(20)`（0~19），死亡年`birth+60+GetRandom(40)`
  （everyone都有），女性额外排一次婚姻年`birth+20+GetRandom(15)`。
- **`maleBirths`按出生年bucket-fill**：男性种子先按`birth`排序（`sort(males.begin()+1,
  males.end(), ...)`），再从`currentBirth+1`扫到`males[i].birth`逐年填
  `maleBirths[year]=i`——这样`maleBirths[year]`就是"出生年≤year的最后一个男性数组下标"，
  婚姻匹配靠这个数组按年龄窗口二分区间取随机候选。**这个排序只在初始100人这一步做一次**，
  后续`LIFE_BIRTH`新增的男性永远`push_back`在末尾，不会再触发重排——数组下标一旦分配就
  终身稳定，`chronology`里排队的死亡/生育事件引用的下标不会因为后续操作失效。
- **年表事件编码**：`chronology[year]`里每条`(eventIdx, LIFE_TYPE)`，`eventIdx>=0`表示
  女性数组下标，`eventIdx<0`表示`-男性数组下标`（真实下标从1开始，0留给哨兵，符号编码不
  会冲突）。
- **停止条件**：`year<100`无条件跑满，之后`year<4096`且"存活+死亡"总人数（`females.size()
  +males.size()`，不是只数活人）达到`target`前继续——**是"累计出生过的人数"而不是"当前
  活人数"**，这是容易读错的一点。
- **婚姻匹配**：`lowId=max(0,birth-10)`/`highId=max(0,birth+5)`是**出生年**，不是数组
  下标——`maleBirths[lowId]`/`maleBirths[highId]`才是数组下标区间，在这个区间里最多试10次
  随机候选，都是已婚就放弃（这次婚姻事件失败，这个女性永远不会再收到新的婚姻事件——和
  老工程一样，不重试）。
- **只把姓名/性别/生日+配偶/子女链接物化进`Citizen`**：父母/兄弟姐妹等其它血缘关系只在
  模拟用的内部临时结构（`populace.cpp`匿名namespace里的`Human`/`LIFE_TYPE`，不对外暴露）
  里用来推进人口数量，模拟结束后不保留（`Citizen`字段范围只到配偶/子女，见`citizen.md`）。
  配偶/子女之所以要保留，是因为`Map::Checkin()`需要靠它们判断"一家人要不要搬进同一间"——
  用户明确要求过这一点，见"不在这次范围内"一节曾经的错误简化历史。只有模拟结束时活着
  （`life != LIFE_DEAD`）且额外通过老工程同款95%存活过滤（`GetRandom(20) > 0`）的个体
  才会真正`new Citizen(...)`，物化时用`Human::idx`记下"这个人在`citizens`里的下标"
  （照抄老工程`Person::idx`的用法），再单独一趟"登记配偶/子女"的遍历把幸存双方的`idx`
  翻译成`Citizen*`互相关联——**只有配偶/子女双方都幸存物化才登记**，只有一方活下来的
  婚姻/亲子关系直接丢弃（本来也没有对应的`Citizen*`可指）。生日年份沿用老工程"2000+模拟
  内部年份"的换算习惯；模拟结束时的年份（`year+2000`）存进`currentYear`，通过
  `GetCurrentYear()`供`Map::Checkin()`给`Citizen::GetAge()`用。

### 姓名生成：真正的Name域（`NameMod`/`NameFactory`/`ChineseName`，第二版迁移）

第一版用了一个10姓+10男名+10女名的内置小词表占位，用户明确要求"不要这种方法，从老工程里
把name Concept迁移过来"——这次把`Source/Dependence/populace/name_mod.h`（`NameMod`接口）
和`Source/Basic/populace/name_chinese.h/.cpp`（具体实现，改名自阶段3占位骨架
`NameBasic`）从阶段3骨架换成老工程真正的取名算法/数据表。

`NameMod`接口照抄老工程`NameMod`（`E:\Projects\Forever_UE\Source\Dependence\populace\
name_mod.h`）的三个纯虚方法：`GetSurname(fullName)`/`GenerateName(male,female,neutral)`/
`GenerateName(surname,male,female,neutral)`，**但去掉了`std::function`回调+
`PostHandle*`参数**——那一套是给老工程"可能异步的UI/脚本触发"场景用的，这次唯一的调用方
`Populace::GenerateCitizens()`是纯同步调用，直接用返回值更简单，也不需要引入这个项目
目前完全没有的`PostHandle`/异步基础设施；生成失败（候选词库为空等）用空字符串表示，和
老工程`Name::GenerateName`回调传空字符串的失败信号语义一致。

`ChineseName`（`Source/Basic/populace/name_chinese.cpp`）逐字段/逐行照抄老工程
`ChineseName`（`E:\Projects\Forever_UE\Source\Basic\populace\name_basic.cpp`）：
- 姓氏表（200个单字，按常见程度从高到低排列）+男/女/中性给定名表（分别90/130/370个
  单字）全部硬编码`std::vector<std::string>`字面量，不走config/数据文件。
- 姓氏采样用"sqrt反索引"实现非均匀分布：`N=surnames.size()`，从`[0,N²)`均匀取
  `randVal`，`idx=floor(sqrt(randVal))`本身偏向大值，`surnameIdx=N-1-idx`把这个偏向
  反过来，让数组靠前（常见）的姓氏被抽中概率远高于靠后（罕见）的，不需要显式权重表。
- 给定名长度：`GetRandom(9)==0`时1个字（概率1/9），否则2个字（概率8/9），比单字名
  常见得多。每个字独立按`allowMale`/`allowFemale`/`allowNeutral`哪些词库被打开，从
  合并后的候选池里均匀抽取（一个2字名可以混合不同词库的字）；三个词库都关闭时兜底成
  只用中性词库。
- `GetSurname(fullName)`：手动解析UTF-8前导字节，截取姓名的第一个UTF-8字符作为姓——
  这个方案假定姓氏永远是单字（这份姓氏表里成立），不做姓氏表查找匹配。

`Populace`新增`InitNames()`（`Init()`一开始调用一次）：和`Map::InitZones/InitBuildings()`
同一个"用`ModLoader`发现/注册config.json配置的mod dll"写法，但**`Populace`自己独立持有
一份`ModLoader modLoader;`+`NameFactory nameFactory;`**（不和`Map`共用，两者互不知道
对方存在）：
```cpp
vector<string> mods = Config::GetMods();
nameFactory.SetModArgs(ToArgsMap(Config::GetConceptMods("name_mods")));
modLoader.RegisterConcept<NameFactory>(mods, "RegisterModNames", "FinishModNames", &nameFactory);
name = new Name(&nameFactory, "chinese"); // 固定用这个具体实现，见下
```
**固定用`"chinese"`这个id，不做通用的enable/disable选择**——这个项目的Factory模式本来
就没有老工程"config必须恰好enable一个name mod，否则`THROW_EXCEPTION`"这套机制（阶段3
约定是"config.json的`<concept>_mods`数组只提供按id的参数字符串，不做启用过滤，未列出的
id依然会被正常创建"），`Map::InitZones/InitBuildings`也是把所有已注册id一视同仁处理，
不存在"多个候选选一个"的问题。但Name这个concept**需要**唯一一个"当前生效"的取名算法给
`Populace`用，所以直接硬编码引用`ChineseName::GetId()`（`"chinese"`）——和
`Building::Layout()`直接用`ResidenceRoom::GetId()`而不是通用查找是同一种"直接耦合到
当前默认内容"的做法。`Populace`的析构函数`delete name`（`Name`析构里调用
`nameFactory.DestroyName(mod)`释放真正的`NameMod*`）。

**架构修正：新增`Core/populace/name.h/.cpp`的`Name`类，`Populace`不再直接持有/调用
`NameMod*`**——最初这一版迁移图省事，让`Populace`直接持有`NameMod* nameMod`并调
`nameMod->GenerateName(...)`，被指出这和`Terrain`（`Core/map/terrain.h`）、`Script`
（`Core/story/script.h`）等其它concept"上层聚合类只操作Core层包装类，不直接碰
`<Concept>Mod*`"的架构不一致后改正：新增`Name`类（`factory`+`mod`+缓存的`type`/`name`，
构造/析构/转发方法的写法逐字照抄`Terrain`），`Populace`现在只持有`Name* name`，通过它转发
`GetSurname`/`GenerateName`两个重载，见`name.md`。

**一处顺手修正的疑似bug**：老工程`Populace::GenerateCitizens`给最初100男100女种子生成
姓名时，两个性别都传`(male=false, female=true, neutral=true)`——男性种子也只从女性+
中性词库取名，读起来像是老工程自己的疏漏（和几十行之后`LIFE_BIRTH`事件"按孩子实际性别
选对应词库、中性词库永远允许"的写法不一致）。这次迁移时改成和`LIFE_BIRTH`一致的
"按性别选对应词库+中性词库永远允许"，不逐字复刻这个疑似bug，`populace.cpp`里有对应
注释说明。

**没有迁移老工程的`Name`包装类（`reserve`/`roll`去重）**：老工程`Name`（不是
`NameMod`）在具体算法之上加了一层"避免和预先脚本化的Story角色姓名撞名"（`reserve`）+
"避免同一轮生成内部撞名"（`roll`）的`unordered_set`去重。这次不迁移——Story/Script域
还没进这个项目，`reserve`完全用不上；`roll`的价值也因为姓名生成算法从10x10的占位小词表
换成了~200姓氏x最多590个给定名字符的真实词库而大幅降低（组合数量级足够大，几百个citizen
之间实际撞名的概率本来就很低），如果以后确实需要严格去重可以再补。

## 房产归属（`Map::Checkin`，进入populace域第四轮迁移）

用户要求"migrate老工程的园区/建筑/房间随机分配归属逻辑，并保留公有资产的逻辑"——这不是
`Populace`自己的职责（`Populace`只管人口模拟，不知道`Map`/`Zone`/`Building`/`Room`的
存在），实际代码在`Map::Checkin()`里，完整算法见`Source/Core/map/map.md`"人口初始化
对接"一节、字段设计见`Source/Core/map/room.md`/`zone.md`。这里只记录几个需要显式决策
（不是照抄就能定下来）的点：

- **"归属"和"住处"是两个独立概念，之前混在一起过一次**：第一版实现把"抽到住处的那个
  citizen"直接设成`room->owner`，被用户指出不对——一个room的owner应该由独立的"房产
  归属"流程决定，可能是从没在这里住过的人（房东），和"谁实际住在这里"（`tenants`/
  `occupants`）没有必然关系。修正后`Map::Checkin()`先跑完整的zone→building→room归属
  级联（对应老工程的部分，见下），再跑完全独立的住处分配循环，两者互不干扰。
- **"公有"（`stated`）这个结果这次是真正随机可达的，老工程原版不是**：核对过老工程
  `Map::Checkin`全部代码，"stated"分支的触发条件永远是`zone->GetStated()`/
  `building->GetStated()`这种"查询一个预先已经被设置好的标记"，但搜了整个老工程也没有
  任何mod/代码路径会在`Checkin`跑之前主动`SetStated(true)`——也就是说"公有"这条分支在
  老工程实际发布的内容里从来不会被触发，是个只有骨架、没有内容能激活它的死分支。用户
  明确要求"保留公有资产的逻辑"，为了让"公有"真的能在这个项目里出现，这次自己加了两个
  小概率（`kZoneStatedChance=2`、`kBuildingStatedChance=3`，和老工程`ZONE_OWNERSHIP_
  CHANCE=2`/`BUILDING_OWNERSHIP_CHANCE=5`同一个量级，满分`kProbabilityScale=100`）——
  **这两个具体数值是这次新定的，不是老工程的原始数据**，只是结构上照抄"整体统一归属"的
  三选一模式（公有/私有/下探到下一级），按需可以调整。
- **不照抄老工程对zone内部building的重复roll**：老工程`Map::Checkin`处理完
  `zones`（含级联到zone内部building/room的归属）之后，会再无条件遍历一次
  `Map::buildings`（这个map本来就包含所有building，含zone内部的）给每个building重新
  roll一次归属——如果一个zone刚被判定"整体私有/公有"，它内部的building几行代码之后就会
  被这第二轮独立roll出来的结果覆盖掉，级联下来的归属形同虚设。核对多遍确认这是老工程的
  疏漏（不是有意为之的"精细化覆盖"设计——"stated"分支专门有`if (building->GetStated())
  continue`保护、不会被覆盖，但"私有"分支没有对应保护，一个显然是漏写了），这次移植时
  显式跳过`GetParentZone()`非空的building，不逐字复刻这个疏漏。

## 依赖关系

- 依赖：`Source/Core/populace/citizen.h`（`GENDER_TYPE`/`Citizen`）、
  `Source/Core/populace/name.h`（`Name`——`Populace`只通过这层转发访问取名算法，不直接持有
  `NameMod*`，见`name.md`）、`Source/Dependence/populace/name_factory.h`（`NameFactory`，
  `Populace`自己持有一份、传给`Name`的构造函数）、`Source/Core/common/loader.h`
  （`ModLoader::RegisterConcept`）、`Source/Core/common/config.h`（`Config::GetMods`/
  `GetConceptMods`）、`Source/Dependence/common/utility.h`（`GetRandom`/`Time::DaysInMonth`）。
- 被谁依赖：`Source/Core/map/map.h/.cpp`（`Map::Checkin(const Populace&)`读
  `GetCitizens()`）、`Source/Forever/Framework/ForeverFrameworkActor.h/.cpp`（持有
  `Populace*`，`EnsureMapGenerated()`里`new`+`Init`+`Checkin`）、`Source/Forever/
  Framework/ForeverPopulaceFrameworkComponent.h/.cpp`（`GenerateCitizens(Map*,
  Populace*)`缓存`GetCitizens()`列表）。

## 不在这次范围内

- 老工程`Name`包装类的`reserve`/`roll`去重——见上"姓名生成"一节末尾。
- 除中文（`ChineseName`）外的其它取名算法（比如英文名）——`NameMod`接口已经是通用的，
  以后要加别的语言/风格直接新增一个具体实现+在`Populace::InitNames()`里按需切换
  `CreateName`的id即可。
- `Citizen`的父母/兄弟姐妹等亲属关系（配偶/子女除外，见上）、性格/交情、资产、职业/
  日程——见`citizen.md`。
- `Citizen`真正的AI行为（走动/工作/日程驱动的移动）——见`Source/Forever/Element/
  CitizenElement.md`。
- 老工程`Map::Checkin`房产归属分配时顺带创建`Asset`对象登记进`adults[index]->AddAsset(
  asset)`——这次不迁移，`Asset`/`Player::assetFactory`所在的Player/Asset域还没进这个
  项目，`Citizen`这次也没有资产列表字段（见`citizen.md`）。归属本身（`owner`/`stated`）
  完整迁移了，只是不再额外生成一份"资产"记录。

### 两处曾经的简化，已按用户反馈依次修正

1. 第一版实现时`Citizen`只有姓名/性别/生日，没有配偶/子女字段，`Map::Checkin()`因此
   简化成"一人一间随机分配"——用户指出"所有人都是独自在一个房间里"不对，老工程Checkin
   本来就是"一个家庭消费一个room名额"（配偶+未成年子女一起搬进同一间）。修正方案：
   `Citizen`补上`spouse`/`children`两个字段（见`citizen.md`），
   `Populace::GenerateCitizens`模拟结束后额外做一趟"登记配偶/子女"的遍历（见上），
   `Map::Checkin()`照抄老工程算法（成年citizen各自抽房间，配偶90%概率+未成年子女无条件
   跟着搬进同一间）。
2. 紧接着又把"抽到住处的那个citizen"直接设成`room->owner`——用户指出"归属"和"住处"是
   两个独立概念，进一步要求把老工程完整的zone→building→room归属级联算法迁移过来，
   `owner`应该由这套独立算法决定，和"谁住在这里"没有必然关系。修正方案见上"房产归属"
   一节。

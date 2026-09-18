# implement.h / implement.cpp

## 职责

`PostHandle`（`Dependence/common/handle.h`，定义在Dependence层供所有mod使用的"向Core发起
查询"抽象接口，见`Dependence/common/handle.md`）的**第一个具体实现**。`PostHandle`本身只是
一个骨架（`Post(request)`提交查询、`GetResult()`取回结果引用），在这次会话之前一直没有任何
代码真正实现或使用它——`PostImplement`补上这一块，复刻老工程`Core/common/implement.h`
"顶层门面"的形状：聚合Core全部7个domain（Map/Populace/Society/Story/Industry/Traffic/
Player）的裸指针，供Mod通过`Post()`反向查询Core状态。

这次唯一用到的场景：`ScriptMod::WrapScript`（`Dependence/story/script_mod.h`）遇到剧情
脚本里的`PlaceHolderChange`占位符时，需要向Core要一个"随机挑一个citizen"的姓名，构造出
`ChangeControlChange`去替换占位符——这条查询链路见`Dependence/story/script_mod.md`"典型
用法"一节、`Forever_Mod/Empty/Cpp/Empty/empty_mods.h`的`EmptyScript::WrapScript`。

## 关键设计

### 不复刻老工程"`GlobalBase`持有的全局单例"这一层

老工程`PostImplement`（或类似角色）依赖`GlobalBase`这个全局单例去拿7个domain的指针。这个
项目里`Map`/`Populace`/`Story`本来就不是单例（`Story`本身可以在任意地方单独`new`出来，见
`Core/story/story.md`），`PostImplement`延续同一个原则：不做成单例、不持有任何全局状态，
就是一个按需现场构造的普通对象，构造函数直接把7个指针传进来存好。谁来构造它、指针从哪里
来，`PostImplement`自己完全不关心——这次是`UForeverStoryFrameworkComponent::
BroadcastGameStart`在UE层从`AForeverFrameworkActor`身上取到7个指针后现场`new`（严格说是
栈上构造）一个，只活这一次广播的生命周期，见`Forever/Framework/
ForeverStoryFrameworkComponent.md`"PostImplement"一节。

### 只实现一种post类型，其余6个domain指针先占位

构造函数接收全部7个指针（`Map*`/`Populace*`/`Society*`/`Story*`/`Industry*`/`Traffic*`/
`Player*`）并存成成员，但`Post()`目前只识别一种请求：`request["post"] == "random citizen"`
——从`populace->GetCitizens()`（`std::vector<Citizen*>`）里`GetRandom(size)`随机挑一个，
成功返回`{"result":"success","name":"<姓名>"}`，`populace`为空或citizen列表为空返回
`{"result":"fail","msg":"no citizen available."}`；请求不认识（`post`字段不是`"random
citizen"`，或者请求本身不是一个JSON对象）统一返回`{"result":"fail","msg":"post not
found."}`。`society`/`story`/`industry`/`traffic`/`player`这五个指针（`map`同样暂时用不到）
这次构造出来传进去，但`Post()`里还完全用不上——等对应域真正迁移出业务逻辑、需要通过`Post`
查询它们的数据时，再在这个`if/else if`链上加新分支，不需要改构造函数签名。

### `GetResult()`返回成员的引用，不按值返回

```cpp
virtual const JsonValue& GetResult() const override { return result; }
```

`result`是`PostImplement`自己的成员（每次`Post()`调用开头`result = JsonValue(DATA_OBJECT)`
重置），`GetResult()`返回它的引用——这是`PostHandle`接口本身的约定（见`handle.md`），结果
对象的生命周期留在`PostImplement`这一侧管理，调用方（可能是mod DLL，比如这次的
`EmptyScript::WrapScript`）只读引用，不持有、不释放，避免跨模块传递返回值对象触发"由哪个
模块的分配器释放"的问题，和`[[memory:cross_dll_allocator_crash]]`是同一类考量。

## 依赖关系

- 依赖：`common/handle.h`（`PostHandle`基类）、`common/json.h`（`JsonValue`）、
  `populace/populace.h`/`populace/citizen.h`（`Populace::GetCitizens()`/`Citizen`，
  `Post()`里唯一真正用到的domain）、`common/utility.h`（`GetRandom`）、`Map`/`Society`/
  `Story`/`Industry`/`Traffic`/`Player`（仅前置声明，构造函数存指针，`Post()`里暂未使用）。
- 被谁依赖：`Forever/Framework/ForeverStoryFrameworkComponent.cpp`
  （`BroadcastGameStart()`现场构造`PostImplement`，传给`Story::BroadcastGameStart`的
  `post`参数）、`Dependence/story/script_mod.h`/`Forever_Mod/Empty/Cpp/Empty/
  empty_mods.h`（间接：`ScriptMod::WrapScript`拿到的`PostHandle*`在运行时实际指向一个
  `PostImplement`，但mod侧代码只认`PostHandle`接口，不直接引用`PostImplement`类型）。

## 待办/后续阶段

- `society`/`story`（`Post()`里目前不用，虽然构造函数收了）/`industry`/`traffic`/`player`
  五个domain指针目前只存不用，等对应域真正迁移出业务逻辑、需要通过`Post`反向查询时再加
  新的`else if`分支。
- 目前只有"random citizen"一种查询类型，请求/响应的JSON字段命名（`post`/`result`/`name`/
  `msg`）没有形成任何通用约定或校验，后续查询类型多起来后可能需要梳理一份统一的请求/响应
  格式规范。

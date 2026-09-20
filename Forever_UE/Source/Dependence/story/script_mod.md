# script_mod.h

阶段3骨架（`GetType`/`GetName`）之上，阶段4新增：

- `ScriptAction`类型别名：`using ScriptAction = std::variant<const Dialog*, const Change*>;`——
  `Script::MatchEvent`匹配命中后要执行的一个动作，`Dialog*`/`Change*`都是引用（本体挂在某个
  `Milestone`上，见`dialog.md`/`change.md`），这里绝不delete。
- `WrapScript`虚方法 + `actionStack`/`AutoCopy`/`AutoPop`：mod自定义改写`Script`匹配结果的
  入口（json表达不了的复杂剧情逻辑走这里，见用户"顶层原则"）。阶段4占位：默认实现只调
  `AutoCopy(actions)`原样透传，不做任何改写；真正按`PlaceHolderChange`标签替换成mod自己
  持有的Dialog/Change这套改写逻辑，留到被点名实现时再补。

## `WrapScript`补第四个参数`PostHandle* post`、新增非虚`FindLabel`——占位替换逻辑正式落地

上面"阶段4占位"一节写的"留到被点名实现时再补"这次补上了（`Forever_Mod/Empty`的
`EmptyScript::WrapScript`，见`Forever_Mod/Empty/Cpp/Empty/empty_mods.h`）：

- **`WrapScript`新增`PostHandle* post`参数**：mod侧改写`actionStack`顶层动作时，如果需要的
  内容不是mod自己能算出来的、必须问Core要（比如"随便挑一个citizen的姓名"），就通过这个
  参数向Core发起查询——`post->Post(request)`提交请求，`post->GetResult()`取结果引用，见
  `common/handle.md`。这个参数和`ScriptAction`一样遵循"引用/裸指针跨DLL传递安全，STL容器
  按值传递不安全"的约定：`Post()`的请求`JsonValue`由调用方（mod侧）在自己模块里构造+析构，
  `GetResult()`返回的结果`JsonValue`引用由提供查询的一侧（`PostImplement`，见
  `Core/common/implement.md`）管理，mod侧只读，不持有、不释放。这个改动是给`ScriptMod`新增
  虚方法参数、不是新增虚方法本身，不涉及vtable槽位变化，不需要重新走"新增虚方法前先确认过
  跨DLL兼容性"那套流程——但因为函数签名变了，`Empty`/`Wxdj`/`Test`三个mod仍然全部要重新
  编译（否则调用点传参数量对不上）。
- **新增非虚方法`FindLabel(label, context)`**：在`actionStack`当前层（`actionStack.back()`）
  里按标签字符串查找第一个匹配的`PlaceHolderChange`，返回它在这一层vector里的下标，找不到
  返回`-1`。`label`参数已经是纯字符串（不是`Expression`），但`PlaceHolderChange::GetLabel()`
  返回的是`Expression`（可能引用变量，不是纯字面量），所以要按传入的`context`先
  `EvaluateValue`+`ToString`求值出实际标签字符串，再和`label`比较。非虚——理由和
  `AutoCopy`/`AutoPop`一样：只会被已经虚分派到mod侧的`WrapScript`重载从内部调用，天然已经
  跑在mod自己的模块里，不需要单独走vtable。
- **典型用法**（`EmptyScript::WrapScript`）：`AutoCopy(actions)`之后，`FindLabel("control",
  context)`找`test.json`里`{"type":"place_holder","label":"control"}`这一项的下标，找到后
  `post->Post({"post":"random citizen"})`向`PostImplement`查询一个随机citizen姓名，把结果
  解析成`Expression`塞进mod自己长期持有的`ChangeControlChange controlChange`成员，最后
  `actionStack.back()[idx] = &controlChange`原地替换掉那个`PlaceHolderChange`——`
  controlChange`必须是mod自己长期持有（这里是`EmptyScript`的成员变量，永不delete），不能
  是`WrapScript`这次调用里现场`new`出来的临时对象，替换后的指针要在这次广播处理完之前一直
  有效。

## `WrapScript`不能按值返回`vector<ScriptAction>`——这是实测踩过的坑

`ScriptMod`是被`Forever_Mod`目录下几个预编译dll（`Empty`/`Wxdj`/`Test`）继承的接口，每个dll都
是独立链接的模块，各自有自己的CRT堆。第一版实现把`WrapScript`写成
`virtual std::vector<ScriptAction> WrapScript(...) { return actions; }`——这是个内联在头文件里
的虚函数默认体，`EmptyScript`不override它时，编译器会在`mod_empty.cpp`（Empty.dll自己的目标
文件）里生成这份默认实现的私有副本，用于填`EmptyScript`的vtable。这份副本执行`return actions`
时，**在Empty.dll自己的堆上**构造并返回了一个新`vector<ScriptAction>`；这个vector被
Core（`Script::WrapScript`，链接进Forever.dll）接住之后迟早要析构，析构时用的是Forever.dll的
CRT堆——两边堆不一致，直接触发`EXCEPTION_ACCESS_VIOLATION`。这不是理论推测，是实际起
PIE验证时复现的崩溃（崩溃点：`operator delete()`被`Story::BroadcastGameStart`间接调用，见git
历史）。和`[[memory:cross_dll_allocator_crash]]`是同一类问题的另一种触发方式：不是"mod调用
Core对象的STL容器mutator"，而是"mod的编译产物里默认实现构造的STL容器被Core析构"。

### 修复：actionStack模式（照抄老工程的设计，这次验证过它确实是为了绕开这个坑）

- `WrapScript`改成返回`void`，往`actionStack`（`ScriptMod`自己的`std::deque<vector<
  ScriptAction>>`成员）里`push_back`一层。
- `Core/story/script.h`的`Script::WrapScript`转调`mod->WrapScript(...)`后，返回
  `mod->actionStack.back()`的**引用**，不做拷贝。
- 调用方（`Script::MatchEvent`）拿到这个引用后，立刻拷贝进自己的（Core分配的）
  `vector<ScriptAction>`——这一步拷贝是安全的，因为`ScriptAction`只是两个裸指针的
  `variant`，拷贝值本身不涉及任何堆分配，不跨堆。拷贝完之后再调用`AutoPop()`
  （`Script::AutoPop`转调`mod->AutoPop()`）弹出这一层——**顺序不能反**，`AutoPop`一旦执行，
  前面那个引用就失效了。
- 这样一来，`actionStack`这个deque（以及它里面每个vector）从`push_back`到`pop_back`，整个
  生命周期里的每一次堆分配/释放都发生在**构造这个`ScriptMod`实例的那个模块**（`AutoCopy`/
  `AutoPop`都是通过虚函数或者mod自己的override天然跑在mod那一侧），Core自始至终只读一个引用、
  从不持有/析构mod那边的容器本体。

## 新增虚方法前先确认过跨DLL兼容性

`ScriptMod`新增虚方法会在vtable末尾插入新槽位——如果某个dll是用**旧版**`ScriptMod`头文件编译
的，它产出对象的vtable天然没有这个新槽位，Core这边按新头文件的槽位号去调用就会读到vtable数组
越界的脏内存。这次加`WrapScript`（以及后来改成`actionStack`版本）之前，都先验证/重新编译了
`Empty`/`Wxdj`/`Test`三个mod：

- `Empty`一开始因为一个和这次任务无关的既有问题（`EmptyName`没有实现`NameMod`后来新增的
  `GetSurname`/`GenerateName`两个纯虚接口）编译不过，顺手在`Forever_Mod/Empty/Cpp/Empty/
  empty_mods.h`补上了这几个方法（按接口约定的"失败"语义统一返回空字符串，`EmptyName`本来就
  不持有任何姓名词库）。
- 确认三个mod都能重新编译之后，才把`WrapScript`相关的方法加进`ScriptMod`，并且**每次改动
  `ScriptMod`的虚方法集合之后都重新编译了这三个dll**（不是只改头文件），保证它们产出对象的
  vtable和这份新头文件的槽位号一致。

以后再给`ScriptMod`新增/修改虚方法，都要重复这个步骤：先确认`Empty`/`Wxdj`/`Test`能编译，改完
头文件后把这三个dll也一起重新编译，不能只改`Dependence`这一层就当作完事；如果新增的方法涉及
跨DLL传递STL容器，参考上面"actionStack模式"，不要直接按值返回容器。

## 依赖关系

- 依赖：`expression.h`（`Expression`/`ScriptContext`）、`change.h`（`FindLabel`里
  `dynamic_cast<const PlaceHolderChange*>`）、`../common/handle.h`（`PostHandle`）、
  `<deque>`。
- 被谁依赖：`script_factory.h`（`ScriptFactory::CreateFunc`/`DestroyFunc`按`ScriptMod*`工作）、
  `Core/story/script.h`（`ScriptAction`、`Script::mod`、`Script::WrapScript`/`AutoPop`转调
  `mod->WrapScript`/`mod->AutoPop`）、`Forever_Mod/Empty/Cpp/Empty/empty_mods.h`
  （`EmptyScript::WrapScript`真正override，用`FindLabel`+`post`实现占位替换）。

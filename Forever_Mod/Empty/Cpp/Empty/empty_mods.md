# empty_mods.h / mod_empty.cpp

## 职责
外部Mod示例/占位DLL:20个concept各注册一个`Empty<Concept>`,id统一为`<concept>_empty`(如`room_empty`、`script_empty`)。不含玩法内容,用来验证`config.json`→`Config`→`ModLoader`→mod DLL这条加载与参数传递链路,同时是新写一个外部Mod时可抄的最小骨架。

## 关键设计
- `mod_empty.cpp`导出每个concept的`GetMod<Concept>s`/`RegisterMod<Concept>s`/`FinishMod<Concept>s`三件套,以及`GetModDependencies`(每个id都声明无依赖)。`Register`里只登记creator/deleter(以及该concept要求的静态函数指针),`Finish`为空。creator/deleter是DLL内的lambda,销毁走本DLL的`delete`,符合跨DLL内存安全约定。
- 参数走"创建实例时直接传给creator"这条唯一路径:`config.json`的`"<concept>_mods"`里`"empty --test true"`解析出的参数串,作为构造函数参数交给`EmptyXxx`,存进`name`成员,`GetName()`返回`"empty(<args>)"`,方便在日志里确认参数一路透传到了mod。
- `EmptyZone`/`EmptyBuilding`是"一个本体独占一个mod实例"模型:不接收参数串,用静态`count`在构造函数里生成唯一名字(`empty0`、`empty1`...),`Assign`为空实现(不参与显式占位),并实现`GetInstanceCounter`/`SetInstanceCounter`供存档保存/恢复该计数器(见`Source/Dependence/common/mod_state.md`)。`EmptyBuilding`的面积/权重静态方法全部返回0。
- 其余占位的纯虚方法一律空实现,按接口约定的"失败/无结果"语义处理:`EmptyName`不调用`setResult`;`EmptyOrganization::GetRequirements`返回空表,`Society::Init`跳过它;`EmptyProduct`/`EmptyStorage`/`EmptyManufacture`/`EmptyRoute`/`EmptyStation`/`EmptyVehicle`的字段保持基类默认值。`EmptyVehicle::GetCategory`返回占位种类`"empty"`,不会被任何停车位选中。
- `EmptyPuzzle`:`SetParam`记下剧情脚本`start_puzzle`传来的本次启动参数(与构造时的`config.json`参数不是一回事),`Loop`在左上角打印`param: ...`并永远返回`nullptr`(不结束),用来验证param在`Init`之前到达。`EmptyApp`四个回调全空。
- `EmptyScript::WrapScript`是唯一带真实逻辑的占位:`AutoCopy`后用`FindLabel("control")`找到`PlaceHolderChange{label:"control"}`(如`Resource/Story/test.json`里`game_start`那个milestone的最后一项),向Core Post查询`"random citizen"`,把姓名写进成员`controlChange`,再原地替换`actionStack.back()`里的占位。`controlChange`是mod自己长期持有的成员,不会被delete,符合"mod塞进actionStack的指针必须由mod长期维护"的约定(见`script_mod.md`)。

## 依赖关系
- 依赖:`Source/Dependence`全部20个concept的`<Concept>Mod`/`<Concept>Factory`头文件,链接`Dependence.lib`(`#pragma comment(lib)`);不依赖Core、不依赖UE。
- 不在`Framework.sln`里,Dependence公共头文件变动后需单独重编(见CONVENTIONS第9节)。

## 待办/后续阶段
- 无。

# TestScript(script_test)

## 职责
`Test.dll`里唯一的mod:`test.script`的主线ScriptMod。`WrapScript`把`game_start`里label为`control`的`PlaceHolderChange`换成"控制一个随机有家的市民"的`ChangeControlChange`,否则玩家停在飞行相机上。文件:`script_test.h`、`mod_test.cpp`(导出函数)。

## 关键设计
- 流程:`AutoCopy(actions)` -> `FindLabel("control", context)` -> 向`post`发`{"post":"random citizen"}` -> 成功时`controlChange.SetName(result["name"])`并把`actionStack.back()[controlIdx]`指向它。找不到label、`post`为空或请求失败都保持原样透传。
- `controlChange`必须是mod自己长期持有的成员:`actionStack`只存指针,遵守`script_mod.h`"mod往actionStack里塞的指针必须是mod自己长期维护"的约定(见`Dependence/story/script_mod.md`)。
- `GetName()`里现拼`test<id>`返回(`id(count++)`);计数器通过`GetInstanceCounter`/`SetInstanceCounter`参与存档。
- `GetModDependencies`为空,不依赖其他mod;creator忽略参数字符串。

## 依赖关系
- 依赖:`Dependence`的`script_mod`/`script_factory`/`common/json.h`。
- 被依赖:`Resource/test.script`与`config_test.json`。

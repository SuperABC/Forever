# mod_args.h / mod_args.cpp

## 职责
所有mod共用的参数串解析器`ModArgs`。`config.json`里`"<concept>_mods"`的条目写成`"id --key value --flag --key=value"`，工厂把`id`之后的参数串原样交给mod的creator(见仓库根`CONVENTIONS.md`"Mod参数传递")；mod在构造函数里`ModArgs args(argString);`，再`args.GetInt("length", 10)`之类按key取值。mod里不要再自己`istringstream`/`find("--xxx")`解析。

## 语法
- `--key value`、`--key=value`、`--flag`(没有值)三种写法。key不带`--`取。同key多次出现取最后一次。
- **值可以是多个token**：`--path C:/my maps/a.terrain`里的空格不用加引号，值一直吃到下一个`--key`(用单个空格拼起来)。也可以用`"..."`/`'...'`包住(`--path="C:/a b/x.terrain"`)，引号里的`--`不算key。
- 单个`-`开头的(`-5`、`-0.3`)是普通值，只有`--`后面至少一个字符才是key。
- 出现在第一个`--key`之前的散装token进`Positionals()`。
- `--seed N`不会到mod手里：Core在构造前用`ModArgs::Remove`剥掉了(`RandomRegistry::ExtractSeed`)，mod自己的参数里读不到。

## 取值规则
- `GetString/GetInt/GetFloat/GetDouble/GetBool(key, def)`：没写返回def。
- 数字必须整个值都能解析(`"12abc"`不行)，否则当没写返回def；带`min,max`的重载把结果夹到范围里(不需要自己再`max(…min(…))`)。
- `GetBool`：`false/0/no/off`(不分大小写)是false；`--flag`没有值或其它值是true。
- 错误参数一律静默退回默认值，不抛异常、不崩——参数是配置不是脚本，写错了该让mod按默认跑起来。

## 关键设计
- **静态库、各DLL各一份**：和`random`一样，Dependence静态链接进每个mod DLL，`ModArgs`对象只在mod自己的构造函数里局部使用，不跨DLL边界传递(里面是`std::string/vector`，跨DLL传会踩分配器)。
- `Remove(args, key, found, value)`：去掉`--key`及其值，其余token保持原样(含引号)、用单个空格拼接；没找到原串原样返回。只给Core剥`--seed`用。

## 依赖关系
只依赖标准库。使用方：`Basic`里的`roadnet_jing/checkboard/file`、`terrain_file/mountain/ocean/river`，`Core/common/registry.cpp`(经`RandomRegistry::ExtractSeed`)。

## 待办/后续阶段
- 无。新mod要加参数，直接在构造函数里用`ModArgs`读，并在mod自己的`.md`里写参数表。

## 保留参数

- `--seed N`：Core剥掉，mod读不到。
- `--skip`(zone/building mod)：`ZoneFactory/BuildingFactory::Assign`在工厂层读，mod本体不用处理。

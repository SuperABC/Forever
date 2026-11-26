# green_factory.h / green_factory.cpp

## 职责

`GreenFactory`是`GreenMod`(绿化区)的注册表/工厂：创建、销毁、枚举已启用的绿化类型，并转发每个类型的两个static权重函数。

## 关键设计

- 和`ZoneFactory`同一套最小注册表模式(全部公开方法`virtual`、`creator`/`deleter`裸函数指针、`DestroyGreen`走注册时的deleter、`SetModArgs`/`GetLiveInstances`)，理由见`zone_factory.md`。
- **两套权重**：`RegisterGreen(id, creator, deleter, powerFree, powerLinked)`多登记两个static函数指针：`powerFree`=没连路网的绿化区的权重，`powerLinked`=连了路网的。`GetPowerFree/GetPowerLinked(id)`不需要实例，id未注册/未启用返回0。`Map::InitGreens`对每块绿化多边形按"有没有连路"选一套，对所有启用的id做加权随机；全0则这块绿化区没有mod(不报错，空的)。
- 实例创建时`CreateGreen`之后由Core调`RandomRegistry::BindInstance`绑定每实例随机流(和其它概念一致)，Factory本身不碰随机。
- 配置键：`config.json`的`"green_mods"`(概念key`Greens`，`Config::ConceptKeyToJsonKey`去末字符再加`_mods`)；id可以是任意字符串，习惯上写成`green_<name>`。

## 依赖关系

- 依赖：`map/green_mod.h`。
- 被谁依赖：`Core/common/registry.h`持有唯一实例，`Core/map/map.h`(`Map::greenFactory`引用)、`Core/map/green.h`。

# terrain_mod.h / terrain_mod.cpp

## 职责

Terrain是阶段4-1（Map域）第一个把业务接口从"只有GetType/GetName"补齐成真正接口的concept，
按`Source/Dependence/README.md`自己的约定从共用文档独立出来。`TerrainMod`定义地形Mod必须实现
的完整业务接口，对照老工程`E:\Projects\Forever_UE\Source\Dependence\map\terrain_mod.h`迁移。

## 关键设计

- **`GetPriority()`决定生成顺序**——`Map::InitContents()`按这个值降序排列所有已注册地形，
  依次调用`DistributeTerrain`。数值越高越先跑，后跑的地形理论上可以覆盖先跑的（但具体覆盖
  策略由每个Mod自己在`DistributeTerrain`内部决定，接口本身不强制）。已知取值：Ocean=1.0f，
  Mountain=0.9f。
- **`SetupTexture()`和`DistributeTerrain()`分两步**——`SetupTexture`只填`diffusePath`/
  `waterHeight`两个数据字段，`DistributeTerrain`才是真正改地图的算法。这样`Map::InitContents`
  可以先统一调用一轮`SetupTexture`拿到所有地形的贴图路径去构建`terrainTextures`索引表，再按
  优先级顺序跑`DistributeTerrain`，两件事互不依赖执行顺序。
- **`DistributeTerrain`用4个`std::function`回调读写地图，不直接依赖`Map`/`Element`类型**——
  这是Dependence层一贯的做法（Mod DLL不知道也不需要知道`Map`的具体实现），调用方（`Map`）
  把自己的`GetTerrain`/`SetTerrain`/`GetHeight`/`SetHeight`包成闭包传进去。
- **去掉了老工程`ShapeFilter`工具方法**——3x3邻域多数投票平滑滤波，老工程声明在`TerrainMod`
  基类里但`OceanTerrain`/`MountainTerrain`都没用到，这次没有迁移。如果未来某个地形Mod确实
  需要这类平滑滤波，再按需加回，不用现在为了"完整对照老工程"而搬一个没人用的方法。
- **`diffusePath`/`waterHeight`是公开数据成员，不是私有+getter**——和老工程一致，`Terrain`
  包装类（`Source/Core/map/terrain.h`）直接读`mod->diffusePath`/`mod->waterHeight`，不通过
  虚函数转发，减少一层调用。

## 依赖关系

- 依赖：无（纯C++，不依赖Core/UE）。
- 被谁依赖：`Source/Core/map/terrain.h`（`Terrain`包装类持有`TerrainMod*`）、
  `Source/Basic/map/terrain_basic.h`（`OceanTerrain`/`MountainTerrain`）、
  `Forever_Mod/Empty`的`EmptyTerrain`demo mod。

## 待办/后续阶段

- 阶段4：如果后续Zone/Building/Roadnet等domain的Mod接口设计中出现了"某个地形Mod需要平滑
  滤波工具"的真实需求，把`ShapeFilter`加回来。

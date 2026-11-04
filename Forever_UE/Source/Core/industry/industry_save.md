# industry_save.h / industry_save.cpp

## 职责
工业域存档：`world.industry`段，保存所有会变化的状态——各product城内批发价、每个仓库库存、每个工坊
input/output cache库存和已开工未结算的`pendingProduction`。

## 关键设计
- **结构不存档，状态存档**：有哪些Storage/Manufacture、容量、取放权限、需求都由Room和mod决定，读档时
  `Industry::Init(map.GetAllComponents())`重新生成，再按名字（房间名/`房间名#mod id`）放回库存。
  房间名是全局唯一的命名计数器，随地图存档恢复，所以名字稳定。
- **读档不播种**：开局的"产出cache放半天产量"只在新开局调`SeedInitialStock()`，读档完全以存档为准。
- 世界里已不存在的仓库/工坊（mod变了）的库存直接丢弃。
- 货物流水`flows`是当天`Tick`的派生结果，不存。
- 段是必须存在的，没有工业段的存档拒绝读取（所有存档段都是必须的）。

## 依赖关系
- 依赖：`Industry`/`Storage`/`Manufacture`（友元访问私有库存）、`Map`（取Component）。
- 被谁依赖：`SaveGame::BuildArchive`，`AForeverFrameworkActor::EnsureIndustryGenerated`读档分支。

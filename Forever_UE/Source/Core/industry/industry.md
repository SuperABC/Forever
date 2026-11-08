# industry.h / industry.cpp

## 职责

`Industry`：工业系统域类，持有`Product`类型目录+`Storage`/`Manufacture`两张表，每天驱动
"工坊结算→全城物流→工坊开工"。**只管货物的生产、存放和流通，不涉及任何金额**：每笔货物流向
记在`GoodsFlow`流水里，由Society按流水付款收款（Step 3）。

## 关键设计

- **"工厂"叫"工坊"**：`XxxFactory`在本项目里是mod管理器，生产单位统一叫工坊（`Manufacture`）。
- **初始化**：`Init(components)`先按已启用的product mod建目录，再遍历Room：`Room::StorageType()`
  非空的生成一个Storage（名字=房间名），`Room::ManufactureTypes()`里每个manufacture mod id生成一个
  Manufacture（名字=`房间名#mod id`）。`GetStorageRoom()`/`GetManufactureRoom()`反查所属房间，
  Society据此找到付款的组织；工坊的两个cache也映射到工坊所在房间。
- **开局播种**：新开局`Init`之后调`SeedInitialStock()`，每个工坊的产出cache放半天产量（每个目标
  product：批数×批量÷2，取整）。读档不播种，库存完全以存档为准，见`industry_save.md`。
- **有序容器**：`products`/`storages`/`manufactures`都用`std::map`，物流先后顺序必须确定，同一份数据
  每次跑出同一个结果。
- **目录key=type**：`CreateProduct(modId)`创建后以`Product::GetType()`为key入目录，与`FindProduct(type)`
  同一个键，不要求mod的`GetId()`等于`GetType()`；两个mod声明同一type时后创建的被丢弃、返回先入表的那个。
- **Society每天同步进来的派生数据（不存档）**：`SetClosedRooms`（停业房间）、`SetStaffingRatios`
  （到岗率）、`SetExportKeepRatio`（出口阈值），都由`Economy::SyncIndustry`设置。
- `ApplyChange`是占位（Industry域目前没有Change子类），恒返回false。

## 每日流程：`Tick()`

跨天时：清空上一天的`flows`；A) 全体工坊`WorkAccount()`把上一轮产量结算进产出cache；
B) `Distribute()`物流；C) 全体工坊`StartProduce(产能比例)`按input cache存量和output cache剩余空间算今天产量。

**产能比例**=50%×(1+到岗率)，不设上限；到岗率=工坊所在房间开放岗位上的在岗人数/房间容量`WorkspaceCapacity()`（容量=满编），由`Economy::SyncIndustry`每天通过`SetStaffingRatios`同步(没登记的房间按到岗率1)。月结扩招会让岗位数超过容量（最多2倍），到岗率随之大于1，产能更高；缺员或裁员则低于1。比例大于1时原料需求和产出空间按比例放大，不够仍会压低。

## 物流：`Distribute()`

仓库按权限分三类：只能放（商店仓库、工坊input cache）=需求方；只能取（工坊output cache）=供给方；
可取可放=独立仓库（目前没有）。取货顺序：先只能取的，再可取可放的。

1. **需求侧**：先精确product需求（`productDemand`，目标=比例×容量，缺口=目标-现有存量，且不超过剩余
   空间），再服务类型需求（`serviceTypeDemand`，该服务类型下所有product存量合计与目标的差额）。
   `Fulfil()`把内部可用供给按统一满足比例分给各需求方（先取整，余数按小数部分大到小补），
   没被满足的部分**直接进口**；服务类型需求缺口进口时买该类型下进口价最低的product。
2. **供给侧**：内销取完之后，只有只能取的仓库里**超出容量阈值**（`exportKeepRatio`，每天由`Economy::SyncIndustry`按`1-1/(x+1)`算出，x=当前全部外部注资/开局注资，x=1时是50%）的部分才往外走（各货物按存量占比分摊这份超出量）：能放进其他可取可放仓库（`AcceptsCategory`）的先放，放不下的
   **出口**；不到阈值就不出口。进口不受限制，缺的货全部进口。

货物按整份流转（`Whole()`取整，+eps避免浮点误差少算一份）。

## 停业房间
`SetClosedRooms`：企业倒闭时它占用的房间停业——这些房间的仓库不参与物流（`Distribute`跳过）、工坊不开工
（阶段C跳过`StartProduce`，但阶段A的`WorkAccount`照常结算上一轮已开工的批次）。

## 货物流水：`GoodsFlow`

`{from, to, product, quantity}`，`from==nullptr`=进口，`to==nullptr`=出口，否则是城内流转。
流水每次`Tick`重建，Society在同一天内读取。

## 依赖关系

- 依赖：`Product`/`Storage`/`Manufacture`、`Room`（Init时读StorageType/ManufactureTypes）。
- 被谁依赖：`AForeverFrameworkActor::EnsureIndustryGenerated`（Init）、Society（`Settlement`读流水按批发价
  付款收款，`Economy::SyncIndustry`同步派生数据）、`PostImplement`（"product info"）、`IndustrySaveCodec`（友元）。

## 待办/后续阶段

- 工坊每天目标批数（mod里的`targets`）目前是占位值，远小于全城日需求，缺口靠进口补。

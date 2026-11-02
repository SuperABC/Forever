# save_refs.h

## 职责

**跨域存档引用的统一编码**：人口/社会/交通等域的存档都用这一套(只用各类的公开接口)，是头文件内联实现的`namespace SaveRefs`，没有`.cpp`。

## 关键设计

- **Room/Component**：编码成(所属Building名字, 在`building->GetRooms()`/`GetComponents()`里的下标)。地图段按存档原样恢复，读回来的下标和保存时一致；建筑名为空/下标无效/越界一律读回`nullptr`，不崩溃。
- **Time**：年月日时分秒毫秒逐个写成`I32`。
- **mod实例 <-> 注册id**：`LiveIds(factory)`从Factory的存活实例表(`GetLiveInstances`)建一张"实例 -> 注册id"的表，`LookupId`查表。要用Factory的注册id而不是`mod->GetType()`，因为两者不一定相等。
- **mod自己的状态**：`WriteModState`/`ReadModState`写/读"状态版本+JSON文本"(`JsonStateWriter`/`JsonStateReader`)，mod没有实现`SaveState`时是`"{}"`；读时文本解析失败返回false(调用方打警告，mod保持新建时的状态)，文本为空视为成功。
- 详见`save.md`"跨域引用"一节。

## 依赖关系

- 依赖：`save_archive.h`(`ByteWriter`/`ByteReader`)、`save.h`、`json.h`、`utility.h`(`Time`)、`map/map.h`/`building.h`/`room.h`/`component.h`。
- 被谁依赖：`populace`/`society`/`traffic`/`industry`/`player`各域的存档读写代码。

## 待办/后续阶段

- 无。

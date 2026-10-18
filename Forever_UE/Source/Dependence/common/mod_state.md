# mod_state.h

## 职责

mod 把自己的私有数据写进存档、读档时读回来的接口：`StateWriter` / `StateReader`。宿主（Core 的
`JsonStateWriter` / `JsonStateReader`，见 `Source/Core/common/save.h`）实现，mod 在 `SaveState` / `LoadState`
里调用。存档系统总说明见 `Source/Core/common/save.md`。

## 21 个 mod 基类末尾追加的存档虚函数

| 函数 | 默认 | 作用 |
|---|---|---|
| `SaveState(StateWriter&) const` | 不写 | mod 私有数据存档 |
| `LoadState(StateReader&)` | 不读 | mod 私有数据读档 |
| `GetStateVersion() const` | 0 | `SaveState` 输出格式的版本，改了格式就加一 |
| `GetInstanceCounter() const` | -1 | 这个 mod 类生成唯一实例名用的静态计数器 |
| `SetInstanceCounter(int)` | 不做事 | 读档后恢复计数器 |

这是存档需求（"每个 mod 可以有专属序列化函数"）明确要求的 mod 类签名改动；改动后 Basic 和
`Forever_Mod/{Empty,Test,Wxdj}` 都要重编。

## 关键设计

- **全部 virtual，跨 DLL 安全**：mod DLL 调用时执行的是宿主编译的那份实现，存储容器始终在宿主一侧分配和释放；
  字符串以 `const char*` 返回（指向宿主内存，只在下一次调用 Reader 前有效，mod 自己拷贝）。符合 CONVENTIONS.md §4。
- **键值式 + 嵌套**：在当前层级按 key 写/读，`BeginObject/BeginArray` 进入子层级；数组里的元素 key 传空串，
  读的时候按下标（`GetArraySize` / `EnterArrayElement` / `Get*At`）。
- **基类公共字段不走这里**：`BuildingMod::floors/footprint/decorations...` 这类基类上的公共声明字段由 Core 在
  包装类里直接快照；`SaveState` 只放 mod 自己的私有数据。
- **mod 状态版本**：写存档时宿主自动记下 `GetStateVersion()`，mod 不用自己写版本；读档时
  `StateReader::GetSavedVersion()` 给出当时的版本，mod 自己决定旧格式怎么读。存档里的版本比当前 mod 新时，
  加载前校验就拒绝（`SaveGame::ValidateModStateVersions`）。
- **命名计数器**：每个带 `static int count` 的 mod 类都实现 `Get/SetInstanceCounter` 直接读写自己的 `count`
  （Basic 和三个外部 mod 已全部实现）。存档按 `(concept, id)` 记下存活实例的计数器最大值；读档重建完全部实例
  后由 `SaveGame::RestoreInstanceCounters` 恢复（临时建一个实例 → Set → 销毁），这样读档后新建的实例不会和
  恢复出来的名字撞车。没实现这两个函数的第三方 mod 返回 -1、不恢复，万一撞名由
  `Map::AddZone/AddBuilding` 的重名检查兜底并打警告。

## 依赖关系

- 依赖：只有标准库。
- 被谁依赖：21 个 `*_mod.h` 基类；`Source/Core/common/save.*`。

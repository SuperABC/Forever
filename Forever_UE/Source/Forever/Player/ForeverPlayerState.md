# ForeverPlayerState

## 职责
玩家状态类,当前是空壳。

## 关键设计
- 提前建立独立的`AForeverPlayerState`子类(而非直接使用引擎默认`APlayerState`),为后续"玩家与NPC数据结构统一、任意NPC可被玩家操控"预留挂载点。

## 依赖关系
- 被`AForeverPlayerGameMode`的`PlayerStateClass`引用。

## 待办/后续阶段
- 加入与NPC共用的数据结构字段,支撑"任意NPC可被玩家操控"。

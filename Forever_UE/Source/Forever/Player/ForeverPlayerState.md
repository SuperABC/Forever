# ForeverPlayerState

## 职责
玩家状态类,当前是空壳。

## 关键设计
- 旧工程没有独立的PlayerState类(默认引擎`APlayerState`)。这里提前建一个空的`AForeverPlayerState`子类,是为阶段6"玩家与NPC数据结构统一、可切换被控制角色"预留挂载点,不是遗漏。

## 依赖关系
- 被`AForeverGameMode`的`PlayerStateClass`引用。

## 待办/后续阶段
- 阶段6:在此类里加入与NPC共用的数据结构字段,支撑"任意NPC可被玩家操控"。

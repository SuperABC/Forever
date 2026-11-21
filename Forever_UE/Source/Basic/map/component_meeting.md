# component_meeting.h / component_meeting.cpp

## 职责

`MeetingComponent`(`component_meeting`)：`room_meeting`所属的component，无字段无逻辑的trivial占位(会议室不需要组织)。和`component_residence`等同一种写法。

## 依赖关系

- 依赖：`Source/Dependence/map/component_mod.h`。配套`room_meeting`，被`building_residence_high.cpp`(twin布局)使用。

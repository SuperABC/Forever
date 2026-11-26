# room_meeting.h / room_meeting.cpp

## 职责

`MeetingRoom`(`room_meeting`)：党派会议室。`ResidenceHighBuilding`的twin布局里1、2楼的两个single房间用它(原来是住宅)，党派mod通过post `random room`(type=`room_meeting`)挑其中一间当党代会的会议室。

## 关键设计

- `SetProperty()`只配门：不设`isResidence`/`isWorkspace`等任何功能属性(不是住宅、没有容量和房租)，也不摆家具；门洞标签`home`用和住宅一样的读卡器门。
- 没有Organization/Job/房租逻辑；"开会"完全由Core的政治域(到场统计看`Room::GetOccupants()`)和市民脚本(导航到会议室)完成，房间本身没有行为。

## 依赖关系

- 依赖：`Source/Dependence/map/room_mod.h`。被`building_residence_high.cpp`(twin布局)使用，配套组件`component_meeting`。

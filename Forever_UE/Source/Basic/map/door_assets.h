#pragma once

#include "map/feature_spec.h"

// 所有mod门当前共用的测试门：Door_with_cardreader骨骼网格 + 读OpenProgress的动画蓝图，见Dependence/map/feature_spec.md"门"一节。
// 资产路径要和UE里实际存的位置一致，换门时只改这里。
constexpr const char* kDefaultDoorMesh = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/SKM_door_with_cardreader.SKM_door_with_cardreader";
constexpr const char* kDefaultDoorAnimClass = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/ABP_Door.ABP_Door_C";

inline void UseDefaultDoor(DoorSpec& spec) {
	spec.mesh = kDefaultDoorMesh;
	spec.animClass = kDefaultDoorAnimClass;
}

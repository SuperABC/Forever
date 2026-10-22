#pragma once

#include "CoreMinimal.h"

class Asset;
class Player;
class UForeverAssetFrameworkComponent;

// 背包的六类区域(肩/手/后背按槽位拆开，容器列表/房间列表/附近列表各算一个)。
enum class EInvZone : uint8 {
	LeftShoulder,
	RightShoulder,
	LeftHand,
	RightHand,
	Back,
	Container, // 容器列表：显示选中容器的内容
	Room,      // 房间列表：当前房间(parent==nullptr)或房间里进入的子容器
	Nearby,    // 附近列表：Player::worldAssets(parent==nullptr)或其中进入的子容器
};

// 拖拽的源：asset当前所在的位置。parent是它所在的容器——槽位、房间根、附近根为nullptr，
// 容器列表恒非空，房间/附近进入了子容器时为该子容器。
struct FInvSource {
	EInvZone zone = EInvZone::Container;
	Asset* asset = nullptr;
	Asset* parent = nullptr;
};

// 拖拽的目标：container只对Container(必填)和Room/Nearby(进入了子容器时填，否则放根)有意义。
struct FInvDest {
	EInvZone zone = EInvZone::Container;
	Asset* container = nullptr;
};

namespace InventoryTransfer {

	// 槽位区域对应的Player路径("left"等)，非槽位区域返回空。
	FString SlotPath(EInvZone zone);
	bool IsSlot(EInvZone zone);

	// 松手后才调用：重新校验源与目标规则，通过后再真正移动(带回滚)。返回true=已移动；
	// false且OutError为空=无操作(同区域拖拽)，OutError非空=规则不允许/放不下的原因。
	// all=true移动整个堆叠，false只移1个(堆叠>1时拆出一个新实例)；目标里有同type的非容器
	// 物品时合并进去。
	bool Transfer(Player* player, UForeverAssetFrameworkComponent* assetFramework, FVector pawnLocation,
		const FInvSource& source, const FInvDest& dest, bool all, FString& OutError);

}

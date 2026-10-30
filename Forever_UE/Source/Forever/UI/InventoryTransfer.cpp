#include "UI/InventoryTransfer.h"

#include "Framework/ForeverAssetFrameworkComponent.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverStoryFrameworkComponent.h"

#include "player/player.h"
#include "player/asset.h"
#include "map/room.h"

namespace {

	// asset是不是container的祖先(或就是它)：把容器拖进自己/自己的后代会成环。
	bool ContainsRecursive(const Asset* asset, const Asset* target) {
		if (asset == target) return true;
		for (const auto& [name, content] : asset->GetContents()) {
			if (ContainsRecursive(content, target)) return true;
		}
		return false;
	}

	bool IsAtSource(Player* player, const FInvSource& source) {
		Asset* asset = source.asset;
		if (InventoryTransfer::IsSlot(source.zone)) {
			return player->GetByPath(TCHAR_TO_UTF8(*InventoryTransfer::SlotPath(source.zone))) == asset;
		}
		if (source.parent) {
			const auto& contents = source.parent->GetContents();
			auto it = contents.find(asset->GetName());
			return it != contents.end() && it->second == asset;
		}
		if (source.zone == EInvZone::Room) {
			Room* room = player->GetCurrentRoom();
			if (!room) return false;
			auto it = room->GetAssets().find(asset->GetName());
			return it != room->GetAssets().end() && it->second == asset;
		}
		if (source.zone == EInvZone::Nearby) {
			auto it = player->GetWorldAssets().find(asset->GetName());
			return it != player->GetWorldAssets().end() && it->second.asset == asset;
		}
		return false;
	}

	// 目标规则，见InventoryWidget.h/计划里的表。slotAsset的占用也在这里查。
	bool CanPlace(Player* player, const FInvDest& dest, Asset* asset, FString& error) {
		switch (dest.zone) {
		case EInvZone::LeftShoulder:
		case EInvZone::RightShoulder:
			if (!asset->IsWeapon()) { error = TEXT("肩膀上只能放武器"); return false; }
			break;
		case EInvZone::LeftHand:
		case EInvZone::RightHand:
			if (asset->GetMobility() != AssetMobility::Object) { error = TEXT("这个物品拿不起来"); return false; }
			break;
		case EInvZone::Back:
			if (asset->GetMobility() != AssetMobility::Object || !asset->GetBackpack()) { error = TEXT("后背只能放背包"); return false; }
			break;
		case EInvZone::Container:
			if (!dest.container || !dest.container->IsContainer()) { error = TEXT("没有选中容器"); return false; }
			break;
		case EInvZone::Room:
			if (!dest.container && !player->GetCurrentRoom()) { error = TEXT("当前不在房间内"); return false; }
			break;
		case EInvZone::Nearby:
			break;
		}

		if (InventoryTransfer::IsSlot(dest.zone)) {
			if (player->GetByPath(TCHAR_TO_UTF8(*InventoryTransfer::SlotPath(dest.zone)))) { error = TEXT("该位置已有物品"); return false; }
		}
		if (dest.container && ContainsRecursive(asset, dest.container)) { error = TEXT("不能把容器放进它自己里面"); return false; }
		return true;
	}

	// 目标列表(容器内容/房间根/附近根)里同type且非容器的物品，用于合并堆叠。
	Asset* FindMergeTarget(Player* player, const FInvDest& dest, Asset* asset) {
		if (asset->IsContainer() || InventoryTransfer::IsSlot(dest.zone)) return nullptr;

		auto matches = [&](Asset* candidate) {
			return candidate && candidate != asset && !candidate->IsContainer() && candidate->GetType() == asset->GetType();
		};
		if (dest.container) {
			for (const auto& [name, content] : dest.container->GetContents()) if (matches(content)) return content;
		} else if (dest.zone == EInvZone::Room) {
			if (Room* room = player->GetCurrentRoom()) {
				for (const auto& [name, content] : room->GetAssets()) if (matches(content)) return content;
			}
		} else if (dest.zone == EInvZone::Nearby) {
			for (const auto& [name, entry] : player->GetWorldAssets()) if (matches(entry.asset)) return entry.asset;
		}
		return nullptr;
	}

	Player::WorldAssetTransform MakeTransform(FVector location) {
		Player::WorldAssetTransform transform;
		transform.x = location.X;
		transform.y = location.Y;
		transform.z = location.Z;
		return transform;
	}

	bool Place(Player* player, const FInvDest& dest, Asset* asset, FVector pawnLocation) {
		if (InventoryTransfer::IsSlot(dest.zone)) {
			return player->AddByPath(TCHAR_TO_UTF8(*InventoryTransfer::SlotPath(dest.zone)), asset);
		}
		if (dest.container) return dest.container->AddContent(asset);
		if (dest.zone == EInvZone::Room) {
			Room* room = player->GetCurrentRoom();
			if (!room) return false;
			room->AddAsset(asset);
			return true;
		}
		if (dest.zone == EInvZone::Nearby) return player->AddWorldAsset(asset, MakeTransform(pawnLocation));
		return false;
	}

	// 把asset从源位置摘下来(不delete)。nearbyEntry用来在回滚时原样放回附近根。
	bool Detach(Player* player, const FInvSource& source, Player::WorldAsset& nearbyEntry) {
		Asset* asset = source.asset;
		if (InventoryTransfer::IsSlot(source.zone)) {
			return player->RemoveByPath(TCHAR_TO_UTF8(*InventoryTransfer::SlotPath(source.zone))) == asset;
		}
		if (source.parent) return source.parent->RemoveContent(asset->GetName()) == asset;
		if (source.zone == EInvZone::Room) {
			Room* room = player->GetCurrentRoom();
			return room && room->RemoveAsset(asset->GetName()) == asset;
		}
		if (source.zone == EInvZone::Nearby) {
			auto it = player->GetWorldAssets().find(asset->GetName());
			if (it == player->GetWorldAssets().end()) return false;
			nearbyEntry = it->second;
			return player->RemoveWorldAsset(asset->GetName()) == asset;
		}
		return false;
	}

	// 回滚：把整个摘下来的asset放回源位置。
	void Reattach(Player* player, const FInvSource& source, const Player::WorldAsset& nearbyEntry) {
		Asset* asset = source.asset;
		if (InventoryTransfer::IsSlot(source.zone)) {
			player->AddByPath(TCHAR_TO_UTF8(*InventoryTransfer::SlotPath(source.zone)), asset);
		} else if (source.parent) {
			source.parent->AddContent(asset);
		} else if (source.zone == EInvZone::Room) {
			if (Room* room = player->GetCurrentRoom()) room->AddAsset(asset);
		} else if (source.zone == EInvZone::Nearby) {
			player->AddWorldAsset(asset, nearbyEntry.transform);
		}
	}

	// get_asset/lose_asset事件：地面区(房间/附近)->身上区(肩/手/背/容器)是获得(成功或失败)，身上区->地面区是
	// 失去(只会成功)，同类区之间不发。事件发给这件资产自己的脚本(sender传空)，容器会连里面的东西一起发，
	// 并在左上角显示，见UForeverStoryFrameworkComponent::NotifyAssetEvent。
	void NotifyTransfer(UForeverAssetFrameworkComponent* assetFramework, const FInvSource& source, const FInvDest& dest,
		Asset* asset, bool bSuccess) {
		AForeverFrameworkActor* framework = assetFramework ? Cast<AForeverFrameworkActor>(assetFramework->GetOwner()) : nullptr;
		UForeverStoryFrameworkComponent* story = framework ? framework->GetStoryFramework() : nullptr;
		if (!story || !asset) return;

		auto isGround = [](EInvZone zone) { return zone == EInvZone::Room || zone == EInvZone::Nearby; };
		auto isBody = [](EInvZone zone) { return InventoryTransfer::IsSlot(zone) || zone == EInvZone::Container; };
		if (isGround(source.zone) && isBody(dest.zone)) {
			story->NotifyAssetEvent(asset, /*bGet=*/true, bSuccess ? "success" : "fail", nullptr);
		}
		else if (isBody(source.zone) && isGround(dest.zone) && bSuccess) {
			story->NotifyAssetEvent(asset, /*bGet=*/false, "success", nullptr);
		}
	}

	// 源/目标是房间根或附近根的物品才有对应的世界Actor。
	bool HasWorldActor(EInvZone zone, Asset* container) {
		return !container && (zone == EInvZone::Room || zone == EInvZone::Nearby);
	}

} // namespace

namespace InventoryTransfer {

	bool IsSlot(EInvZone zone) {
		return zone == EInvZone::LeftShoulder || zone == EInvZone::RightShoulder ||
			zone == EInvZone::LeftHand || zone == EInvZone::RightHand || zone == EInvZone::Back;
	}

	FString SlotPath(EInvZone zone) {
		switch (zone) {
		case EInvZone::LeftShoulder: return TEXT("leftShoulder");
		case EInvZone::RightShoulder: return TEXT("rightShoulder");
		case EInvZone::LeftHand: return TEXT("left");
		case EInvZone::RightHand: return TEXT("right");
		case EInvZone::Back: return TEXT("back");
		default: return FString();
		}
	}

	bool Transfer(Player* player, UForeverAssetFrameworkComponent* assetFramework, FVector pawnLocation,
		const FInvSource& source, const FInvDest& dest, bool all, FString& OutError) {
		OutError.Empty();
		Asset* asset = source.asset;
		if (!player || !asset) return false;

		// 同一区域内拖拽视为无操作(同一个槽位、同一个列表/容器视图)。
		if (source.zone == dest.zone && (IsSlot(source.zone) || source.parent == dest.container)) return false;

		if (!IsAtSource(player, source)) { OutError = TEXT("物品已不在原位置"); return false; }
		if (!CanPlace(player, dest, asset, OutError)) { NotifyTransfer(assetFramework, source, dest, asset, false); return false; }

		int total = asset->GetCount();
		int amount = all ? total : FMath::Min(1, total);
		if (amount <= 0) return false;
		if ((dest.zone == EInvZone::LeftHand || dest.zone == EInvZone::RightHand) && amount != 1) {
			OutError = TEXT("手里只能拿1个物品(不按Ctrl拖拽可只拿1个)");
			NotifyTransfer(assetFramework, source, dest, asset, false);
			return false;
		}

		bool sourceHasActor = HasWorldActor(source.zone, source.parent);
		bool destHasActor = HasWorldActor(dest.zone, dest.container);

		// 目标里有同type的非容器物品：合并堆叠。
		if (Asset* target = FindMergeTarget(player, dest, asset)) {
			if (dest.container && asset->GetSize() * amount > dest.container->GetSpace()) {
				OutError = TEXT("容器空间不足");
				NotifyTransfer(assetFramework, source, dest, asset, false);
				return false;
			}
			target->SetCount(target->GetCount() + amount);
			// 合并进目标之前先发事件：整个堆叠合并时这件资产马上被销毁。
			NotifyTransfer(assetFramework, source, dest, asset, true);
			if (amount < total) {
				asset->SetCount(total - amount);
			} else {
				Player::WorldAsset ignored;
				Detach(player, source, ignored);
				if (sourceHasActor && assetFramework) assetFramework->DestroyWorldAsset(asset);
				player->DestroyAsset(asset);
			}
			return true;
		}

		Asset* moving = asset;
		bool whole = amount >= total;
		if (!whole) {
			moving = player->SplitAsset(asset, amount);
			if (!moving) { OutError = TEXT("拆分失败"); NotifyTransfer(assetFramework, source, dest, asset, false); return false; }
		}

		Player::WorldAsset nearbyEntry;
		if (whole && !Detach(player, source, nearbyEntry)) { OutError = TEXT("无法取出物品"); NotifyTransfer(assetFramework, source, dest, asset, false); return false; }

		if (!Place(player, dest, moving, pawnLocation)) {
			if (whole) {
				Reattach(player, source, nearbyEntry);
			} else {
				asset->SetCount(asset->GetCount() + amount);
				player->DestroyAsset(moving);
			}
			OutError = TEXT("放不下(位置已被占用或空间不足)");
			NotifyTransfer(assetFramework, source, dest, asset, false);
			return false;
		}

		if (whole && sourceHasActor && assetFramework) assetFramework->DestroyWorldAsset(asset);
		if (destHasActor && assetFramework) assetFramework->SpawnWorldAsset(moving, pawnLocation);
		NotifyTransfer(assetFramework, source, dest, moving, true);
		return true;
	}

}

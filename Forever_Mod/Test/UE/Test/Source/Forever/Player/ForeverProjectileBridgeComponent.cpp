#include "Player/ForeverProjectileBridgeComponent.h"

FForeverProjectileHitHandler& UForeverProjectileBridgeComponent::HitHandler() {
	static FForeverProjectileHitHandler handler;
	return handler;
}

void UForeverProjectileBridgeComponent::ReportHit(FVector Hit, FVector Normal, FString Sufferer) {
	// 没有所有者脚本(没人绑定处理者，或键为空)时什么都不发生：弹射物蓝图自己的表现照常。
	FForeverProjectileHitHandler& handler = HitHandler();
	if (OwnerScriptKey.IsEmpty() || !handler.IsBound()) return;
	handler.Execute(OwnerScriptKey, Hit, Normal, Sufferer);
}

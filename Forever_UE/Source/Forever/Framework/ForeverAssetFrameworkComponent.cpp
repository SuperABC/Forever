#include "Framework/ForeverAssetFrameworkComponent.h"

#include "Element/AssetElement.h"

AAssetElement* UForeverAssetFrameworkComponent::SpawnWorldAsset(Asset* asset, FVector location) {
	if (!asset || !GetWorld()) return nullptr;

	AAssetElement* element = GetWorld()->SpawnActor<AAssetElement>(location, FRotator::ZeroRotator);
	if (!element) return nullptr;

	element->Init(asset);
	worldAssetActors.Add(asset, element);
	return element;
}

void UForeverAssetFrameworkComponent::DestroyWorldAsset(Asset* asset) {
	if (!asset) return;

	if (AAssetElement** found = worldAssetActors.Find(asset)) {
		if (*found) (*found)->Destroy();
		worldAssetActors.Remove(asset);
	}
}

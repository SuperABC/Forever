#pragma once

#include "CoreMinimal.h"

class UPrimitiveComponent;

// 家具的阻挡碰撞，设计见FurnitureCollision.md。
// 家具网格(Tools/Furniture/make_furniture.py生成)本身不带碰撞，阻挡用若干个盒子拼：按家具种类把网格包围盒
// 拆成几块(床=床头板+床身，沙发=靠背+坐垫+两个扶手……)，这样人能走到床边/坐垫前，相机也不会穿进靠背。
// 房间家具(ABuildingElement::BuildRoomFurniture)和固定物品的场景实体(AAssetElement)共用这一份。
namespace FurnitureCollision {
	// 整件家具(乘完缩放)矮于这个高度(cm)就不挡路：地毯这类薄片。由调用方按整件的包围盒判断。
	constexpr double kMinBlockingHeight = 12.0;

	// meshKey：网格资产路径或名字(只看最后一个'/'之后的部分)。boundsMin/boundsMax：网格局部系的包围盒(cm)。
	// outBoxes：网格局部系里的若干盒子；不认识的家具给一个等于包围盒的盒子。
	void GetBoxes(const FString& meshKey, const FVector& boundsMin, const FVector& boundsMax, TArray<FBox>& outBoxes);

	// 阻挡盒的碰撞设置：WorldStatic，只挡人(Pawn)、物理物体(PhysicsBody)、射线(Visibility)和相机(Camera)，其余忽略。
	void ApplyBlockingResponse(UPrimitiveComponent* component);
}

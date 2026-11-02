#pragma once

#include "CoreMinimal.h"
#include "Components/DecalComponent.h"

#include "LampDecalComponent.generated.h"

// 路灯的光斑Decal：远处用它代替真实光源。是可选组件——哪个灯蓝图需要就添加一个(方向/位置/大小用
// 组件自己的Transform和DecalSize，路灯朝下，墙灯可以侧向，一盏灯可以加多个)。显示/隐藏由
// ALampElement::RefreshLampEffects统一驱动，蓝图里不用写节点。详见LampDecalComponent.md。
UCLASS(ClassGroup = (Lamp), meta = (BlueprintSpawnableComponent))
class FOREVER_API ULampDecalComponent : public UDecalComponent
{
	GENERATED_BODY()

public:
	// 由ALampElement调用：active=true时显示并把亮度写进材质参数，false时隐藏。
	void SetLampActive(bool active, float brightness);

protected:
	// 光斑材质里表示强度的标量参数名。
	UPROPERTY(EditAnywhere, Category = "Lamp")
	FName intensityParameterName = TEXT("Brightness");

	// 写进材质的强度 = intensityScale * 亮度(0/1)。
	UPROPERTY(EditAnywhere, Category = "Lamp")
	float intensityScale = 1.f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> dynamicMaterial;
};

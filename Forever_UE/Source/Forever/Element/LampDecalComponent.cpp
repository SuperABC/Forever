#include "Element/LampDecalComponent.h"

#include "Materials/MaterialInstanceDynamic.h"

void ULampDecalComponent::SetLampActive(bool active, float brightness) {
	SetVisibility(active);
	if (!active) return;

	// 动态材质实例懒创建：只有真正要显示时才需要，远处/白天的灯不付这份开销。
	if (!dynamicMaterial) {
		dynamicMaterial = CreateDynamicMaterialInstance();
		if (!dynamicMaterial) return; // 蓝图里没选光斑材质
	}
	dynamicMaterial->SetScalarParameterValue(intensityParameterName, intensityScale * brightness);
}

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"

// 配置界面几个ScrollBox(DllList/ConceptList/ModList/ResourceList/ScriptList)的item之间
// 统一留白——在这一个地方设置,而不是让每个Row蓝图(WBP_PathRow/WBP_ConceptButton/
// WBP_ModCheckRow)自己在根节点上加Margin,以后要调整间距只改这一处常量。
namespace ForeverScrollBoxUtils {
	constexpr float kItemGap = 4.f;

	inline void AddChildWithGap(UScrollBox* list, UWidget* child) {
		if (!list || !child) {
			return;
		}
		if (UScrollBoxSlot* slot = Cast<UScrollBoxSlot>(list->AddChild(child))) {
			slot->SetPadding(FMargin(0.f, 0.f, 0.f, kItemGap));
		}
	}
}

#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "Styling/SlateTypes.h"

// 背包里的按钮(槽位按钮、列表行里的Button)都被设成HitTestInvisible，好让鼠标事件落到面板/行
// 自己身上做拖拽检测，代价是UButton自带的悬停/按下样式不再生效。这里手动复现：按状态把样式的
// Normal换成它自己样式里的Hovered/Pressed(含对应的Padding)，效果和默认响应一致，
// 用的是WBP里配置的样式，不是写死的颜色。
struct FInvButtonStyleState
{
	enum class EState { Normal, Hovered, Pressed };

	// 在把按钮设成HitTestInvisible之前、也是第一次Apply之前调用，记下WBP里配置的原始样式。
	void Capture(UButton* button) {
		if (!button || bCaptured) return;
		original = button->GetStyle();
		bCaptured = true;
	}

	void Apply(UButton* button, EState state) {
		if (!button || !bCaptured) return;
		FButtonStyle style = original;
		if (state == EState::Hovered) {
			style.Normal = original.Hovered;
		} else if (state == EState::Pressed) {
			style.Normal = original.Pressed;
			style.NormalPadding = original.PressedPadding;
		}
		button->SetStyle(style);
	}

private:
	FButtonStyle original;
	bool bCaptured = false;
};

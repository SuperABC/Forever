#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"


// FKey<->Canvas键码/鼠标按钮的翻译表——PuzzleWidget/PhoneWidget都要把UE的键鼠事件转发进
// Core/player/canvas.h的Canvas对象，这两个Widget需要完全一样的一份映射，之前只在
// UPuzzleWidget里私有实现，这次给PhoneWidget落地时提出来共享，避免复制一份一样的表。
namespace CanvasInputTranslation {
	// 精确对应KEY_LEFT/KEY_UP等常量和字母/数字的ASCII直通，不是老工程CanvasBuffer::FKeyCode
	// 那种任意哈希值。查不到的键返回0（Canvas::PushKey会忽略0）。
	int32 TranslateKey(const FKey& key);

	// FKey鼠标按键转成MOUSE_LEFT/RIGHT/MIDDLE，查不到返回-1。
	int32 TranslateMouseButton(const FKey& key);
}

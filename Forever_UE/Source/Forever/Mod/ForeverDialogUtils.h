#pragma once

#include "CoreMinimal.h"

// 系统文件对话框的小工具，配置界面(ForeverConfigBridgeSubsystem)和存档(ForeverSaveSubsystem)共用。
// 见ForeverDialogUtils.md。
namespace ForeverDialog {
	// 对话框的父窗口句柄——没有活跃的顶层窗口(比如纯命令行/自动化测试环境)时返回nullptr，
	// IDesktopPlatform自己能处理这个情况。
	const void* GetParentWindowHandle();

	// 打开文件对话框，选中一个文件返回true。fileFilter格式如"Forever Save (*.fsav)|*.fsav"。
	bool OpenFile(const FString& title, const FString& fileFilter, FString& outPath);

	// 保存文件对话框，选中/输入一个文件名返回true(不检查文件是否已存在)。
	bool SaveFile(const FString& title, const FString& fileFilter, const FString& defaultFileName, FString& outPath);
}

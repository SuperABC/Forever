#include "Mod/ForeverDialogUtils.h"

#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"

const void* ForeverDialog::GetParentWindowHandle() {
	if (FSlateApplication::IsInitialized()) {
		TSharedPtr<SWindow> activeWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
		if (activeWindow.IsValid() && activeWindow->GetNativeWindow().IsValid()) {
			return activeWindow->GetNativeWindow()->GetOSWindowHandle();
		}
	}
	return nullptr;
}

bool ForeverDialog::OpenFile(const FString& title, const FString& fileFilter, FString& outPath) {
	IDesktopPlatform* desktopPlatform = FDesktopPlatformModule::Get();
	if (!desktopPlatform) return false;

	TArray<FString> selected;
	bool success = desktopPlatform->OpenFileDialog(
		GetParentWindowHandle(), title, TEXT(""), TEXT(""), fileFilter, 0, selected);
	if (success && selected.Num() > 0) {
		// 对话框返回的可能是相对引擎Binaries目录的路径，统一转成绝对路径。
		outPath = FPaths::ConvertRelativePathToFull(selected[0]);
		return true;
	}
	return false;
}

bool ForeverDialog::SaveFile(const FString& title, const FString& fileFilter, const FString& defaultFileName, FString& outPath) {
	IDesktopPlatform* desktopPlatform = FDesktopPlatformModule::Get();
	if (!desktopPlatform) return false;

	TArray<FString> selected;
	bool success = desktopPlatform->SaveFileDialog(
		GetParentWindowHandle(), title, TEXT(""), defaultFileName, fileFilter, 0, selected);
	if (success && selected.Num() > 0) {
		outPath = FPaths::ConvertRelativePathToFull(selected[0]);
		return true;
	}
	return false;
}

#include "ForeverConfigBridgeSubsystem.h"

#include "ForeverModSubsystem.h"
#include "ForeverDialogUtils.h"
#include "UI/ForeverFadeSubsystem.h"

#include "common/config.h"
#include "story/script.h"

#include "Misc/Paths.h"
#include "Kismet/GameplayStatics.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"

namespace {

	// 配置界面所有Config读写都走这同一个固定路径——和UForeverModSubsystem::Initialize()
	// 读config.json用的是同一个路径，Start Game校验通过时写回这里。
	FString GetConfigPath() {
		return FPaths::Combine(FPaths::ProjectDir(), TEXT("Resource/Config/config.json"));
	}

} // namespace

TMap<FString, TArray<FString>> UForeverConfigBridgeSubsystem::ComputeRequiredModIds() const {
	TMap<FString, TArray<FString>> required;

	// 全部Story screen脚本的mod_dependences——无条件计入，不看这份脚本自己有没有被启用
	// (Story screen里的脚本本来就是"要加载"的，没有单独的启用/禁用开关)。
	for (const std::string& scriptPath : Config::GetStoryScripts()) {
		FString dependent = FString::Printf(TEXT("剧情脚本 %s"), *FPaths::GetCleanFilename(UTF8_TO_TCHAR(scriptPath.c_str())));
		for (const std::string& id : Script::GetModDependences(scriptPath)) {
			required.FindOrAdd(UTF8_TO_TCHAR(id.c_str())).AddUnique(dependent);
		}
	}

	// 每个真正被启用的mod id自己声明的依赖——按modId粒度，不会因为同一个DLL里另一个不
	// 相关的mod被启用而被迫多算一堆依赖，见config.h GetModDependences()注释。
	for (const Config::ModEntry& entry : Config::GetModEnables()) {
		if (!entry.enabled) continue;
		FString dependent = UTF8_TO_TCHAR(entry.modId.c_str());
		for (const std::string& id : Config::GetModDependences(entry.modId)) {
			required.FindOrAdd(UTF8_TO_TCHAR(id.c_str())).AddUnique(dependent);
		}
	}

	return required;
}

TArray<FString> UForeverConfigBridgeSubsystem::GetDllPaths() const {
	TArray<FString> result;
	for (const std::string& path : Config::GetDllPaths()) {
		result.Add(UTF8_TO_TCHAR(path.c_str()));
	}
	return result;
}

TArray<FForeverModRow> UForeverConfigBridgeSubsystem::GetModRows() const {
	TArray<FForeverModRow> rows;
	TMap<FString, TArray<FString>> required = ComputeRequiredModIds();

	for (const Config::ModEntry& entry : Config::GetModEnables()) {
		FForeverModRow row;
		row.DllPath = UTF8_TO_TCHAR(entry.dllPath.c_str());
		row.ConceptKey = UTF8_TO_TCHAR(entry.conceptKey.c_str());
		row.ModId = UTF8_TO_TCHAR(entry.modId.c_str());
		row.bEnabled = entry.enabled;
		if (const TArray<FString>* dependents = required.Find(row.ModId)) {
			row.bLocked = true;
			row.Dependents = *dependents;
		}
		rows.Add(row);
	}

	return rows;
}

void UForeverConfigBridgeSubsystem::AddDllPath(const FString& Path) {
	Config::AddDllPath(TCHAR_TO_UTF8(*Path));
}

void UForeverConfigBridgeSubsystem::RemoveDllPath(const FString& Path) {
	Config::RemoveDllPath(TCHAR_TO_UTF8(*Path));
}

void UForeverConfigBridgeSubsystem::SetModEnabled(const FString& ConceptKey, const FString& ModId, bool bEnabled) {
	Config::SetModEnabled(TCHAR_TO_UTF8(*ConceptKey), TCHAR_TO_UTF8(*ModId), bEnabled);
}

bool UForeverConfigBridgeSubsystem::IsModLocked(const FString& ConceptKey, const FString& ModId) const {
	// 锁定判定不看concept，只认mod id字符串，和Registry::CheckModRegistered同一套匹配
	// 规则——ConceptKey这个参数只是为了和GetModRows()的FForeverModRow对称，调用方传什么
	// 都不影响结果。
	(void)ConceptKey;
	return ComputeRequiredModIds().Contains(ModId);
}

TArray<FString> UForeverConfigBridgeSubsystem::GetResourceRootPaths() const {
	TArray<FString> result;
	for (const std::string& path : Config::GetResourceRootPaths()) {
		result.Add(UTF8_TO_TCHAR(path.c_str()));
	}
	return result;
}

void UForeverConfigBridgeSubsystem::AddResourcePath(const FString& Path) {
	Config::AddResourcePath(TCHAR_TO_UTF8(*Path));
}

void UForeverConfigBridgeSubsystem::RemoveResourcePath(const FString& Path) {
	Config::RemoveResourcePath(TCHAR_TO_UTF8(*Path));
}

TArray<FString> UForeverConfigBridgeSubsystem::GetStoryScripts() const {
	TArray<FString> result;
	for (const std::string& path : Config::GetStoryScripts()) {
		result.Add(UTF8_TO_TCHAR(path.c_str()));
	}
	return result;
}

void UForeverConfigBridgeSubsystem::AddStoryScript(const FString& Path) {
	Config::AddStoryScript(TCHAR_TO_UTF8(*Path));
}

void UForeverConfigBridgeSubsystem::RemoveStoryScript(const FString& Path) {
	Config::RemoveStoryScript(TCHAR_TO_UTF8(*Path));
}

bool UForeverConfigBridgeSubsystem::ValidateAndStartGame(FString& OutFailureMessage) {
	TArray<FString> failures;

	// 刷新缓存的.layout/.script文件列表，让运行中往资源目录里增删的文件在这次开局生效。
	Config::RescanFiles();

	for (const std::string& scriptPath : Config::GetStoryScripts()) {
		for (const std::string& id : Script::GetModDependences(scriptPath)) {
			if (!Config::IsModIdEnabled(id)) {
				failures.Add(FString::Printf(TEXT("剧情脚本 %s 依赖的mod \"%s\" 未启用"),
					UTF8_TO_TCHAR(scriptPath.c_str()), UTF8_TO_TCHAR(id.c_str())));
			}
		}
	}

	for (const Config::ModEntry& entry : Config::GetModEnables()) {
		if (!entry.enabled) continue;
		for (const std::string& id : Config::GetModDependences(entry.modId)) {
			if (!Config::IsModIdEnabled(id)) {
				failures.Add(FString::Printf(TEXT("Mod \"%s\" 依赖的mod \"%s\" 未启用"),
					UTF8_TO_TCHAR(entry.modId.c_str()), UTF8_TO_TCHAR(id.c_str())));
			}
		}
	}

	if (failures.Num() > 0) {
		OutFailureMessage = FString::Join(failures, TEXT("\n"));
		return false;
	}

	// 全部校验通过才写回config.json——只在这一刻保存一次，中途编辑不自动保存。
	Config::WriteConfig(TCHAR_TO_UTF8(*GetConfigPath()));

	UGameInstance* gameInstance = GetGameInstance();
	if (gameInstance) {
		if (UForeverModSubsystem* modSubsystem = gameInstance->GetSubsystem<UForeverModSubsystem>()) {
			modSubsystem->EnsureModsRegistered();
		}
		// 先变黑再进World(OpenLevel要同步加载很久，期间画面不刷新)，见UForeverFadeSubsystem。
		if (UForeverFadeSubsystem* fade = gameInstance->GetSubsystem<UForeverFadeSubsystem>()) {
			fade->OpenWorldLevel();
		}
		else {
			UGameplayStatics::OpenLevel(gameInstance, FName(TEXT("/Game/Levels/World.World")));
		}
	}

	return true;
}

bool UForeverConfigBridgeSubsystem::SaveConfigWithDialog(FString& OutError) {
	FString path;
	if (!ForeverDialog::SaveFile(TEXT("保存配置"), TEXT("JSON (*.json)|*.json"), TEXT("config.json"), path)) {
		return false;
	}
	Config::WriteConfig(TCHAR_TO_UTF8(*path));
	return true;
}

bool UForeverConfigBridgeSubsystem::LoadConfigWithDialog(FString& OutError) {
	FString path;
	if (!ForeverDialog::OpenFile(TEXT("读取配置"), TEXT("JSON (*.json)|*.json"), path)) {
		return false;
	}

	UGameInstance* gameInstance = GetGameInstance();
	UForeverModSubsystem* modSubsystem = gameInstance ? gameInstance->GetSubsystem<UForeverModSubsystem>() : nullptr;
	if (!modSubsystem || !modSubsystem->ReloadConfigFrom(path)) {
		OutError = TEXT("配置文件无效或为空");
		return false;
	}

	// 写绝对路径，所选文件里的相对路径已按它自己所在目录解析。
	Config::WriteConfig(TCHAR_TO_UTF8(*GetConfigPath()));
	return true;
}

bool UForeverConfigBridgeSubsystem::SelectFolder(FString& OutPath) const {
	IDesktopPlatform* desktopPlatform = FDesktopPlatformModule::Get();
	if (!desktopPlatform) {
		return false;
	}

	FString selected;
	bool success = desktopPlatform->OpenDirectoryDialog(
		ForeverDialog::GetParentWindowHandle(), TEXT("选择文件夹"), TEXT(""), selected);
	if (success) {
		OutPath = selected;
	}
	return success;
}

bool UForeverConfigBridgeSubsystem::SelectFile(const FString& FileFilter, FString& OutPath) const {
	return ForeverDialog::OpenFile(TEXT("选择文件"), FileFilter, OutPath);
}

#include "ForeverModSubsystem.h"

#include "common/config.h"

#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/CoreDelegates.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "ShaderCodeLibrary.h"

void UForeverModSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const FString configPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Resource/Config/config.json"));
	ReadConfigWithFallbacks(configPath);
}

bool UForeverModSubsystem::ReloadConfigFrom(const FString& Path) {
	Config::ReadConfig(TCHAR_TO_UTF8(*Path));

	// ReadConfig对打不开/语法错误的文件只会留空状态、不报错，这里据此判定无效，恢复成config.json。
	if (Config::GetDllPaths().empty() && !Config::HasResourcePaths() && Config::GetStoryScripts().empty()) {
		ReadConfigWithFallbacks(FPaths::Combine(FPaths::ProjectDir(), TEXT("Resource/Config/config.json")));
		return false;
	}

	ReadConfigWithFallbacks(Path);
	return true;
}

void UForeverModSubsystem::ReadConfigWithFallbacks(const FString& configPath) {
	// Config内部用std::cerr打印警告，进程(尤其-game Standalone)不一定把它接进Forever.log，所以这里用UE_LOG打印诊断信息。
	UE_LOG(LogTemp, Log, TEXT("ForeverModSubsystem: configPath=%s, exists=%d"),
		*configPath, FPaths::FileExists(configPath) ? 1 : 0);
	Config::ReadConfig(TCHAR_TO_UTF8(*configPath));
	UE_LOG(LogTemp, Log, TEXT("ForeverModSubsystem: ReadConfig后GetDllPaths().size()=%d, GetMods().size()=%d"),
		static_cast<int>(Config::GetDllPaths().size()), static_cast<int>(Config::GetMods().size()));

	std::vector<std::string> mods = Config::GetMods();
	if (mods.empty()) {
		// config.json缺失、解析失败或dll_paths为空时,回退扫描默认的Forever_Mod/目录
		// (和ForeverKeyBindingSubsystem"缺配置就用硬编码默认值"的容错风格一致)。
		const FString defaultModDir = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectDir(), TEXT("../Forever_Mod")));
		UE_LOG(LogTemp, Warning, TEXT("ForeverModSubsystem: config.json 未提供任何dll_paths,回退扫描默认目录 %s。"), *defaultModDir);
		Config::AddDllPath(TCHAR_TO_UTF8(*defaultModDir));
		mods = Config::GetMods();
	}

	// resource_paths不做回退：config.json里没有就是没有。
}

void UForeverModSubsystem::MountModContent() {
	// 挂载mod自己的UE资产Plugin，Editor/打包分两条路径，原因见ForeverModSubsystem.md。
#if WITH_EDITOR
	// Editor/PIE：直接挂载Plugin的Content目录，不需要cook/pak，也不用在主.uproject里启用。
	for (const std::string& pluginPath : Config::GetPlugins()) {
		FString pluginFile(UTF8_TO_TCHAR(pluginPath.c_str()));
		FString pluginDir = FPaths::GetPath(pluginFile);
		FString pluginName = FPaths::GetBaseFilename(pluginFile);
		FString contentDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(pluginDir, TEXT("Content")));
		FString mountPoint = TEXT("/") + pluginName + TEXT("/");
		if (mountedMountPoints.Contains(mountPoint)) continue; // 上一局已经挂载过
		if (FPaths::DirectoryExists(contentDir)) {
			mountedMountPoints.Add(mountPoint);
			FPackageName::RegisterMountPoint(mountPoint, contentDir);
			if (IAssetRegistry* assetRegistry = IAssetRegistry::Get()) {
				assetRegistry->ScanPathsSynchronous({ mountPoint }, true);
			}
			UE_LOG(LogTemp, Log, TEXT("ForeverModSubsystem: mounted plugin %s -> %s"), *pluginName, *contentDir);
		}
	}
#else
	// 打包/Shipping：挂载mod的.pak，内部路径固定为"../../../<PluginName>/Content/"(mod自己的pak.bat生成filelist时必须对得上)。
	for (const std::string& pakPath : Config::GetPakFiles()) {
		FString pakFile(UTF8_TO_TCHAR(pakPath.c_str()));
		FString pluginName = FPaths::GetBaseFilename(pakFile);
		FString mountPoint = TEXT("/") + pluginName + TEXT("/");
		if (mountedMountPoints.Contains(mountPoint)) continue; // 上一局已经挂载过
		if (FCoreDelegates::MountPaksEx.IsBound()) {
			mountedMountPoints.Add(mountPoint);
			TArray<UE::FMountPaksExArgs> mountArgsArr;
			UE::FMountPaksExArgs& mountArgs = mountArgsArr.AddDefaulted_GetRef();
			mountArgs.PakFilePath = *pakFile;
			mountArgs.Order = 4;
			mountArgs.MountOptions.MountFlags = FPakMountOptions::EMountFlags::SkipContainerFile;
			// 不能靠返回值gate下面的RegisterMountPoint：pak挂载成功时它仍可能返回false，
			// 否则会停在"Mount point ... is not mounted to a valid Root Path yet"的半挂载状态。
			FCoreDelegates::MountPaksEx.Execute(mountArgsArr);
			{
				FString virtualContentDir = TEXT("../../../") + pluginName + TEXT("/Content/");
				FPackageName::RegisterMountPoint(mountPoint, virtualContentDir);
				FShaderCodeLibrary::OpenLibrary(pluginName, virtualContentDir);
				if (IAssetRegistry* assetRegistry = IAssetRegistry::Get()) {
					assetRegistry->ScanPathsSynchronous({ mountPoint }, true);
				}
				UE_LOG(LogTemp, Log, TEXT("ForeverModSubsystem: mounted pak %s"), *pluginName);
			}
		}
	}
#endif
}

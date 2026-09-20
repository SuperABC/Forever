// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Forever : ModuleRules
{
	public Forever(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		bEnableExceptions = true;
		bUseRTTI = true;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "ProceduralMeshComponent", "Water" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// 本模块内部按子文件夹(Player/Input/...)组织,没有Public/Private/Classes三段式结构,
		// 把模块根目录加进来才能用"Player/Xxx.h"、"Input/Xxx.h"这种跨子目录相对路径互相include。
		PublicIncludePaths.Add(ModuleDirectory);

		// Dependence/Core 是与UE无关的纯C++城市模拟内核,以独立的VS静态库工程
		// (Source/Framework.sln)编译,产出的.lib在此手动链接进UE模块。
		// 详见仓库根目录 CONVENTIONS.md。
		string LibDir = Path.Combine(ModuleDirectory, "..", "..", "x64", "Release");
		PublicAdditionalLibraries.Add(Path.Combine(LibDir, "Dependence.lib"));
		PublicAdditionalLibraries.Add(Path.Combine(LibDir, "Core.lib"));

		PublicIncludePaths.AddRange(new string[] {
			Path.Combine(ModuleDirectory, "..", "Dependence"),
			Path.Combine(ModuleDirectory, "..", "Core"),
			Path.Combine(ModuleDirectory, "..", "Core", "common"),
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

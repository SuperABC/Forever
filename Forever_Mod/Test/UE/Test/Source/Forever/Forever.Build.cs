// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

// 迷你工程里的"Forever"桩模块(Editor-only)：只把主程序里mod蓝图要继承的那几个类的头/源文件(硬链接，
// 和Forever_UE/Source/Forever里是同一份)编进来，让编辑器的父类选择器里有它们、蓝图能创建/保存/cook。
// 模块名必须叫Forever，蓝图里记的父类路径才是/Script/Forever.xxx，和主程序一致。
// 不链接Dependence/Core：这里只放纯UE的类(特效等)，需要城市模拟内核的类不放进来。
// 游戏运行时用的永远是主程序exe里的类，这个模块不进打包的游戏，见Forever_Mod/README.md"用mod提供特效蓝图"。
public class Forever : ModuleRules
{
	public Forever(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PublicIncludePaths.Add(ModuleDirectory);
	}
}

using UnrealBuildTool;

// 带画面的UE自动化测试模块：只在命令行带 -ForeverTest=<套件> 时才激活，见 Source/Test/README.md。
// 独立模块(不在Forever模块目录里)，依赖Forever的导出类；不进发行包。
public class ForeverTest : ModuleRules
{
	public ForeverTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Core/Dependence头文件用到异常和RTTI，和Forever模块一致。
		bEnableExceptions = true;
		bUseRTTI = true;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Forever" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Json", "NavigationSystem" });
	}
}

#pragma once

#include "CoreMinimal.h"

class UWorld;

// 内存泄漏排查用：往Forever.log里打一行进程内存+UObject数+各类游戏Actor数的快照。
// 控制台命令`Forever.MemoryReport`随时手动打一行；每次进入主菜单关卡(AForeverMenuController::
// BeginPlay)会先强制GC再自动打一行，反复"开始游戏→暂停界面退出游戏→回菜单"几轮，
// 对比每次进菜单那一行：UsedPhysical/UObjects/Actors应该在第二轮之后趋于平稳，
// 菜单里市民/建筑/载具/资产的数量应该全是0，见ForeverMemoryReport.md。
namespace ForeverMemoryReport
{
	void Log(UWorld* world, const TCHAR* tag);
}

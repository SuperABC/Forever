#pragma once

#include "CoreMinimal.h"

#include <string>

class UWorld;
class ACitizenElement;

// 伤害结算：single_damage/area_damage两个Change的UE层执行体(设计见ForeverDamage.md)。伤害数值、衰减曲线、
// 部位倍率都由脚本算好传进来，这里只负责"扣血 + 致死则广播击杀事件并处理死亡"，以及范围伤害的圈选和遮挡判断。
// 目标只能是已实例化(场景里有Actor)的市民；载具以后另加DamageVehicle。
struct FOREVER_API FForeverDamage
{
	// 对一个市民造成damage点伤害。source=凶手姓名、weapon=武器种类、part=命中部位(head/body/legs，空按body)，
	// 只用于致死时的CitizenDeceaseEvent。已死亡/没有Citizen的目标什么都不做。
	static void DamageCitizen(UWorld* world, ACitizenElement* target, float damage,
		const std::string& source, const std::string& weapon, const std::string& part);

	// 以center为中心、radius为半径的范围伤害。每个市民 damage * (1 - (距离/radius)^power)，超出半径不受伤；
	// occlusion为真时，从center(抬高一点，避免贴地被地面挡)到该市民中心连线，被挡住就不受伤(市民自己不算遮挡物)。
	static void DamageArea(UWorld* world, const FVector& center, float radius, float damage, bool occlusion, float power,
		const std::string& source, const std::string& weapon);

	// 按命中点在胶囊体上的相对高度粗分部位：顶部20%算head，下半截(40%以下)算legs，中间body。
	static const char* ClassifyPart(const ACitizenElement* citizen, const FVector& impactPoint);

	// 解析脚本里的vec3字符串"(x, y, z)"；格式不对返回false，out不变。
	static bool ParseVec3(const std::string& text, FVector& out);
};

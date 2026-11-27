#include "Player/ForeverDamage.h"

#include "Element/CitizenElement.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverStoryFrameworkComponent.h"
#include "Framework/ForeverPopulaceFrameworkComponent.h"
#include "Framework/ForeverDeathFrameworkComponent.h"

#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"
#include "Kismet/GameplayStatics.h"

#include "story/event.h"
#include "populace/citizen.h"
#include "populace/populace.h"

#include <cstdio>

using namespace std;

namespace {
	// 范围伤害的遮挡射线起点抬高(cm)：爆炸点常常贴着地面，不抬高会被地面挡住。
	constexpr float kOcclusionLift = 30.f;
}

const char* FForeverDamage::ClassifyPart(const ACitizenElement* citizen, const FVector& impactPoint) {
	// 射线只打得到胶囊体，按命中点在胶囊体上的相对高度粗分。
	float radius = 0.f, halfHeight = 0.f;
	citizen->GetSimpleCollisionCylinder(radius, halfHeight);
	float relativeHeight = halfHeight > 0.f
		? (impactPoint.Z - (citizen->GetActorLocation().Z - halfHeight)) / (2.f * halfHeight) : 0.5f;
	return relativeHeight >= 0.8f ? "head" : (relativeHeight >= 0.4f ? "body" : "legs");
}

bool FForeverDamage::ParseVec3(const string& text, FVector& out) {
	float x = 0.f, y = 0.f, z = 0.f;
	if (sscanf(text.c_str(), " ( %f , %f , %f )", &x, &y, &z) != 3) return false;
	out = FVector(x, y, z);
	return true;
}

void FForeverDamage::DamageCitizen(UWorld* world, ACitizenElement* target, float damage,
	const string& source, const string& weapon, const string& part) {
	if (!world || !target) return;
	Citizen* citizen = target->GetCitizen();
	if (!citizen || citizen->IsDead()) return;

	citizen->TakeDamage(damage);

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(world, AForeverFrameworkActor::StaticClass()));
	if (!framework) return;

	if (!citizen->IsDead()) {
		// 受伤(没死)：发citizen_injured(单体/范围伤害都走这里，不区分)。致死的那一击只发下面的citizen_decease。
		if (framework->GetStoryFramework()) {
			CitizenInjuredEvent injured(citizen->GetName(), source, weapon, part, damage, citizen->GetHealth());
			framework->GetStoryFramework()->BroadcastCitizenInjured(injured);
		}
		return;
	}

	// 击杀信息：凶手(source姓名)、武器种类、命中部位、致死伤害、凶手是否可见、死亡地址。
	// 凶手可以为空或不在场景里(没实例化/已经销毁)，此时没有"是否可见"(false)。
	ACitizenElement* killerElement = nullptr;
	Citizen* killer = nullptr;
	if (!source.empty() && framework->GetPopulace()) {
		killer = framework->GetPopulace()->FindCitizenByName(source);
		if (killer && framework->GetPopulaceFramework()) killerElement = framework->GetPopulaceFramework()->FindActiveCitizen(killer);
	}
	const string killerName = killer ? killer->GetName() : string();
	const string hitPart = part.empty() ? string("body") : part;
	const string location = framework->DescribeLocation(target->GetActorLocation());

	if (framework->GetStoryFramework()) {
		// 凶手是否可见：连接死者和凶手中心点的线段有没有被遮挡物(墙等)挡住，两人自己不算遮挡。
		bool visible = false;
		if (killerElement) {
			FCollisionQueryParams sightParams;
			sightParams.AddIgnoredActor(target);
			sightParams.AddIgnoredActor(killerElement);
			visible = !world->LineTraceTestByChannel(target->GetActorLocation(),
				killerElement->GetActorLocation(), ECC_Visibility, sightParams);
		}

		CitizenDeceaseEvent event(citizen->GetName(), "player", killerName, weapon, hitPart, damage, visible, location);
		framework->GetStoryFramework()->BroadcastCitizenDecease(event);
	}
	// 死的是玩家扮演的市民：交给死亡流程(慢镜头/结算界面/复活)，尸体先留着，到继续游戏时才撤。
	if (UForeverDeathFrameworkComponent* death = framework->GetDeathFramework()) {
		if (death->HandlePlayerDeath(target, citizen, killerName, weapon, hitPart, location)) return;
	}
	if (framework->GetPopulaceFramework()) {
		framework->GetPopulaceFramework()->HandleCitizenDeath(citizen);
	}
}

void FForeverDamage::DamageArea(UWorld* world, const FVector& center, float radius, float damage, bool occlusion, float power,
	const string& source, const string& weapon) {
	if (!world || radius <= 0.f || damage <= 0.f) return;
	if (power <= 0.f) power = 1.f;

	// 圈出范围内的所有Pawn(市民胶囊体)，先收集成弱指针：伤害结算可能触发死亡处理，改变场景里的Actor。
	TArray<FOverlapResult> overlaps;
	world->OverlapMultiByObjectType(overlaps, center, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(radius));

	TArray<TWeakObjectPtr<ACitizenElement>> targets;
	for (const FOverlapResult& overlap : overlaps) {
		if (ACitizenElement* citizen = Cast<ACitizenElement>(overlap.GetActor())) targets.AddUnique(citizen);
	}

	const FVector rayStart = center + FVector(0.f, 0.f, kOcclusionLift);
	for (const TWeakObjectPtr<ACitizenElement>& weak : targets) {
		ACitizenElement* citizen = weak.Get();
		if (!citizen) continue;

		const float ratio = FVector::Dist(center, citizen->GetActorLocation()) / radius;
		if (ratio >= 1.f) continue;

		if (occlusion) {
			// 必须用ECC_Visibility：这里要找的是墙/地形，市民的胶囊体(Pawn预设忽略Visibility)本来就不挡，目标自己也忽略。
			FCollisionQueryParams params;
			params.AddIgnoredActor(citizen);
			if (world->LineTraceTestByChannel(rayStart, citizen->GetActorLocation(), ECC_Visibility, params)) continue;
		}

		DamageCitizen(world, citizen, damage * (1.f - FMath::Pow(ratio, power)), source, weapon, string());
	}
}

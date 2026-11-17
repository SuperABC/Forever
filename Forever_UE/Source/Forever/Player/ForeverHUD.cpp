#include "Player/ForeverHUD.h"

#include "Player/ForeverCharacter.h"
#include "Element/CitizenElement.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

#include "populace/citizen.h"

void AForeverHUD::DrawHUD()
{
	Super::DrawHUD();

	// 左上角常驻显示玩家操控的市民的生命值(测试用，方便看什么时候被炸死)。Pawn被销毁(死亡)后保留最后一次的显示并标注已死亡。
	if (Canvas && GEngine) {
		static float lastHealth = 0.f, lastMaxHealth = 0.f;
		bool bDead = false;
		if (ACitizenElement* citizenElement = Cast<ACitizenElement>(GetOwningPawn())) {
			if (Citizen* citizen = citizenElement->GetCitizen()) {
				lastHealth = citizen->GetHealth();
				lastMaxHealth = citizen->GetMaxHealth();
				bDead = citizen->IsDead();
			}
		}
		else if (!GetOwningPawn() && lastMaxHealth > 0.f) {
			bDead = true; // 没有Pawn了(市民被销毁)：当作已死亡。坐载具时Pawn是载具，保持最后一次的血量显示。
			lastHealth = 0.f;
		}
		if (lastMaxHealth > 0.f) {
			const FString text = FString::Printf(TEXT("生命值: %d/%d%s"), FMath::CeilToInt(lastHealth), FMath::CeilToInt(lastMaxHealth),
				bDead ? TEXT("  (已死亡)") : TEXT(""));
			DrawText(text, bDead ? FLinearColor::Red : FLinearColor::White, 20.f, 20.f, GEngine->GetMediumFont(), 1.5f);
		}
	}

	AForeverCharacter* character = Cast<AForeverCharacter>(GetOwningPawn());
	if (!character || !character->IsAiming() || !Canvas) return;

	// 激活的是投掷物：不画准星。预览曲线由UForeverThrowableComponent画成带深度测试的世界
	// 线段(这里是屏幕空间，画出来没有遮挡关系)。
	if (character->IsAimingThrow()) return;

	const FVector2D center(Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f);
	const float gap = 6.f;      // 中心留空的半径，不遮住画面正中心的点
	const float armLength = 8.f; // 每条线的长度
	const float thickness = 2.f;
	const FLinearColor color = FLinearColor::White;

	DrawLine(center.X - gap - armLength, center.Y, center.X - gap, center.Y, color, thickness); // 左
	DrawLine(center.X + gap, center.Y, center.X + gap + armLength, center.Y, color, thickness); // 右
	DrawLine(center.X, center.Y - gap - armLength, center.X, center.Y - gap, color, thickness); // 上
	DrawLine(center.X, center.Y + gap, center.X, center.Y + gap + armLength, color, thickness); // 下
}

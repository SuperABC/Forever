#include "Player/ForeverHUD.h"

#include "Player/ForeverCharacter.h"
#include "Engine/Canvas.h"

void AForeverHUD::DrawHUD()
{
	Super::DrawHUD();

	// 只有当前被这个HUD所属PlayerController占有的Pawn是AForeverCharacter且在瞄准中才画——
	// 用户要求"临时"画一个准心，先用DrawLine这类原生Canvas调用验证位置/尺寸，不引入正式的
	// 准心贴图资产，和这次武器系统其余"先用DrawDebugLine/AddOnScreenDebugMessage验证手感"
	// 的一贯做法一致。
	AForeverCharacter* character = Cast<AForeverCharacter>(GetOwningPawn());
	if (!character || !character->IsAiming() || !Canvas) return;

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

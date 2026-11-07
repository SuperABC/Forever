#include "Player/ForeverHUD.h"

#include "Player/ForeverCharacter.h"
#include "Engine/Canvas.h"

void AForeverHUD::DrawHUD()
{
	Super::DrawHUD();

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

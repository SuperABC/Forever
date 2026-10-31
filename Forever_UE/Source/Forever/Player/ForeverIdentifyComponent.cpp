#include "Player/ForeverIdentifyComponent.h"

#include "Element/CitizenElement.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverPopulaceFrameworkComponent.h"
#include "Framework/ForeverSkyFrameworkComponent.h"
#include "UI/IdentifyTagWidget.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include "populace/citizen.h"

UForeverIdentifyComponent::UForeverIdentifyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	tagWidgetClass = UIdentifyTagWidget::StaticClass();
}

void UForeverIdentifyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Exit();
	Super::EndPlay(EndPlayReason);
}

TArray<FString> UForeverIdentifyComponent::BuildLines(const Citizen& citizen) const
{
	TArray<FString> lines;
	lines.Add(UTF8_TO_TCHAR(citizen.GetName().c_str()));
	return lines;
}

void UForeverIdentifyComponent::Enter()
{
	if (bActive) return;
	bActive = true;

	APawn* pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(this, AForeverFrameworkActor::StaticClass()));
	UForeverPopulaceFrameworkComponent* populace = framework ? framework->GetPopulaceFramework() : nullptr;
	if (!pawn || !populace) return;

	// 参与的人物在这一刻定下来：先收集再创建，创建过程中不再查询。
	const FVector origin = pawn->GetActorLocation();
	TArray<ACitizenElement*> inRange;
	populace->ForEachActiveCitizen([&](ACitizenElement* citizenActor) {
		if (!citizenActor || citizenActor->IsHidden() || !citizenActor->GetCitizen()) return;
		if (FVector::Dist(origin, citizenActor->GetActorLocation()) <= identifyRadius) inRange.Add(citizenActor);
	});
	for (ACitizenElement* citizenActor : inRange) AddTag(citizenActor);
}

void UForeverIdentifyComponent::AddTag(ACitizenElement* citizenActor)
{
	UWidgetComponent* tag = NewObject<UWidgetComponent>(citizenActor, NAME_None, RF_Transient);
	tag->SetWidgetSpace(EWidgetSpace::World);
	tag->SetWidgetClass(tagWidgetClass);
	tag->SetDrawSize(drawSize * resolutionScale);
	tag->SetPivot(FVector2D(0.5f, 1.f)); // 底边中点对齐挂点，行往上长
	tag->SetTwoSided(true);
	tag->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	tag->SetCastShadow(false);
	const float brightness = ComputeTintBrightness();
	tag->SetTintColorAndOpacity(FLinearColor(brightness, brightness, brightness, 1.f));

	// 挂在人物身上：人走动时标签跟着走，人被销毁时标签一起消失。
	tag->SetupAttachment(citizenActor->GetRootComponent());
	tag->SetRelativeLocation(FVector(0.f, 0.f,
		citizenActor->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - headBelowCapsuleTop + headMargin));
	tag->SetRelativeScale3D(FVector(tagScale / FMath::Max(resolutionScale, 0.1f)));
	tag->RegisterComponent();

	if (!tag->GetUserWidgetObject()) tag->InitWidget();
	if (UIdentifyTagWidget* widget = Cast<UIdentifyTagWidget>(tag->GetUserWidgetObject())) {
		widget->SetLines(BuildLines(*citizenActor->GetCitizen()), resolutionScale);
	}
	tags.Add(tag);
}

float UForeverIdentifyComponent::ComputeTintBrightness() const
{
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(this, AForeverFrameworkActor::StaticClass()));
	UForeverSkyFrameworkComponent* sky = framework ? framework->GetSkyFramework() : nullptr;
	if (!sky) return tintBrightness;
	// 曝光EV每低1，画面亮一倍：夜里EV比白天参考值低，标签亮度按2的幂次缩小。
	return tintBrightness * FMath::Pow(2.f, sky->GetCurrentExposureEV() - sky->GetReferenceDayEV());
}

void UForeverIdentifyComponent::Exit()
{
	if (!bActive) return;
	bActive = false;

	for (const TWeakObjectPtr<UWidgetComponent>& tag : tags) {
		if (tag.IsValid()) tag->DestroyComponent();
	}
	tags.Reset();
}

void UForeverIdentifyComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bActive || tags.Num() == 0) return;

	APlayerController* controller = Cast<APlayerController>(GetOwner());
	APlayerCameraManager* camera = controller ? controller->PlayerCameraManager : nullptr;
	if (!camera) return;

	// 朝向和屏幕平行(X指向摄像机、Z跟摄像机的上方向一致)，不是"看向摄像机位置"——后者在屏幕
	// 边缘的标签会倾斜。只更新朝向，不更新内容和参与的人物。
	const FRotator cameraRotation = camera->GetCameraRotation();
	const FRotator facing = FRotationMatrix::MakeFromXZ(
		-cameraRotation.Vector(), FRotationMatrix(cameraRotation).GetUnitAxis(EAxis::Z)).Rotator();

	// 识别期间时间还在走(黄昏/黎明曝光在变)，亮度跟着当前曝光每帧换算。
	const float brightness = ComputeTintBrightness();
	const FLinearColor tint(brightness, brightness, brightness, 1.f);
	tags.RemoveAll([&](const TWeakObjectPtr<UWidgetComponent>& tag) {
		if (!tag.IsValid()) return true; // 人物被销毁，标签已经跟着消失
		tag->SetWorldRotation(facing);
		tag->SetTintColorAndOpacity(tint);
		return false;
	});
}

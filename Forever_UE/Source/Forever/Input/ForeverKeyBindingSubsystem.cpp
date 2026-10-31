#include "ForeverKeyBindingSubsystem.h"

#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

const TArray<UForeverKeyBindingSubsystem::FBindingDefinition>& UForeverKeyBindingSubsystem::GetBindingDefinitions()
{
	static const TArray<FBindingDefinition> definitions = {
		{ TEXT("Jump"), EKeys::SpaceBar },
		{ TEXT("ToggleView"), EKeys::V },
		{ TEXT("Sprint"), EKeys::LeftShift },
		// 和"Jump"共用空格键——两者是不同的UInputAction，分别只在各自的Pawn
		// (ACitizenElement/AVehicleElement)上绑定消费，同一个键同时映射到多个动作在
		// Enhanced Input里不冲突，见AVehicleElement::SetupPlayerInputComponent。
		{ TEXT("Handbrake"), EKeys::SpaceBar },
		// 载具系统：驾驶中按Q下车，只绑在AVehicleElement::SetupPlayerInputComponent上
		// （车辆专属，不是全局/不在Controller上）——上车改成MeetOption选"上车"选项，不再
		// 用热键，见ForeverTrafficFrameworkComponent::ApplyEnterVehicle/ExitVehicle。
		{ TEXT("ExitVehicle"), EKeys::Q },
		// MeetOption对话选项UI：滚轮上下移动高亮/F键确认选中，见ForeverPlayerController.md"旧MainController热键"一节，AForeverCharacter::SetupPlayerInputComponent
		// 转发到AForeverPlayerController持有的MeetOptionWidget。
		{ TEXT("MeetOptionUp"), EKeys::MouseScrollUp },
		{ TEXT("MeetOptionDown"), EKeys::MouseScrollDown },
		{ TEXT("MeetOptionSelect"), EKeys::F },
		// 飞机动力：滚轮上增/下减，和MeetOption共用滚轮键(各自只在自己的Pawn上绑定，同一个键映射
		// 多个动作不冲突)，只绑在APlaneElement::SetupPlayerInputComponent上，见PlaneElement.md。
		{ TEXT("ThrottleUp"), EKeys::MouseScrollUp },
		{ TEXT("ThrottleDown"), EKeys::MouseScrollDown },
		// 飞机自由环视：按住时视角不自动回正，只绑在APlaneElement上。
		{ TEXT("FreeLook"), EKeys::C },
		// 剧情对话SectionSpeaking的空格键推进不走这里——UForeverStoryFrameworkComponent::
		// ShowCurrentItem在对话展示期间把输入模式切成UIOnly，Pawn的Enhanced Input完全收不到
		// 输入，空格键改成SectionSpeaking自己的NativeOnKeyDown原生响应，见
		// USectionSpeakingWidget.h类注释。
		// 手机(Phone)系统：这个热键绑在AForeverPlayerController上,不是Pawn上——玩家会通过
		// ChangeControlChange换着操控不同市民/载具,Controller是唯一在换人时不变的对象,见
		// AForeverPlayerController::SetupInputComponent。
		{ TEXT("TogglePhone"), EKeys::P },
		// 背包(Inventory)系统：这个热键和TogglePhone绑在同一个Controller上，理由同上——
		// 玩家会通过ChangeControlChange换着操控不同市民/载具，见
		// AForeverPlayerController::SetupInputComponent、ForeverPlayerController.md"旧MainController热键"一节。
		{ TEXT("ToggleInventory"), EKeys::B },
		// 武器系统：鼠标左键开火(按住连发交给全自动武器自己的fireRate冷却判断，见
		// UForeverWeaponComponent::TickComponent)/R键换弹/数字键1、2切枪，见
		// AForeverCharacter::SetupPlayerInputComponent。
		{ TEXT("FireWeapon"), EKeys::LeftMouseButton },
		{ TEXT("ReloadWeapon"), EKeys::R },
		{ TEXT("SwitchWeapon1"), EKeys::One },
		{ TEXT("SwitchWeapon2"), EKeys::Two },
		{ TEXT("AimWeapon"), EKeys::RightMouseButton },
		// 识别模式：按住鼠标中键显示附近人物头上的信息，松开退出，见UForeverIdentifyComponent。
		{ TEXT("Identify"), EKeys::MiddleMouseButton },
		// 投掷物：短按3在身上的投掷物之间切换，长按3(弹出选择列表)本期未做，见
		// UForeverThrowableComponent。
		{ TEXT("ThrowItem"), EKeys::Three },
		// 游戏内地图：M开/关(ToggleMap)，Esc只关不开(CloseMap)——跟TogglePhone/
		// ToggleInventory同样的理由绑在Controller上，见AForeverPlayerController::
		// SetupInputComponent、UMapWidget::ToggleMap/CloseMap。
		{ TEXT("ToggleMap"), EKeys::M },
		{ TEXT("CloseMap"), EKeys::Escape },
		// 暂停界面(保存/加载)：Tab开关，理由同上绑在Controller上，见AForeverPlayerController::
		// TogglePause、UPausePanelWidget(界面打开期间关闭键由它自己的NativeOnKeyDown拦)。
		{ TEXT("TogglePause"), EKeys::Tab },
	};
	return definitions;
}

void UForeverKeyBindingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const FString configPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Resource/Config/KeyBindings.ini"));

	FConfigFile configFile;
	configFile.Read(configPath);
	if (configFile.Num() == 0) {
		UE_LOG(LogTemp, Warning, TEXT("ForeverKeyBindingSubsystem: 未找到按键配置文件 %s,全部按键使用硬编码默认值。"), *configPath);
	}

	BuildDynamicContext(configFile);
}

FKey UForeverKeyBindingSubsystem::ResolveKey(const FConfigFile& configFile, const FBindingDefinition& definition) const
{
	FString keyName;
	if (configFile.GetString(TEXT("KeyBindings"), *definition.name.ToString(), keyName)) {
		const FKey configuredKey{ FName(*keyName) };
		if (configuredKey.IsValid()) {
			return configuredKey;
		}

		UE_LOG(LogTemp, Warning, TEXT("ForeverKeyBindingSubsystem: 动作 %s 的配置键名 \"%s\" 无效,回退到默认键 %s。"),
			*definition.name.ToString(), *keyName, *definition.defaultKey.ToString());
	}

	return definition.defaultKey;
}

void UForeverKeyBindingSubsystem::BuildDynamicContext(const FConfigFile& configFile)
{
	bindingContext = NewObject<UInputMappingContext>(this);

	for (const FBindingDefinition& definition : GetBindingDefinitions()) {
		UInputAction* action = NewObject<UInputAction>(this);
		action->ValueType = EInputActionValueType::Boolean;

		actions.Add(definition.name, action);
		FKey key = ResolveKey(configFile, definition);
		keys.Add(definition.name, key);
		bindingContext->MapKey(action, key);
	}
}

UInputAction* UForeverKeyBindingSubsystem::GetAction(FName bindingName) const
{
	if (const TObjectPtr<UInputAction>* found = actions.Find(bindingName)) {
		return *found;
	}

	return nullptr;
}

FKey UForeverKeyBindingSubsystem::GetKey(FName bindingName) const
{
	if (const FKey* found = keys.Find(bindingName)) {
		return *found;
	}

	return EKeys::Invalid;
}

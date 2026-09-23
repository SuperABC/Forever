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
		{ TEXT("Test"), EKeys::T },
		// 和"Jump"共用空格键——两者是不同的UInputAction，分别只在各自的Pawn
		// (ACitizenElement/AVehicleElement)上绑定消费，同一个键同时映射到多个动作在
		// Enhanced Input里不冲突，见AVehicleElement::SetupPlayerInputComponent。
		{ TEXT("Handbrake"), EKeys::SpaceBar },
		// MeetOption对话选项UI：滚轮上下移动高亮/F键确认选中，见MAINCONTROLLER_TODO.md
		// "MouseScrollUp/Down + F"这一行热键，AForeverCharacter::SetupPlayerInputComponent
		// 转发到AForeverPlayerController持有的MeetOptionWidget。
		{ TEXT("MeetOptionUp"), EKeys::MouseScrollUp },
		{ TEXT("MeetOptionDown"), EKeys::MouseScrollDown },
		{ TEXT("MeetOptionSelect"), EKeys::F },
		// 剧情对话SectionSpeaking的空格键推进不走这里——UForeverStoryFrameworkComponent::
		// ShowCurrentItem在对话展示期间把输入模式切成UIOnly，Pawn的Enhanced Input完全收不到
		// 输入，空格键改成SectionSpeaking自己的NativeOnKeyDown原生响应，见
		// USectionSpeakingWidget.h类注释。
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
		bindingContext->MapKey(action, ResolveKey(configFile, definition));
	}
}

UInputAction* UForeverKeyBindingSubsystem::GetAction(FName bindingName) const
{
	if (const TObjectPtr<UInputAction>* found = actions.Find(bindingName)) {
		return *found;
	}

	return nullptr;
}

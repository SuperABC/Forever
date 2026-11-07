#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ModPanelWidget.generated.h"

class UScrollBox;
class UButton;
class UPathRowWidget;
class UConceptButtonWidget;
class UModCheckRowWidget;

// Mod screen：左列DllList(dll根目录路径，增删经Bridge)、中列ConceptList(按
// GetModConceptDescriptors()顺序在NativeConstruct里动态生成一次)、右列ModList(选中concept下的mod id勾选框)。
UCLASS()
class FOREVER_API UModPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 任意一个mod勾选框切换后调用——只需要重新刷新右列(锁定状态可能级联变化)，不影响
	// 左列/中列。
	UFUNCTION(BlueprintCallable, Category = "Config")
	void RefreshModList();

	// dll路径增删后调用——刷新左列；因为"至少一个id启用"的DLL集合可能跟着变，顺带刷新
	// 右列(调用方自己决定要不要同时调RefreshModList，这个函数本身不会自动联动)。
	UFUNCTION(BlueprintCallable, Category = "Config")
	void RefreshDllList();

protected:
	virtual void NativeConstruct() override;

	UFUNCTION()
	void HandleAddDllPathClicked();

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UScrollBox> DllList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UScrollBox> ConceptList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UScrollBox> ModList;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> AddDllPathButton;

	// Blueprint侧分别指定三个子Blueprint类——左列行用WBP_PathRow(和Story/Resource共用)，
	// 中列按钮用WBP_ConceptButton，右列行用WBP_ModCheckRow。
	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSubclassOf<UPathRowWidget> PathRowClass;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSubclassOf<UConceptButtonWidget> ConceptButtonClass;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSubclassOf<UModCheckRowWidget> ModCheckRowClass;

private:
	void SelectConcept(const FString& ConceptKey);

	// 当前中列选中的concept——右列ModList只显示这一个concept的mod，没有选中任何concept时
	// (刚打开面板，还没点过任何按钮)为空，右列保持空白。
	FString selectedConceptKey;
};

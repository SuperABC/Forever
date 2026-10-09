#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ForeverModPanelWidget.generated.h"

class UScrollBox;
class UButton;
class UForeverPathRowWidget;
class UForeverConceptButtonWidget;
class UForeverModCheckRowWidget;

// Mod screen：三栏布局——
// - 左列DllList：当前已加载的全部dll(根目录)路径，删除=Bridge::RemoveDllPath，和
//   AddDllPathButton一起管理Config::GetDllPaths()。
// - 中列ConceptList：固定的21个concept按钮，按Source/Core/common/loader.h
//   GetModConceptDescriptors()的顺序在NativeConstruct里动态生成一次(这张表运行期不变)，
//   不需要在Blueprint里手摆21个按钮。
// - 右列ModList：点击中列某个concept按钮后，清空重新显示该concept下全部已发现的mod id
//   勾选框；再点另一个concept按钮时重复这个"清空再生成"的过程。
UCLASS()
class FOREVER_API UForeverModPanelWidget : public UUserWidget
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
	TSubclassOf<UForeverPathRowWidget> PathRowClass;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSubclassOf<UForeverConceptButtonWidget> ConceptButtonClass;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSubclassOf<UForeverModCheckRowWidget> ModCheckRowClass;

private:
	void SelectConcept(const FString& ConceptKey);

	// 当前中列选中的concept——右列ModList只显示这一个concept的mod，没有选中任何concept时
	// (刚打开面板，还没点过任何按钮)为空，右列保持空白。
	FString selectedConceptKey;
};

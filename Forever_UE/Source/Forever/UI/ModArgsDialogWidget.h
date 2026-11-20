#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ModArgsDialogWidget.generated.h"

class UEditableTextBox;
class UHorizontalBox;
class UScrollBox;
class UModArgsDialogWidget;

// 参数行上的按钮回调代理：UMG按钮的OnClicked是动态委托、不带发送者，每行建一个代理才知道点的是哪一行。
UCLASS()
class UModArgRowProxy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TWeakObjectPtr<UModArgsDialogWidget> Owner;

	UPROPERTY()
	TWeakObjectPtr<UHorizontalBox> Row;

	UFUNCTION()
	void OnDeleteClicked();
};

// 一个mod的参数编辑弹窗：每个参数一行(一个原始token，如"--path=C:/x"、"--skip")，可增、删、改，
// 点确定把非空行用单个空格拼回整串参数。控件全由C++动态搭建，不需要蓝图。
// 用法：CreateWidget → Open(标题, 当前参数串, 确定回调) → AddToViewport。见ModArgsDialogWidget.md。
UCLASS()
class FOREVER_API UModArgsDialogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Open(const FString& InTitle, const FString& InArgs, TFunction<void(const FString&)> InOnConfirm);

	void AddArgRow(const FString& Text);
	void RemoveArgRow(UHorizontalBox* Row);

protected:
	UFUNCTION()
	void HandleAddClicked();

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

private:
	FString JoinArgs() const;

	UPROPERTY()
	TObjectPtr<UScrollBox> RowList;

	UPROPERTY()
	TArray<TObjectPtr<UHorizontalBox>> Rows;

	UPROPERTY()
	TArray<TObjectPtr<UEditableTextBox>> Boxes;

	UPROPERTY()
	TArray<TObjectPtr<UModArgRowProxy>> Proxies;

	TFunction<void(const FString&)> onConfirm;
};

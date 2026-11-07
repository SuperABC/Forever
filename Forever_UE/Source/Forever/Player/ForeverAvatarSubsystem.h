#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ForeverAvatarSubsystem.generated.h"

class Citizen;
class UTexture2D;

// 市民头像贴图的加载器(磁盘PNG→UTexture2D，带LRU缓存)，详见ForeverAvatarSubsystem.md。
UCLASS()
class FOREVER_API UForeverAvatarSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 取这个市民的头像，选图规则见md。没有可用头像返回nullptr。
	UTexture2D* GetCitizenAvatar(const Citizen* citizen, int32 currentYear);

private:
	// 读相对Resource/Avatars/的路径(不带扩展名)；命中缓存直接返回，失败(含缓存过的失败)返回nullptr。
	UTexture2D* LoadAvatar(const FString& relativePath);

	// 扫描每个性别目录下的PNG，按文件名里标注的年龄建索引。
	void ScanAvatars();

	// 一张自动头像：文件名形如 a023_00417.png，a后面是这张图标注的年龄。
	struct FAvatarEntry {
		int32 age = 0;
		FString name; // 相对Resource/Avatars/的路径，不带扩展名，如 male/a023_00417
	};

	FString avatarRoot;
	// 按年龄从小到大排序；下标0=男、1=女。
	TArray<FAvatarEntry> entries[2];

	// LRU缓存：超过maxCached时淘汰最久没用的。
	UPROPERTY()
	TMap<FString, TObjectPtr<UTexture2D>> cache;
	TMap<FString, uint64> lastUsed;
	TSet<FString> missing;
	uint64 useCounter = 0;
	static constexpr int32 maxCached = 512;
};

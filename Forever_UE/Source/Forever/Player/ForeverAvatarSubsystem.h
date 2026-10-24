#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ForeverAvatarSubsystem.generated.h"

class Citizen;
class UTexture2D;

// 市民头像贴图的加载器：头像是磁盘上的128x128 PNG(Resource/Avatars/{male|female}/NNNNN.png，由
// Tools/AvatarGen生成)，运行时按需读取并建成UTexture2D，不导入成UE资产。详见ForeverAvatarSubsystem.md。
// 市民按距离流式生成/销毁，所以带一个LRU缓存；文件缺失/目录不存在时返回空，调用方保留默认头像。
UCLASS()
class FOREVER_API UForeverAvatarSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 取这个市民的头像：citizen->GetAvatar()非空先找Resource/Avatars/<avatar>.png(找不到打警告并回退
	// 自动)；自动=在该性别、该市民年龄所在年龄段的头像集合里，按avatarIndex取模挑一张(年龄=
	// currentYear-出生年)。该年龄段没有图时取年龄最接近的。没有可用头像返回nullptr。
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

	// LRU缓存：超过maxCached时淘汰最久没用的。被角色引用着的贴图不会被GC掉(角色的avatarTexture持有它)，
	// 淘汰只是以后再取时重新读一次。
	UPROPERTY()
	TMap<FString, TObjectPtr<UTexture2D>> cache;
	TMap<FString, uint64> lastUsed;
	TSet<FString> missing;
	uint64 useCounter = 0;
	static constexpr int32 maxCached = 512;
};

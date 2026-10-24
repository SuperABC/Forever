#include "Player/ForeverAvatarSubsystem.h"

#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "populace/citizen.h"

namespace {
	const TCHAR* GenderDir(bool bFemale) { return bFemale ? TEXT("female") : TEXT("male"); }
}

void UForeverAvatarSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
	Super::Initialize(Collection);
	avatarRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("Resource/Avatars"));
	ScanAvatars();
	UE_LOG(LogTemp, Log, TEXT("UForeverAvatarSubsystem: %d male / %d female avatars under %s."), entries[0].Num(), entries[1].Num(), *avatarRoot);
}

void UForeverAvatarSubsystem::ScanAvatars() {
	for (int32 g = 0; g < 2; g++) {
		TArray<FString> files;
		IFileManager::Get().FindFiles(files, *FPaths::Combine(avatarRoot, GenderDir(g == 1), TEXT("*.png")), true, false);
		entries[g].Reset();
		for (const FString& file : files) {
			// a{年龄}_{编号}.png：年龄解析不出来的文件不当自动头像用。
			FString stem = FPaths::GetBaseFilename(file);
			FString agePart, indexPart;
			if (!stem.StartsWith(TEXT("a")) || !stem.Split(TEXT("_"), &agePart, &indexPart)) continue;
			agePart.RightChopInline(1);
			if (!agePart.IsNumeric()) continue;
			FAvatarEntry entry;
			entry.age = FCString::Atoi(*agePart);
			entry.name = FString::Printf(TEXT("%s/%s"), GenderDir(g == 1), *stem);
			entries[g].Add(entry);
		}
		entries[g].Sort([](const FAvatarEntry& a, const FAvatarEntry& b) {
			return a.age != b.age ? a.age < b.age : a.name < b.name;
		});
	}
}

UTexture2D* UForeverAvatarSubsystem::GetCitizenAvatar(const Citizen* citizen, int32 currentYear) {
	if (!citizen) return nullptr;

	const std::string& custom = citizen->GetAvatar();
	if (!custom.empty()) {
		if (UTexture2D* texture = LoadAvatar(UTF8_TO_TCHAR(custom.c_str()))) return texture;
		UE_LOG(LogTemp, Warning, TEXT("UForeverAvatarSubsystem: avatar \"%s\" of citizen \"%s\" not found, using an automatic one."),
			UTF8_TO_TCHAR(custom.c_str()), UTF8_TO_TCHAR(citizen->GetName().c_str()));
	}

	const TArray<FAvatarEntry>& list = entries[citizen->GetGender() == GENDER_FEMALE ? 1 : 0];
	if (list.IsEmpty()) return nullptr;

	// 年龄段边界必须和Tools/AvatarGen/generate.mjs里的BANDS一致；同一段内的市民从这一段的头像里按
	// avatarIndex挑，年龄在段内增长时头像不变，跨段才换(自然的"长大了")。
	static const int32 kBandMax[] = { 5, 12, 17, 29, 44, 59, 74, MAX_int32 };
	const int32 age = FMath::Max(0, currentYear - citizen->GetBirthYear());
	int32 bandMin = 0, bandMax = MAX_int32;
	for (int32 i = 0; i < UE_ARRAY_COUNT(kBandMax); i++) {
		if (age <= kBandMax[i]) {
			bandMax = kBandMax[i];
			bandMin = i == 0 ? 0 : kBandMax[i - 1] + 1;
			break;
		}
	}

	// list按年龄排序，段内是一段连续区间。
	int32 first = list.IndexOfByPredicate([&](const FAvatarEntry& e) { return e.age >= bandMin; });
	int32 last = first;
	if (first != INDEX_NONE) {
		while (last < list.Num() && list[last].age <= bandMax) last++;
	}
	if (first == INDEX_NONE || last == first) {
		// 这一段没有图(没生成/段外年龄)：取年龄最接近的那张所在年龄。
		int32 best = 0;
		for (int32 i = 1; i < list.Num(); i++) {
			if (FMath::Abs(list[i].age - age) < FMath::Abs(list[best].age - age)) best = i;
		}
		first = best;
		last = best + 1;
	}
	return LoadAvatar(list[first + citizen->GetAvatarIndex() % (last - first)].name);
}

UTexture2D* UForeverAvatarSubsystem::LoadAvatar(const FString& relativePath) {
	if (missing.Contains(relativePath)) return nullptr;

	if (TObjectPtr<UTexture2D>* hit = cache.Find(relativePath)) {
		if (UTexture2D* texture = hit->Get()) {
			lastUsed.Add(relativePath, ++useCounter);
			return texture;
		}
		cache.Remove(relativePath);
		lastUsed.Remove(relativePath);
	}

	// 自定义头像名来自剧情脚本，不允许跳出头像目录。
	if (relativePath.Contains(TEXT(".."))) {
		missing.Add(relativePath);
		return nullptr;
	}

	TArray<uint8> bytes;
	if (!FFileHelper::LoadFileToArray(bytes, *FPaths::Combine(avatarRoot, relativePath + TEXT(".png")))) {
		missing.Add(relativePath);
		return nullptr;
	}
	UTexture2D* texture = FImageUtils::ImportBufferAsTexture2D(bytes);
	if (!texture) {
		missing.Add(relativePath);
		return nullptr;
	}

	if (cache.Num() >= maxCached) {
		const FString* oldest = nullptr;
		uint64 oldestUse = TNumericLimits<uint64>::Max();
		for (const TPair<FString, uint64>& entry : lastUsed) {
			if (entry.Value < oldestUse) {
				oldestUse = entry.Value;
				oldest = &entry.Key;
			}
		}
		if (oldest) {
			FString key = *oldest;
			cache.Remove(key);
			lastUsed.Remove(key);
		}
	}
	cache.Add(relativePath, texture);
	lastUsed.Add(relativePath, ++useCounter);
	return texture;
}

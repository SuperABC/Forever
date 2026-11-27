#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ForeverVoiceSubsystem.generated.h"

class USoundBase;

// 对话语音的加载器：voice以'/'开头按UE资产路径加载，否则当成resource_paths里的.wav/.ogg/.mp3文件名，运行时解码成PCM
// (带按字节数的LRU缓存)，每次播放给一个新的USoundWaveProcedural。详见ForeverVoiceSubsystem.md。
UCLASS()
class FOREVER_API UForeverVoiceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 取一条对话语音可以直接播放的声音；空串/找不到/解不了返回nullptr(各只警告一次)。
	// 文件语音每次返回一个新的一次性声音对象，交给SpawnSound2D之后由音频组件持有。
	USoundBase* ResolveVoice(const FString& voice);

	// UE资产路径补成对象路径："/Game/Asset/Sound/pistol" -> "/Game/Asset/Sound/pistol.pistol"(已经带'.'的原样返回)。
	// sound_effect也用它。
	static FString ToObjectPath(const FString& assetPath);

private:
	struct FDecodedVoice {
		TArray<uint8> pcm;     // 16位有符号PCM，声道交错
		int32 sampleRate = 0;
		int32 channels = 0;
		float duration = 0.f;
		FDateTime stamp;       // 文件修改时间：变了就重新解码
		uint64 lastUsed = 0;
	};

	// 按完整路径解码(命中缓存且文件没改过直接返回)；失败返回nullptr。
	const FDecodedVoice* Decode(const FString& fullPath);
	void EvictIfNeeded(const FString& keep);
	void WarnOnce(const FString& key, const FString& message);

	TMap<FString, FDecodedVoice> cache;
	int64 cachedBytes = 0;
	uint64 useCounter = 0;
	TSet<FString> warned;
	static constexpr int64 maxCachedBytes = 64ll * 1024 * 1024;
};

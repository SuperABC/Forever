#include "Player/ForeverVoiceSubsystem.h"

#include "Player/ForeverAudioDecode.h"

#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/Package.h"


FString UForeverVoiceSubsystem::ToObjectPath(const FString& assetPath) {
	if (assetPath.Contains(TEXT("."))) return assetPath;
	return assetPath + TEXT(".") + FPackageName::GetShortName(assetPath);
}

void UForeverVoiceSubsystem::WarnOnce(const FString& key, const FString& message) {
	if (warned.Contains(key)) return;
	warned.Add(key);
	UE_LOG(LogTemp, Warning, TEXT("对话语音：%s"), *message);
}

USoundBase* UForeverVoiceSubsystem::ResolveVoice(const FString& voice) {
	if (voice.IsEmpty()) return nullptr;

	// UE资产(主工程/Game/...或mod插件的/<Mod>/...)。
	if (voice.StartsWith(TEXT("/"))) {
		USoundBase* sound = LoadObject<USoundBase>(nullptr, *ToObjectPath(voice));
		if (!sound) WarnOnce(voice, FString::Printf(TEXT("加载不到UE声音资产 %s"), *voice));
		return sound;
	}

	// 文件：在config的resource_paths里按文件名(含扩展名)找。
	const FString found = ForeverAudioDecode::ResolveResourceFile(voice);
	if (found.IsEmpty()) {
		WarnOnce(voice, FString::Printf(TEXT("resource_paths里找不到语音文件 %s"), *voice));
		return nullptr;
	}
	const FDecodedVoice* decoded = Decode(found);
	if (!decoded) return nullptr;

	// 程序化声音只能把排进去的PCM播一遍，所以每次播放都新建一个。
	USoundWaveProcedural* wave = NewObject<USoundWaveProcedural>(GetTransientPackage());
	wave->SetSampleRate(static_cast<uint32>(decoded->sampleRate));
	wave->NumChannels = decoded->channels;
	wave->Duration = decoded->duration;
	wave->SoundGroup = SOUNDGROUP_Voice;
	wave->bLooping = false;
	wave->QueueAudio(decoded->pcm.GetData(), decoded->pcm.Num());
	return wave;
}

const UForeverVoiceSubsystem::FDecodedVoice* UForeverVoiceSubsystem::Decode(const FString& fullPath) {
	const FDateTime stamp = IFileManager::Get().GetTimeStamp(*fullPath);
	if (FDecodedVoice* hit = cache.Find(fullPath)) {
		if (hit->stamp == stamp) {
			hit->lastUsed = ++useCounter;
			return hit;
		}
		cachedBytes -= hit->pcm.Num();
		cache.Remove(fullPath); // 文件改过：重新解码
	}

	ForeverAudioDecode::EnsureDecodersLoaded();
	FForeverDecodedAudio audio;
	FString error;
	if (!ForeverAudioDecode::DecodeFile(fullPath, audio, error)) {
		WarnOnce(fullPath, FString::Printf(TEXT("解码失败 %s：%s"), *fullPath, *error));
		return nullptr;
	}
	FDecodedVoice decoded;
	decoded.pcm = MoveTemp(audio.pcm);
	decoded.sampleRate = audio.sampleRate;
	decoded.channels = audio.channels;
	decoded.duration = audio.duration;
	decoded.stamp = stamp;
	decoded.lastUsed = ++useCounter;
	cachedBytes += decoded.pcm.Num();
	cache.Add(fullPath, MoveTemp(decoded));
	EvictIfNeeded(fullPath); // 不会淘汰刚放进去的这一条
	return cache.Find(fullPath);
}

void UForeverVoiceSubsystem::EvictIfNeeded(const FString& keep) {
	while (cachedBytes > maxCachedBytes && cache.Num() > 1) {
		const FString* oldest = nullptr;
		uint64 oldestUse = MAX_uint64;
		for (const auto& [path, entry] : cache) {
			if (path == keep || entry.lastUsed >= oldestUse) continue;
			oldest = &path;
			oldestUse = entry.lastUsed;
		}
		if (!oldest) break;
		const FString victim = *oldest;
		cachedBytes -= cache[victim].pcm.Num();
		cache.Remove(victim);
	}
}

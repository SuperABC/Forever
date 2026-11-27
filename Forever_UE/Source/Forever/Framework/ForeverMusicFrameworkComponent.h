#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Async/Future.h"

#include <atomic>

#include "Player/ForeverAudioDecode.h"

#include "ForeverMusicFrameworkComponent.generated.h"

class UAudioComponent;
class USoundBase;

// 文件音乐的声音对象：从共享的整首PCM里按游标往外喂(音频线程)，进度精确、跳转=改游标、暂停时音频线程不来取所以游标停住、
// 循环时读到结尾回到开头。不循环时读到结尾就只喂静音，由UForeverMusicFrameworkComponent看到IsFinished()后停掉。
UCLASS()
class FOREVER_API UForeverPcmSoundWave : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	void Setup(TSharedPtr<const FForeverDecodedAudio, ESPMode::ThreadSafe> inAudio, double startSeconds, bool bInLoop);
	double GetPositionSeconds() const;
	bool IsFinished() const { return bFinished.load(); }

	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;

private:
	TSharedPtr<const FForeverDecodedAudio, ESPMode::ThreadSafe> audio;
	std::atomic<int64> cursor{ 0 };  // 下一次要喂的样本下标(所有声道一起数)
	std::atomic<bool> bLoopPlayback{ false };
	std::atomic<bool> bFinished{ false };
};

// 音乐音轨的UE侧：每帧对照Core的MusicTracks(Player::GetMusic)去播，并把实际进度/时长/状态写回Core，详见
// ForeverMusicFrameworkComponent.md。一律2D、按界面声音处理(暂停游戏时继续放)。
UCLASS()
class FOREVER_API UForeverMusicFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverMusicFrameworkComponent();
	virtual void TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction* thisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

	// 解码结果缓存上限(字节)：一首三分钟的立体声约35MB，切回最近放过的歌不用重新解。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
	int64 maxCachedBytes = 128ll * 1024 * 1024;

private:
	using FAudioPtr = TSharedPtr<const FForeverDecodedAudio, ESPMode::ThreadSafe>;

	// UE这边正在放的一条音轨(声音对象和音频组件在下面的UPROPERTY表里，按轨道名)。
	struct FPlaying {
		uint64 generation = 0;     // 对应Core Track::generation，变了就从Core记的进度重新开始
		FString asset;
		bool bLoop = false;
		bool bAssetSound = false;  // UE资产(否则是文件)
		double startSeconds = 0.0; // 这次从哪开始
		double duration = 0.0;
		double assetPosition = 0.0; // UE资产的进度(OnAudioPlaybackPercent回调写)
		bool bAssetFinished = false;
		bool bEnded = false;
		bool bError = false;
		bool bLoading = false;
		FString loadingPath;       // 文件资产：在等这个完整路径的异步解码(decoding表)
		bool bPausedApplied = false; // 已经对音频组件SetPaused过的状态，和Core不一样时再设
	};

	// 开始/重新开始一条音轨(generation变了时)。文件没解码过就发起异步解码，状态先是加载中。
	void Start(const FString& track, double startSeconds, const FString& asset, bool bLoop, uint64 generation);
	void BeginFilePlayback(const FString& track, FPlaying& playing, FAudioPtr audio);
	void StopAudio(const FString& track);
	FAudioPtr FindCached(const FString& fullPath);
	void AddToCache(const FString& fullPath, FAudioPtr audio);

	TMap<FString, FPlaying> playing;
	UPROPERTY()
	TMap<FString, TObjectPtr<UAudioComponent>> audioComponents;

	struct FCached {
		FAudioPtr audio;
		FDateTime stamp;
		uint64 lastUsed = 0;
	};
	TMap<FString, FCached> cache;
	// 正在线程池里解码的文件(完整路径 -> 结果)：同一个文件只解一次，解完放进cache，等它的音轨下一帧开始播。
	TMap<FString, TFuture<FAudioPtr>> decoding;
	int64 cachedBytes = 0;
	uint64 useCounter = 0;
	TSet<FString> warned;
};

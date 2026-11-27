#include "Framework/ForeverMusicFrameworkComponent.h"

#include "Framework/CoreGuard.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Player/ForeverVoiceSubsystem.h"

#include "Async/Async.h"
#include "Components/AudioComponent.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#include "player/music.h"
#include "player/player.h"

// ---- UForeverPcmSoundWave ----

void UForeverPcmSoundWave::Setup(TSharedPtr<const FForeverDecodedAudio, ESPMode::ThreadSafe> inAudio, double startSeconds, bool bInLoop) {
	audio = inAudio;
	NumChannels = audio ? audio->channels : 1;
	SetSampleRate(static_cast<uint32>(audio ? audio->sampleRate : 44100));
	// 时长交给音频组件就会按它自己算的播放时间停，跳转后从中间开始就对不上了：按无限长处理，停由组件这边管。
	Duration = INDEFINITELY_LOOPING_DURATION;
	bLooping = false;
	SoundGroup = SOUNDGROUP_Music;
	int64 start = 0;
	if (audio && audio->channels > 0) {
		start = static_cast<int64>(FMath::Max(0.0, startSeconds) * audio->sampleRate) * audio->channels;
		start = FMath::Min(start, audio->NumSamples());
	}
	cursor.store(start);
	bLoopPlayback.store(bInLoop);
	bFinished.store(false);
}

double UForeverPcmSoundWave::GetPositionSeconds() const {
	if (!audio || audio->channels <= 0 || audio->sampleRate <= 0) return 0.0;
	return static_cast<double>(cursor.load()) / (static_cast<double>(audio->channels) * audio->sampleRate);
}

int32 UForeverPcmSoundWave::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) {
	// 音频线程。返回0时引擎补静音。
	if (!audio || audio->pcm.Num() < 2 || NumSamples <= 0) return 0;
	const int64 total = audio->NumSamples();
	const int32 channels = FMath::Max(1, audio->channels);
	int32 want = NumSamples - NumSamples % channels;
	if (want <= 0) want = NumSamples;
	OutAudio.SetNumUninitialized(want * 2);
	int16* dst = reinterpret_cast<int16*>(OutAudio.GetData());
	const int16* src = reinterpret_cast<const int16*>(audio->pcm.GetData());
	int32 written = 0;
	int64 position = cursor.load();
	while (written < want) {
		if (position >= total) {
			if (bLoopPlayback.load()) position = 0;
			else {
				bFinished.store(true);
				break;
			}
		}
		const int64 count = FMath::Min<int64>(want - written, total - position);
		FMemory::Memcpy(dst + written, src + position, count * 2);
		written += static_cast<int32>(count);
		position += count;
	}
	cursor.store(position);
	if (written == 0) {
		OutAudio.Reset();
		return 0;
	}
	OutAudio.SetNum(written * 2, EAllowShrinking::No);
	return written;
}

// ---- UForeverMusicFrameworkComponent ----

UForeverMusicFrameworkComponent::UForeverMusicFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true; // 暂停游戏时音乐照常放，进度照常回写
}

void UForeverMusicFrameworkComponent::EndPlay(const EEndPlayReason::Type endPlayReason) {
	TArray<FString> names;
	audioComponents.GetKeys(names);
	for (const FString& name : names) StopAudio(name);
	playing.Reset();
	cache.Reset();
	cachedBytes = 0;
	decoding.Reset(); // 还没解完的任务各自持有自己的数据，结果没人要了就随它释放
	Super::EndPlay(endPlayReason);
}

UForeverMusicFrameworkComponent::FAudioPtr UForeverMusicFrameworkComponent::FindCached(const FString& fullPath) {
	FCached* hit = cache.Find(fullPath);
	if (!hit) return nullptr;
	if (hit->stamp != IFileManager::Get().GetTimeStamp(*fullPath)) { // 文件改过：重新解码
		cachedBytes -= hit->audio ? hit->audio->pcm.Num() : 0;
		cache.Remove(fullPath);
		return nullptr;
	}
	hit->lastUsed = ++useCounter;
	return hit->audio;
}

void UForeverMusicFrameworkComponent::AddToCache(const FString& fullPath, FAudioPtr audio) {
	if (!audio) return;
	if (FCached* old = cache.Find(fullPath)) cachedBytes -= old->audio ? old->audio->pcm.Num() : 0;
	FCached entry;
	entry.audio = audio;
	entry.stamp = IFileManager::Get().GetTimeStamp(*fullPath);
	entry.lastUsed = ++useCounter;
	cache.Add(fullPath, entry);
	cachedBytes += audio->pcm.Num();
	// 淘汰最久没用的(正在放的音轨自己持有一份共享指针，淘汰了也照常放完)。
	while (cachedBytes > maxCachedBytes && cache.Num() > 1) {
		FString oldest;
		uint64 oldestUse = MAX_uint64;
		for (const auto& [path, cached] : cache) {
			if (path != fullPath && cached.lastUsed < oldestUse) { oldest = path; oldestUse = cached.lastUsed; }
		}
		if (oldest.IsEmpty()) break;
		cachedBytes -= cache[oldest].audio ? cache[oldest].audio->pcm.Num() : 0;
		cache.Remove(oldest);
	}
}

void UForeverMusicFrameworkComponent::StopAudio(const FString& track) {
	if (TObjectPtr<UAudioComponent>* found = audioComponents.Find(track)) {
		if (UAudioComponent* component = found->Get()) {
			component->OnAudioFinishedNative.RemoveAll(this);
			component->OnAudioPlaybackPercentNative.RemoveAll(this);
			component->Stop();
			component->DestroyComponent();
		}
	}
	audioComponents.Remove(track);
}

void UForeverMusicFrameworkComponent::Start(const FString& track, double startSeconds, const FString& asset, bool bLoop, uint64 generation) {
	StopAudio(track);
	FPlaying& entry = playing.FindOrAdd(track);
	entry = FPlaying();
	entry.generation = generation;
	entry.asset = asset;
	entry.bLoop = bLoop;
	entry.startSeconds = startSeconds;

	auto warnOnce = [this](const FString& key, const FString& message) {
		if (warned.Contains(key)) return;
		warned.Add(key);
		UE_LOG(LogTemp, Warning, TEXT("音乐：%s"), *message);
	};

	// UE资产(/Game/...或mod插件的/<Mod>/...)：普通音频组件，进度用播放百分比回调，循环在播完回调里重新Play。
	if (asset.StartsWith(TEXT("/"))) {
		USoundBase* sound = LoadObject<USoundBase>(nullptr, *UForeverVoiceSubsystem::ToObjectPath(asset));
		if (!sound) {
			warnOnce(asset, FString::Printf(TEXT("加载不到UE声音资产 %s"), *asset));
			entry.bError = true;
			return;
		}
		entry.bAssetSound = true;
		const float duration = sound->GetDuration();
		entry.duration = duration >= INDEFINITELY_LOOPING_DURATION ? 0.0 : duration;
		UAudioComponent* component = UGameplayStatics::CreateSound2D(this, sound, 1.f, 1.f, 0.f, nullptr, false, false);
		if (!component) {
			entry.bError = true; // 没有音频设备(-nosound)
			return;
		}
		component->bIsUISound = true;
		component->OnAudioPlaybackPercentNative.AddWeakLambda(this, [this, track](const UAudioComponent*, const USoundWave*, float percent) {
			if (FPlaying* current = playing.Find(track)) current->assetPosition = percent * current->duration;
		});
		component->OnAudioFinishedNative.AddWeakLambda(this, [this, track](UAudioComponent*) {
			if (FPlaying* current = playing.Find(track)) current->bAssetFinished = true;
		});
		component->Play(static_cast<float>(startSeconds));
		entry.assetPosition = startSeconds;
		audioComponents.Add(track, component);
		return;
	}

	// 文件：resource_paths里按文件名找，解码过的直接放，没解过的扔进线程池解，状态先是加载中。
	const FString fullPath = ForeverAudioDecode::ResolveResourceFile(asset);
	if (fullPath.IsEmpty()) {
		warnOnce(asset, FString::Printf(TEXT("resource_paths里找不到音乐文件 %s"), *asset));
		entry.bError = true;
		return;
	}
	if (FAudioPtr cached = FindCached(fullPath)) {
		BeginFilePlayback(track, entry, cached);
		return;
	}
	entry.bLoading = true;
	entry.loadingPath = fullPath;
	if (!decoding.Contains(fullPath)) {
		ForeverAudioDecode::EnsureDecodersLoaded(); // vorbis动态库要在游戏线程先加载
		decoding.Add(fullPath, Async(EAsyncExecution::ThreadPool, [fullPath]() -> FAudioPtr {
			TSharedPtr<FForeverDecodedAudio, ESPMode::ThreadSafe> decoded = MakeShared<FForeverDecodedAudio, ESPMode::ThreadSafe>();
			FString error;
			if (!ForeverAudioDecode::DecodeFile(fullPath, *decoded, error)) {
				UE_LOG(LogTemp, Warning, TEXT("音乐：解码失败 %s：%s"), *fullPath, *error);
				return nullptr;
			}
			return decoded;
		}));
	}
}

void UForeverMusicFrameworkComponent::BeginFilePlayback(const FString& track, FPlaying& entry, FAudioPtr audio) {
	entry.bLoading = false;
	entry.duration = audio->duration;
	UForeverPcmSoundWave* wave = NewObject<UForeverPcmSoundWave>(this);
	wave->Setup(audio, entry.startSeconds, entry.bLoop);
	UAudioComponent* component = UGameplayStatics::CreateSound2D(this, wave, 1.f, 1.f, 0.f, nullptr, false, false);
	if (!component) {
		entry.bError = true; // 没有音频设备(-nosound)
		return;
	}
	component->bIsUISound = true;
	component->Play();
	entry.bPausedApplied = false;
	audioComponents.Add(track, component);
}

void UForeverMusicFrameworkComponent::TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction* thisTickFunction) {
	Super::TickComponent(deltaTime, tickType, thisTickFunction);
	CORE_GUARD_BEGIN("UForeverMusicFrameworkComponent::TickComponent")
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* player = framework ? framework->GetPlayer() : nullptr;
	if (!player) return;
	MusicTracks& music = player->GetMusic();

	// 解完的文件放进缓存。
	for (auto it = decoding.CreateIterator(); it; ++it) {
		if (!it->Value.IsReady()) continue;
		FAudioPtr decoded = it->Value.Get();
		if (decoded) AddToCache(it->Key, decoded);
		else warned.Add(it->Key);
		it.RemoveCurrent();
	}

	// Core里已经没有的轨道(stop了)：停掉。
	TArray<FString> names;
	playing.GetKeys(names);
	for (const FString& name : names) {
		if (!music.Find(TCHAR_TO_UTF8(*name))) {
			StopAudio(name);
			playing.Remove(name);
		}
	}

	for (const auto& [trackName, track] : music.GetTracks()) {
		const FString name = UTF8_TO_TCHAR(trackName.c_str());
		FPlaying* entry = playing.Find(name);
		if (!entry || entry->generation != track.generation) {
			Start(name, track.position, UTF8_TO_TCHAR(track.asset.c_str()), track.loop, track.generation);
			entry = playing.Find(name);
		}
		if (!entry) continue;

		// 在等异步解码的：解完(进了缓存)就开始放；解码失败就是出错。
		if (entry->bLoading && !decoding.Contains(entry->loadingPath)) {
			if (FAudioPtr cached = FindCached(entry->loadingPath)) BeginFilePlayback(name, *entry, cached);
			else {
				entry->bLoading = false;
				entry->bError = true;
			}
		}

		UAudioComponent* component = audioComponents.FindRef(name);
		if (component && !entry->bEnded && entry->bPausedApplied != track.paused) {
			component->SetPaused(track.paused);
			entry->bPausedApplied = track.paused;
		}

		double position = track.position;
		MusicTracks::Status status = MusicTracks::Status::Playing;
		if (entry->bError) status = MusicTracks::Status::Error;
		else if (entry->bLoading) status = MusicTracks::Status::Loading;
		else if (!entry->bEnded && !entry->bAssetSound) {
			UForeverPcmSoundWave* wave = component ? Cast<UForeverPcmSoundWave>(component->Sound) : nullptr;
			if (wave) position = wave->GetPositionSeconds();
			if (wave && wave->IsFinished()) entry->bEnded = true;
		}
		else if (!entry->bEnded && entry->bAssetSound) {
			position = entry->assetPosition;
			if (entry->bAssetFinished) {
				entry->bAssetFinished = false;
				if (entry->bLoop && component) {
					component->Play(0.f);
					entry->assetPosition = 0.0;
					position = 0.0;
				}
				else entry->bEnded = true;
			}
		}
		if (entry->bEnded) {
			if (component) StopAudio(name);
			status = MusicTracks::Status::Ended;
			position = entry->duration;
		}
		music.Report(trackName, position, entry->duration, status);
	}
	CORE_GUARD_END
}

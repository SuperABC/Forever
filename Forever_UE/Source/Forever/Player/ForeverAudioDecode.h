#pragma once

#include "CoreMinimal.h"

// 原始音频文件 -> 16位有符号PCM(声道交错)，对话语音(UForeverVoiceSubsystem)和音乐(UForeverMusicFrameworkComponent)共用，
// 见ForeverAudioDecode.md。全部是纯函数，可以在工作线程里调(OGG要先在游戏线程调过一次EnsureDecodersLoaded)。
struct FForeverDecodedAudio {
	TArray<uint8> pcm;     // 16位小端，声道交错
	int32 sampleRate = 0;
	int32 channels = 0;
	float duration = 0.f;  // 秒

	int64 NumSamples() const { return pcm.Num() / 2; } // 所有声道的样本总数
};

namespace ForeverAudioDecode {
	// OGG的vorbis在Windows上是动态库：第一次解码之前要在游戏线程加载一次。重复调用没有开销。
	void EnsureDecodersLoaded();

	// 按扩展名(.wav/.ogg/.mp3，不分大小写)解码整个文件；只支持单声道/立体声。失败返回false并给出原因。
	bool DecodeFile(const FString& fullPath, FForeverDecodedAudio& out, FString& error);
	bool DecodeBytes(const TArray<uint8>& bytes, const FString& extension, FForeverDecodedAudio& out, FString& error);

	bool DecodeWav(const TArray<uint8>& bytes, FForeverDecodedAudio& out, FString& error); // 16位PCM
	bool DecodeOgg(const TArray<uint8>& bytes, FForeverDecodedAudio& out, FString& error); // 引擎VorbisAudioDecoder
	bool DecodeMp3(const TArray<uint8>& bytes, FForeverDecodedAudio& out, FString& error); // 引擎dr_libs的FMP3Decoder

	// 文件资产名(UTF-8，同对话语音的voice/音乐的asset)在config的resource_paths里的完整路径，找不到返回空串。
	FString ResolveResourceFile(const FString& fileName);
}

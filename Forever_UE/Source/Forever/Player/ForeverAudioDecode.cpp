#include "Player/ForeverAudioDecode.h"

#include "Audio.h"
#include "Decoders/VorbisAudioInfo.h"
#include "Interfaces/IAudioFormat.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#if WITH_DR_LIBS_MP3
#include "dr_libs_mp3decoder.h"
#endif

#include "common/config.h"

namespace {
	constexpr uint16 kWaveFormatPcm = 1;
	constexpr uint16 kWaveFormatExtensible = 0xFFFE;
	constexpr int32 kMp3MaxSamplesPerFrame = 1152 * 2; // 一帧最多1152个样本×2声道

	void FinishDuration(FForeverDecodedAudio& out) {
		out.duration = out.channels > 0 && out.sampleRate > 0
			? static_cast<float>(static_cast<double>(out.NumSamples()) / (static_cast<double>(out.channels) * out.sampleRate)) : 0.f;
	}
}

void ForeverAudioDecode::EnsureDecodersLoaded() {
	static bool bLoaded = false;
	if (bLoaded) return;
	LoadVorbisLibraries();
	bLoaded = true;
}

FString ForeverAudioDecode::ResolveResourceFile(const FString& fileName) {
	const std::string found = Config::GetResourceFilePath(TCHAR_TO_UTF8(*fileName));
	return found.empty() ? FString() : FString(UTF8_TO_TCHAR(found.c_str()));
}

bool ForeverAudioDecode::DecodeFile(const FString& fullPath, FForeverDecodedAudio& out, FString& error) {
	TArray<uint8> bytes;
	if (!FFileHelper::LoadFileToArray(bytes, *fullPath)) {
		error = TEXT("读不了文件");
		return false;
	}
	return DecodeBytes(bytes, FPaths::GetExtension(fullPath), out, error);
}

bool ForeverAudioDecode::DecodeBytes(const TArray<uint8>& bytes, const FString& extension, FForeverDecodedAudio& out, FString& error) {
	const FString lower = extension.ToLower();
	bool ok = false;
	if (lower == TEXT("wav")) ok = DecodeWav(bytes, out, error);
	else if (lower == TEXT("ogg")) ok = DecodeOgg(bytes, out, error);
	else if (lower == TEXT("mp3")) ok = DecodeMp3(bytes, out, error);
	else error = TEXT("只支持.wav/.ogg/.mp3");
	if (ok && (out.channels < 1 || out.channels > 2 || out.sampleRate <= 0 || out.pcm.Num() == 0)) {
		error = FString::Printf(TEXT("只支持单声道/立体声(这个文件%d声道、%dHz)"), out.channels, out.sampleRate);
		return false;
	}
	return ok;
}

bool ForeverAudioDecode::DecodeWav(const TArray<uint8>& bytes, FForeverDecodedAudio& out, FString& error) {
	FWaveModInfo info;
	if (!info.ReadWaveInfo(bytes.GetData(), bytes.Num(), &error)) return false;
	const uint16 format = info.pFormatTag ? *info.pFormatTag : 0;
	if ((format != kWaveFormatPcm && format != kWaveFormatExtensible) || !info.pBitsPerSample || *info.pBitsPerSample != 16) {
		error = TEXT("WAV只支持16位PCM");
		return false;
	}
	out.channels = *info.pChannels;
	out.sampleRate = static_cast<int32>(*info.pSamplesPerSec);
	out.pcm.Reset();
	out.pcm.Append(info.SampleDataStart, static_cast<int32>(info.SampleDataSize));
	FinishDuration(out);
	return true;
}

bool ForeverAudioDecode::DecodeOgg(const TArray<uint8>& bytes, FForeverDecodedAudio& out, FString& error) {
	FVorbisAudioInfo vorbis;
	FSoundQualityInfo quality = {};
	if (!vorbis.ReadCompressedInfo(bytes.GetData(), static_cast<uint32>(bytes.Num()), &quality)) {
		error = TEXT("不是有效的Ogg Vorbis文件");
		return false;
	}
	out.channels = static_cast<int32>(quality.NumChannels);
	out.sampleRate = static_cast<int32>(quality.SampleRate);
	out.pcm.SetNumZeroed(static_cast<int32>(quality.SampleDataSize));
	vorbis.ExpandFile(out.pcm.GetData(), &quality);
	FinishDuration(out);
	return true;
}

bool ForeverAudioDecode::DecodeMp3(const TArray<uint8>& bytes, FForeverDecodedAudio& out, FString& error) {
#if WITH_DR_LIBS_MP3
	int32 offset = 0;
	// 开头的ID3v2标签：10字节头 + 同步安全整数的标签长度(+可选的10字节尾)，直接跳过。
	if (bytes.Num() >= 10 && bytes[0] == 'I' && bytes[1] == 'D' && bytes[2] == '3') {
		const int32 tagSize = (bytes[6] & 0x7F) << 21 | (bytes[7] & 0x7F) << 14 | (bytes[8] & 0x7F) << 7 | (bytes[9] & 0x7F);
		offset = 10 + tagSize + ((bytes[5] & 0x10) ? 10 : 0);
	}
	FMP3Decoder decoder;
	TArray<float> frame;
	frame.SetNumUninitialized(kMp3MaxSamplesPerFrame);
	out.pcm.Reset();
	out.pcm.Reserve(bytes.Num() * 8); // 粗估(192kbps立体声约7倍)，不够TArray自己再长
	out.channels = 0;
	out.sampleRate = 0;
	while (offset < bytes.Num()) {
		FMP3Decoder::FFrameInfo info;
		const int decoded = decoder.Decode(&info, frame.GetData(), kMp3MaxSamplesPerFrame * static_cast<int>(sizeof(float)),
			bytes.GetData() + offset, bytes.Num() - offset);
		if (decoded < 0 || info.NumFrameBytes <= 0) break; // 出错或者后面没有帧了
		offset += info.NumFrameBytes;
		if (decoded == 0) continue; // 跳过的垃圾数据/标签
		if (out.channels == 0) {
			out.channels = info.NumChannels;
			out.sampleRate = info.SampleRate;
		}
		if (info.NumChannels != out.channels || info.SampleRate != out.sampleRate) continue; // 中途变格式的帧不要
		const int32 count = decoded * info.NumChannels;
		const int32 start = out.pcm.AddUninitialized(count * 2);
		int16* dst = reinterpret_cast<int16*>(out.pcm.GetData() + start);
		for (int32 i = 0; i < count; i++) {
			dst[i] = static_cast<int16>(FMath::Clamp(FMath::RoundToInt(frame[i] * 32767.f), -32768, 32767));
		}
	}
	if (out.pcm.Num() == 0) {
		error = TEXT("不是有效的MP3文件");
		return false;
	}
	out.pcm.Shrink();
	FinishDuration(out);
	return true;
#else
	error = TEXT("这个平台的引擎不带MP3解码");
	return false;
#endif
}

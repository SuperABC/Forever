# ForeverAudioDecode（运行时音频解码）

把原始音频文件整个解成16位有符号PCM(声道交错，`FForeverDecodedAudio`)。对话语音(`UForeverVoiceSubsystem`)和音乐
(`UForeverMusicFrameworkComponent`)共用。全是纯函数，可以在线程池里调。

| 格式 | 实现 | 说明 |
|---|---|---|
| `.wav` | `FWaveModInfo` | 只接受16位PCM |
| `.ogg` | 引擎`VorbisAudioDecoder`模块的`FVorbisAudioInfo` | Windows上vorbis是动态库，第一次解码前要在**游戏线程**调一次`EnsureDecodersLoaded()` |
| `.mp3` | 引擎`dr_libs`模块的`FMP3Decoder`(`WITH_DR_LIBS_MP3`，Electra播放器也用它) | 先跳过开头的ID3v2标签，再逐帧解成float转int16；采样率/声道取第一帧 |

只支持单声道和立体声；扩展名不分大小写。失败返回false和原因(由调用方决定只警告一次)。

`ResolveResourceFile(name)`：UTF-8文件名(可以是中文) → `Config::GetResourceFilePath` → UE的`FString`完整路径，找不到返回空串。
Config的文件名索引是UTF-8的(见`Core/common/config.md`)。

内存：整首解码，一首3分钟44.1kHz立体声约35MB。语音和音乐各自有按字节数的LRU缓存。

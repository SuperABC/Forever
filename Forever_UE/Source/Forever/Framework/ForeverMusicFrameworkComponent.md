# UForeverMusicFrameworkComponent（音乐播放）

挂在`AForeverFrameworkActor`上(`GetMusicFramework()`)。每帧对照Core的`Player::GetMusic()`(`Core/player/music.md`)播放音乐，
并把实际进度/时长/状态`Report`回Core。UE这边不接收命令，没有对外接口。

## 每帧做的事

1. 线程池里解完的文件放进解码缓存。
2. Core里已经没有的轨道(被stop了)：停掉并销毁音频组件。
3. 每条Core轨道：
   - UE这边没有，或者`generation`和开始播时不同 → `Start`：从Core记的`position`开始；
   - 文件资产还在等解码 → 状态报`loading`；解完就开始播；解码失败报`error`；
   - `paused`和已经设过的不一样 → `UAudioComponent::SetPaused`；
   - 回写进度：文件资产读`UForeverPcmSoundWave`的游标，UE资产用`OnAudioPlaybackPercent`×时长；
   - 不循环的放完了 → 停掉音频组件，状态报`ended`，进度=时长。

一律2D、`bIsUISound=true`，组件`bTickEvenWhenPaused`：按Esc暂停游戏时音乐照常放，进度照常回写。

## 两种资产

- **UE声音资产**(`/`开头)：`LoadObject<USoundBase>`(路径补全用`UForeverVoiceSubsystem::ToObjectPath`) + `CreateSound2D`，
  `Play(startTime)`跳转；循环不靠资产自己的Looping，而是在`OnAudioFinished`里重新`Play(0)`；时长取`GetDuration()`(无限长的报0)。
- **文件**(resource_paths里的wav/ogg/mp3)：`ForeverAudioDecode::ResolveResourceFile`找完整路径，在线程池里`DecodeFile`整首解成16位PCM
  (见`Source/Forever/Player/ForeverAudioDecode.md`)，交给`UForeverPcmSoundWave`播。

### UForeverPcmSoundWave

继承`USoundWaveProcedural`，`OnGeneratePCMAudio`(音频线程)从共享的整首PCM里按原子游标往外拷：
- 进度 = 游标/(声道×采样率)，精确；
- 跳转不改游标，而是整条重新开始(新的声音对象，游标从起点)，和Core的generation语义一致；
- 暂停时音频线程不来取，游标自然停住；
- 循环时读到结尾回到开头；不循环时读到结尾置`IsFinished()`，之后只返回0(引擎补静音)，由组件停掉。

`Duration`设成`INDEFINITELY_LOOPING_DURATION`：给真实时长的话音频组件会按自己算的播放时间提前停(跳转后从中间开始就对不上)。

## 解码缓存

按完整路径缓存解码结果(`TSharedPtr`，播放中的声音对象自己也持有一份，淘汰了照常放完)，文件修改时间变了重新解。
总字节数上限`maxCachedBytes`(默认128MB，一首3分钟立体声约35MB)，超了按最近使用淘汰。同一个文件同时只解一次(`decoding`表)，
解码期间轨道被seek/重新play不会丢掉这次解码。找不到文件/加载不到资产/解码失败只警告一次(`warned`)。

## 构建依赖

`Forever.Build.cs`：`dr_libs`(MP3)、`VorbisAudioDecoder`(OGG)、`AudioExtensions`(继承`USoundWaveProcedural`链接要用)。

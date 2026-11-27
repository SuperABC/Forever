# ForeverVoiceSubsystem.h / ForeverVoiceSubsystem.cpp

## 职责

对话语音的加载器（`UGameInstanceSubsystem`）。脚本对话每一句的 `voice` 字段求值后交给 `ResolveVoice`，拿到可以直接播放的 `USoundBase`；播放和停止由 `UForeverStoryFrameworkComponent` 管（见那边的"对话语音"）。

## voice 的两种写法

和头像（`ForeverAvatarSubsystem`）同一套规则：

- **以 `/` 开头**：UE 声音资产路径，主工程的 `/Game/...` 或 mod 插件里的 `/<Mod>/...`，`LoadObject<USoundBase>`。只写包路径时自动补对象名（`ToObjectPath`，`sound_effect` 也用它）。主工程的语音适合走这条：引擎自带压缩、流式加载。
- **其它**：文件名（要带扩展名），在 config 的 `resource_paths` 里按文件名查找（`Config::GetResourceFilePath`，和 `.script`、mod 图片同一个索引，重名时靠前的根目录优先，新文件要重开一局才会被扫到）。外部 mod 把语音文件和脚本放一起就能用，不需要 UE 编辑器、不用 cook。
  - `.wav`/`.ogg`/`.mp3`：解码在 `ForeverAudioDecode`（和音乐共用，见 `ForeverAudioDecode.md`），整个解成 16 位 PCM。文件名可以是中文（Config 的索引是 UTF-8）。
  - 只支持单声道/立体声。每次播放新建一个 `USoundWaveProcedural`（采样率、声道、时长、`SOUNDGROUP_Voice`、不循环）把 PCM 排进去——程序化声音只能播一遍，所以不复用。

`voice` 在脚本里和 speaker/content 一样是表达式，字面量要带内层引号：`"voice": "\"greeting.ogg\""`、`"voice": "\"/Game/Asset/Sound/camera\""`。空串=没有语音。

## 缓存与失败

- 解码结果按完整路径缓存，带文件修改时间：文件改过就重新解码。按 PCM 字节数做 LRU，上限 64MB（`maxCachedBytes`）。
- 找不到的文件、加载不到的资产、解不了的格式各只警告一次（`warned`），之后返回 nullptr 不再刷日志。

## 打包

文件语音和脚本、图片一样从 `resource_paths` 读原始文件。现在的打包（`pak.bat`）里 `Resource/` 是 junction，所以直接能读；正式发行要把这些原始文件真正放进包里（见 TODO）。UE 资产语音随 cook/mod pak 走。

## 测试

- 2026-11-27 用 `Tools/SoundGen/voicegen.py` 合成的占位语音（WAV 和 `--ogg` 转出的 OGG）在 UE 里验证过：两种格式解码出的声道和时长一致，前台/后台/UE 资产三种播放都正常。验证完演示文件和脚本里的 `voice` 都撤掉了，仓库里没有常驻的语音文件；以后要再测就用这个脚本重新生成，放进任一 `resource_paths` 目录。
- UE 测试 `components` 套件（`Source/Test/UE`）常驻检查：UE 资产路径能加载、找不到的文件和空串返回空。

## 依赖关系

- 依赖：`ForeverAudioDecode`(内部用 Core `Config::GetResourceFilePath`、引擎 `FWaveModInfo`/`VorbisAudioDecoder`/`dr_libs`)、`USoundWaveProcedural`。
- 被谁依赖：`UForeverStoryFrameworkComponent`（对话语音）、`UForeverAssetFrameworkComponent::ApplySoundEffect`（`ToObjectPath`）。

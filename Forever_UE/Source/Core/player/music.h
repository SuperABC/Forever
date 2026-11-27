#pragma once

#include <cstdint>
#include <map>
#include <string>


// 音乐音轨：按轨道名各自独立的一条音乐，详见music.md。Core只记"应该是什么样"(哪首、放没放、从哪开始)，
// UE层(UForeverMusicFrameworkComponent)每帧对照着去播，并把实际进度/时长/状态写回来——存档存的就是真实进度。
// 两个入口：post handle {"post":"music",...}(implement.cpp) 和 music change(Player::ApplyChange)。
class MusicTracks {
public:
	enum class Status { Loading, Playing, Ended, Error };

	struct Track {
		std::string asset;      // '/'开头=UE声音资产路径，否则=resource_paths里的.wav/.ogg/.mp3文件名(同对话语音)
		bool loop = false;
		bool paused = false;
		double position = 0.0;  // 秒。play/seek时是"从这里开始播"，之后由UE每帧回写实际进度
		double duration = 0.0;  // 秒，UE加载完才知道，之前是0
		Status status = Status::Loading;
		// 每次play/seek加1：UE看到变了就从position重新开始播。读档后从1开始，UE本来就没在播，也会从position接着放。
		uint64_t generation = 0;
	};

	// 都返回空串=成功，否则是失败原因(post/change原样报出来)。
	// play：新建或替换这条轨道，从头播asset(asset不能为空)。
	std::string Play(const std::string& track, const std::string& asset, bool loop);
	std::string Pause(const std::string& track);
	std::string Resume(const std::string& track);
	// stop：删掉这条轨道(UE随之停掉声音)。
	std::string Stop(const std::string& track);
	// seek：跳到seconds(<0按0)，暂停状态不变；已经放完(Ended)的轨道跳转后接着放。
	std::string Seek(const std::string& track, double seconds);
	// 按op名字分派("play"/"pause"/"resume"/"stop"/"seek")，post和change共用。
	std::string Apply(const std::string& op, const std::string& track, const std::string& asset, bool loop, double position);

	const Track* Find(const std::string& track) const;
	const std::map<std::string, Track>& GetTracks() const { return tracks; }

	// UE层回写：实际进度、时长、状态。轨道已经不在(刚被stop)时什么都不做。
	void Report(const std::string& track, double position, double duration, Status status);

	static const char* StatusName(Status status, bool paused); // loading/playing/paused/ended/error

private:
	friend class PlayerSaveCodec;
	std::map<std::string, Track> tracks;
	uint64_t nextGeneration = 1;
};

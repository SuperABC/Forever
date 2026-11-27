#include "player/music.h"

#include <algorithm>


using namespace std;

string MusicTracks::Play(const string& track, const string& asset, bool loop) {
	if (track.empty()) return "music: track name is empty";
	if (asset.empty()) return "music: play needs an asset";
	Track& entry = tracks[track];
	entry = Track();
	entry.asset = asset;
	entry.loop = loop;
	entry.generation = nextGeneration++;
	return "";
}

string MusicTracks::Pause(const string& track) {
	auto it = tracks.find(track);
	if (it == tracks.end()) return "music: no track named \"" + track + "\"";
	it->second.paused = true;
	return "";
}

string MusicTracks::Resume(const string& track) {
	auto it = tracks.find(track);
	if (it == tracks.end()) return "music: no track named \"" + track + "\"";
	it->second.paused = false;
	return "";
}

string MusicTracks::Stop(const string& track) {
	if (tracks.erase(track) == 0) return "music: no track named \"" + track + "\"";
	return "";
}

string MusicTracks::Seek(const string& track, double seconds) {
	auto it = tracks.find(track);
	if (it == tracks.end()) return "music: no track named \"" + track + "\"";
	Track& entry = it->second;
	entry.position = max(0.0, seconds);
	if (entry.duration > 0.0) entry.position = min(entry.position, entry.duration);
	if (entry.status == Status::Ended) entry.status = Status::Playing;
	entry.generation = nextGeneration++;
	return "";
}

string MusicTracks::Apply(const string& op, const string& track, const string& asset, bool loop, double position) {
	if (op == "play") return Play(track, asset, loop);
	if (op == "pause") return Pause(track);
	if (op == "resume") return Resume(track);
	if (op == "stop") return Stop(track);
	if (op == "seek") return Seek(track, position);
	return "music: unknown op \"" + op + "\" (play/pause/resume/stop/seek)";
}

const MusicTracks::Track* MusicTracks::Find(const string& track) const {
	auto it = tracks.find(track);
	return it != tracks.end() ? &it->second : nullptr;
}

void MusicTracks::Report(const string& track, double position, double duration, Status status) {
	auto it = tracks.find(track);
	if (it == tracks.end()) return;
	it->second.position = max(0.0, position);
	it->second.duration = max(0.0, duration);
	it->second.status = status;
}

const char* MusicTracks::StatusName(Status status, bool paused) {
	switch (status) {
	case Status::Loading: return "loading";
	case Status::Playing: return paused ? "paused" : "playing";
	case Status::Ended: return "ended";
	case Status::Error: return "error";
	}
	return "error";
}

#include "terrain_file.h"

#include "common/utility.h"

#include <fstream>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

using namespace std;

int FileTerrain::count = 0;

namespace {
	// 取"--path=xxx"或"--path xxx"的值：到下一个" --"或结尾，首尾空白去掉(路径里可以有空格)。
	string ParsePathArg(const string& args) {
		size_t position = args.find("--path");
		if (position == string::npos) return "";
		position += 6;
		while (position < args.size() && (args[position] == '=' || args[position] == ' ' || args[position] == '\t')) position++;
		size_t end = args.find(" --", position);
		string value = args.substr(position, end == string::npos ? string::npos : end - position);
		while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n')) value.pop_back();
		return value;
	}

	struct Reader {
		const vector<char>& data;
		size_t offset = 0;
		bool ok = true;
		explicit Reader(const vector<char>& d) : data(d) {}
		template <typename T> T Read() {
			T value{};
			if (offset + sizeof(T) > data.size()) { ok = false; return value; }
			memcpy(&value, data.data() + offset, sizeof(T));
			offset += sizeof(T);
			return value;
		}
		string ReadString(uint32_t length) {
			if (offset + length > data.size()) { ok = false; return ""; }
			string value(data.data() + offset, length);
			offset += length;
			return value;
		}
	};
}

FileTerrain::FileTerrain(const string& args) : id(count++) {
	path = ParsePathArg(args);
}

FileTerrain::~FileTerrain() {

}

const char* FileTerrain::GetId() {
	return "terrain_file";
}

const char* FileTerrain::GetType() const {
	return "terrain_file";
}

const char* FileTerrain::GetName() {
	name = "文件地形" + to_string(id);
	return name.data();
}

float FileTerrain::GetPriority() const {
	// 最先执行：其它地形mod(山/海/河)若同时启用，只会在文件没占用的plain格上再叠加。
	return 1.0f;
}

void FileTerrain::SetupTexture() {
	diffusePath = "/Game/Asset/Terrain/Plain/PlainDiffuse.PlainDiffuse";
	normalPath = "/Game/Asset/Terrain/Plain/PlainNormal.PlainNormal";
	roughnessPath = "/Game/Asset/Terrain/Plain/PlainRoughness.PlainRoughness";
}

void FileTerrain::DistributeTerrain(int width, int height,
	const function<const string&(int, int)>& getTerrain, const function<bool(int, int, const string&)>& setTerrain,
	const function<float(int, int)>& getHeight, const function<bool(int, int, float)>& setHeight,
	const function<bool(int, int, bool, float)>& setWater,
	const function<bool(int, int, const float*)>& setFineHeights) const {

	if (path.empty()) {
		debugf("Warning: FileTerrain has no --path, terrain left untouched.\n");
		return;
	}
	vector<char> data;
	{
		ifstream file(path, ios::binary);
		if (!file) {
			debugf("Warning: FileTerrain cannot open %s, terrain left untouched.\n", path.c_str());
			return;
		}
		data.assign(istreambuf_iterator<char>(file), istreambuf_iterator<char>());
	}

	// 格式见terrain_file.md：魔数FTRN, u32 w,h, 调色板(u16条数,每条u16长度+UTF8), u16[w*h]下标, f32[w*h]高度, f32[w*h]水位(NaN=无水)。
	Reader reader(data);
	string magic = reader.ReadString(4);
	if (!reader.ok || magic != "FTRN") {
		debugf("Warning: FileTerrain %s is not a .terrain file, terrain left untouched.\n", path.c_str());
		return;
	}
	uint32_t fileWidth = reader.Read<uint32_t>();
	uint32_t fileHeight = reader.Read<uint32_t>();
	uint16_t paletteCount = reader.Read<uint16_t>();
	vector<string> palette;
	for (uint16_t i = 0; i < paletteCount && reader.ok; i++) {
		uint16_t length = reader.Read<uint16_t>();
		palette.push_back(reader.ReadString(length));
	}
	size_t cellCount = static_cast<size_t>(fileWidth) * fileHeight;
	size_t expected = reader.offset + cellCount * (sizeof(uint16_t) + 2 * sizeof(float));
	if (!reader.ok || palette.empty() || fileWidth == 0 || fileHeight == 0 || expected > data.size()) {
		debugf("Warning: FileTerrain %s is truncated, terrain left untouched.\n", path.c_str());
		return;
	}
	const char* indices = data.data() + reader.offset;
	const char* heights = indices + cellCount * sizeof(uint16_t);
	const char* waters = heights + cellCount * sizeof(float);

	if (static_cast<int>(fileWidth) != width || static_cast<int>(fileHeight) != height)
		debugf("Warning: FileTerrain size %ux%u differs from map %dx%d, clipping/padding with plain.\n", fileWidth, fileHeight, width, height);

	int copyWidth = min(width, static_cast<int>(fileWidth));
	int copyHeight = min(height, static_cast<int>(fileHeight));
	for (int y = 0; y < copyHeight; y++) {
		for (int x = 0; x < copyWidth; x++) {
			size_t cell = static_cast<size_t>(y) * fileWidth + x;
			uint16_t index;
			memcpy(&index, indices + cell * sizeof(uint16_t), sizeof(index));
			const string& terrain = index < palette.size() ? palette[index] : palette[0];
			setTerrain(x, y, terrain);
			// 建筑假定在高度0的平原上：plain/construction强制高度0。水不限制：岸边的平原格也要有水面，否则水面和地面之间会露出缝隙。
			bool flat = terrain == "plain" || terrain == "construction";
			float elementHeight, elementWater;
			memcpy(&elementHeight, heights + cell * sizeof(float), sizeof(float));
			memcpy(&elementWater, waters + cell * sizeof(float), sizeof(float));
			if (flat) elementHeight = 0.f;
			// 先setHeight后setWater(SetHeight会重置fine heights，且这里本来就没有fine heights)。
			if (elementHeight != 0.f) setHeight(x, y, elementHeight);
			if (!std::isnan(elementWater)) setWater(x, y, true, elementWater);
		}
	}
	debugf("Log: FileTerrain loaded %s (%ux%u).\n", path.c_str(), fileWidth, fileHeight);
}

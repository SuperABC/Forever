#include "terrain_file.h"

#include "common/utility.h"
#include "common/mod_args.h"

#include <fstream>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

using namespace std;

int FileTerrain::count = 0;

namespace {
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
	path = ModArgs(args).GetString("path");
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
	SetMapColor(124, 152, 92);
}

void FileTerrain::DistributeTerrain(int width, int height,
	const function<const string&(int, int)>& getTerrain, const function<bool(int, int, const string&)>& setTerrain,
	const function<float(int, int)>& getHeight, const function<bool(int, int, float)>& setHeight,
	const function<bool(int, int, bool, float)>& setWater,
	const function<bool(int, int, const float*)>& setFineHeights) const {

	vector<char> data;
	// [map_embed:load begin] Tools/MapEmbed生成内嵌数据mod时把begin/end之间换成从内嵌数组读进data，这一段之外的代码原样拷过去。
	if (path.empty()) {
		debugf(DebugLevel::Warning, "FileTerrain has no --path, terrain left untouched.\n");
		return;
	}
	{
		ifstream file(path, ios::binary);
		if (!file) {
			debugf(DebugLevel::Warning, "FileTerrain cannot open %s, terrain left untouched.\n", path.c_str());
			return;
		}
		data.assign(istreambuf_iterator<char>(file), istreambuf_iterator<char>());
	}
	// [map_embed:load end]

	// 格式见terrain_file.md：魔数FTRN, u32 w,h, 调色板(u16条数,每条u16长度+UTF8), u16[w*h]下标, f32[w*h]高度, f32[w*h]水位(NaN=无水)。
	Reader reader(data);
	string magic = reader.ReadString(4);
	if (!reader.ok || magic != "FTRN") {
		debugf(DebugLevel::Warning, "FileTerrain %s is not a .terrain file, terrain left untouched.\n", path.c_str());
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
		debugf(DebugLevel::Warning, "FileTerrain %s is truncated, terrain left untouched.\n", path.c_str());
		return;
	}
	const char* indices = data.data() + reader.offset;
	const char* heights = indices + cellCount * sizeof(uint16_t);
	const char* waters = heights + cellCount * sizeof(float);

	if (static_cast<int>(fileWidth) != width || static_cast<int>(fileHeight) != height)
		debugf(DebugLevel::Warning, "FileTerrain size %ux%u differs from map %dx%d, clipping/padding with plain.\n", fileWidth, fileHeight, width, height);

	int copyWidth = min(width, static_cast<int>(fileWidth));
	int copyHeight = min(height, static_cast<int>(fileHeight));
	for (int y = 0; y < copyHeight; y++) {
		for (int x = 0; x < copyWidth; x++) {
			size_t cell = static_cast<size_t>(y) * fileWidth + x;
			uint16_t index;
			memcpy(&index, indices + cell * sizeof(uint16_t), sizeof(index));
			const string& terrain = index < palette.size() ? palette[index] : palette[0];
			setTerrain(x, y, terrain);
			// 平原格的高度照文件读(编辑器的"允许改平地"可以抬高/压低平原)；Core只把3x3全是平原且高度都为0的区域升格成建设用地。
			float elementHeight, elementWater;
			memcpy(&elementHeight, heights + cell * sizeof(float), sizeof(float));
			memcpy(&elementWater, waters + cell * sizeof(float), sizeof(float));
			// 先setHeight后setWater(SetHeight会重置fine heights，且这里本来就没有fine heights)。
			if (elementHeight != 0.f) setHeight(x, y, elementHeight);
			if (!std::isnan(elementWater)) setWater(x, y, true, elementWater);
		}
	}
	debugf(DebugLevel::Log, "FileTerrain loaded %s (%ux%u).\n", path.c_str(), fileWidth, fileHeight);
}

#include "terrain.h"
#include "utility.h"


using namespace std;

string OceanTerrain::GetId() {
    return "ocean";
}

string OceanTerrain::GetType() const {
    return "ocean";
}

string OceanTerrain::GetName() const {
    return "海洋地形";
}

float OceanTerrain::GetPriority() const {
    return 1.0f;
};

void OceanTerrain::DistributeTerrain(int width, int height,
    function<bool(int, int, const string)> set, function<string(int, int)> get) const {
	// 随机生成四个方向是否临海，并保证至少有一面临海
	int distribute = 0;
	while (distribute == 0) {
		distribute = GetRandom(16);
	}

	// 处理西面海面区域
	if (distribute & 1) {
		int distance = width / 16 + GetRandom(width / 16);
		debugf("Sea in the west %d.\n", distance);
		float shift = 0.0f, slope = 0.0f;
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < distance + shift; x++) {
				set(x, y, GetType());
			}
			slope += float(GetRandom(33) - 16) / 256.0f;
			shift += slope;
			if (distance + shift <= width / 64) {
				slope = 0.0f;
				shift = width / 64 - (float)distance;
			}
			if (distance + shift >= width / 8) {
				slope = 0.0f;
				shift = width / 8 - (float)distance;
			}
		}
	}

	// 处理东面海面区域
	if (distribute & 2) {
		int distance = width / 16 + GetRandom(width / 16);
		debugf("Sea in the east %d.\n", distance);
		float shift = 0.0f, slope = 0.0f;
		for (int y = 0; y < height; y++) {
			for (int x = width - 1; x >= width - distance - shift; x--) {
				set(x, y, GetType());
			}
			slope += float(GetRandom(33) - 16) / 256.0f;
			shift += slope;
			if (distance + shift <= width / 64) {
				slope = 0.0f;
				shift = width / 64.f - distance;
			}
			if (distance + shift >= width / 8) {
				slope = 0.0f;
				shift = width / 8.f - distance;
			}
		}
	}

	// 处理北面海面区域
	if (distribute & 4) {
		int distance = height / 16 + GetRandom(height / 16);
		debugf("Sea in the north %d.\n", distance);
		float shift = 0.0f, slope = 0.0f;
		for (int x = 0; x < width; x++) {
			for (int y = 0; y < distance + shift; y++) {
				set(x, y, GetType());
			}
			slope += float(GetRandom(33) - 16) / 256.0f;
			shift += slope;
			if (distance + shift <= width / 64) {
				slope = 0.0f;
				shift = width / 64.f - distance;
			}
			if (distance + shift >= width / 8) {
				slope = 0.0f;
				shift = width / 8.f - distance;
			}
		}
	}

	// 处理南面海面区域
	if (distribute & 8) {
		int distance = height / 16 + GetRandom(height / 16);
		debugf("Sea in the south %d.\n", distance);
		float shift = 0.0f, slope = 0.0f;
		for (int x = 0; x < width; x++) {
			for (int y = height - 1; y >= height - distance - shift; y--) {
				set(x, y, GetType());
			}
			slope += float(GetRandom(33) - 16) / 256.0f;
			shift += slope;
			if (distance + shift <= width / 64) {
				slope = 0.0f;
				shift = width / 64.f - distance;
			}
			if (distance + shift >= width / 8) {
				slope = 0.0f;
				shift = width / 8.f - distance;
			}
		}
	}
}

string MountainTerrain::GetId() {
	return "mountain";
}

string MountainTerrain::GetType() const {
	return "mountain";
}

string MountainTerrain::GetName() const {
	return "山区地形";
}

float MountainTerrain::GetPriority() const {
	return 0.9f;
};

void MountainTerrain::DistributeTerrain(int width, int height,
	function<bool(int, int, const string)> set, function<string(int, int)> get) const {
	int scalar = width * height / (512 * 512);
	int num = scalar > 1 ? (4 + GetRandom(scalar * 2)) : 0;

	Counter counter(100);
	int mountain = 0;
	for (int i = 0; i < num; i++) {
		int x = width / 8 + GetRandom(width * 3 / 4);
		int y = height / 8 + GetRandom(height * 3 / 4);

		if (get(x, y) != "plain" ||
			sqrt(pow(x - width / 2, 2) + pow(y - height / 2, 2)) < 128) {
			if (!counter.count())i--;
			continue;
		}
		FloodTerrain(x, y, (int)(64 * 64 + scalar * 0.2f * ((GetRandom(4) ? 0 : 1) * GetRandom(512 * 512)) + GetRandom(128 * 128)),
			false, width, height, set, get);
		mountain++;
	}
	for (int i = 2; i < width - 2; i++) {
		for (int j = 2; j < height - 2; j++) {
			ShapeFilter(i, j, width, height, set, get, 2, 0.5f);
		}
	}
	debugf("Generate mountain %d.\n", mountain);
}


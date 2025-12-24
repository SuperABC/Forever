#include "terrain_base.h"
#include "../common/utility.h"


using namespace std;

int Terrain::FloodTerrain(int x, int y, int num, bool overwrite, int width, int height,
    function<bool(int, int, const string)> set, function<string(int, int)> get) const {
    vector<pair<int, int>> q;

    if (x < 0 || x >= width - 1 || y < 0 || y >= width - 1)return 0;
    if (get(x, y) == GetType())return 0;

    set(x, y, GetType());
    int count = 1;

    for (int i = 0; i < 4; ++i) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
            q.push_back(make_pair(nx, ny));
        }
    }

    while (count < num && !q.empty()) {
        int idx = GetRandom((int)q.size());
        pair<int, int> current = q[idx];
        int cx = current.first;
        int cy = current.second;

        q[idx] = q.back();
        q.pop_back();

        set(cx, cy, GetType());
        ++count;

        UpdateBoundary(cx, cy, q, overwrite, width, height, set, get);
    }

    return count;
}

bool Terrain::CheckBoundary(int x, int y, bool overwrite, int width, int height,
    function<bool(int, int, const string)> set, function<string(int, int)> get) const {
    if (x == 0 || x == width - 1 || y == 0 || y == height - 1) return true;
    for (int i = 0; i < 4; ++i) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        if (overwrite) {
            if (get(nx, ny) != GetType()) return true;
        }
        else {
            if (get(nx, ny) == "plain") return true;
        }
    }
    return false;
}

void Terrain::UpdateBoundary(int x, int y, vector<pair<int, int>>& q, bool overwrite, int width, int height,
    function<bool(int, int, const string)> set, function<string(int, int)> get) const {
    for (int i = 0; i < 4; ++i) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        if (nx >= 0 && nx < width &&
            ny >= 0 && ny < height &&
            CheckBoundary(nx, ny, overwrite, width, height, set, get)) {
            bool overlap = false;
            for (auto b : q) {
                if (b.first == nx && b.second == ny)overlap = true;
            }
            if (!overlap)q.push_back(make_pair(nx, ny));
        }
    }
}

void Terrain::ShapeFilter(int x, int y, int width, int height,
    function<bool(int, int, const string)> set, function<string(int, int)> get, int side, float threshold) const {
    if (get(x, y) == GetType())return;

    int count = 0;
    for (int i = x - side; i <= x + side; i++) {
        for (int j = y - side; j <= y + side; j++) {
            if (get(i, j) == GetType())count++;
        }
    }
    if (count > (side * 2 + 1) * (side * 2 + 1) * threshold)set(x, y, GetType());
}

void TerrainFactory::RegisterTerrain(const string& id, function<unique_ptr<Terrain>()> creator) {
    registries[id] = creator;
}

unique_ptr<Terrain> TerrainFactory::CreateTerrain(const string& id) const {
    if(configs.find(id) == configs.end() || !configs.find(id)->second)return nullptr;
    
    auto it = registries.find(id);
    if (it != registries.end()) {
        return it->second();
    }
    return nullptr;
}

bool TerrainFactory::CheckRegistered(const string& id) const {
    return registries.find(id) != registries.end();
}

void TerrainFactory::SetConfig(string name, bool config) {
    configs[name] = config;
}

vector<unique_ptr<Terrain>> TerrainFactory::GetTerrains() const {
    vector<unique_ptr<Terrain>> terrains;
    for (auto r : registries) {
        if (configs.find(r.first)->second) {
            terrains.push_back(r.second());
        }
    }
    return terrains;
}


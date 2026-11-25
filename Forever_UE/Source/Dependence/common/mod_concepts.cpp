#include "mod_concepts.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>


using namespace std;

namespace {
	const vector<ModConceptDescriptor> kModConceptDescriptors = {
		{ "Terrains",      "GetModTerrains",      "RegisterModTerrains",      "FinishModTerrains" },
		{ "Roadnets",      "GetModRoadnets",      "RegisterModRoadnets",      "FinishModRoadnets" },
		{ "Zones",         "GetModZones",         "RegisterModZones",         "FinishModZones" },
		{ "Greens",        "GetModGreens",        "RegisterModGreens",        "FinishModGreens" },
		{ "Buildings",     "GetModBuildings",     "RegisterModBuildings",     "FinishModBuildings" },
		{ "Components",    "GetModComponents",    "RegisterModComponents",    "FinishModComponents" },
		{ "Rooms",         "GetModRooms",         "RegisterModRooms",         "FinishModRooms" },
		{ "Names",         "GetModNames",         "RegisterModNames",         "FinishModNames" },
		{ "Schedulers",    "GetModSchedulers",    "RegisterModSchedulers",    "FinishModSchedulers" },
		{ "Jobs",          "GetModJobs",          "RegisterModJobs",          "FinishModJobs" },
		{ "Organizations", "GetModOrganizations", "RegisterModOrganizations", "FinishModOrganizations" },
		{ "Scripts",       "GetModScripts",       "RegisterModScripts",       "FinishModScripts" },
		{ "Products",      "GetModProducts",      "RegisterModProducts",      "FinishModProducts" },
		{ "Storages",      "GetModStorages",      "RegisterModStorages",      "FinishModStorages" },
		{ "Manufactures",  "GetModManufactures",  "RegisterModManufactures",  "FinishModManufactures" },
		{ "Vehicles",      "GetModVehicles",      "RegisterModVehicles",      "FinishModVehicles" },
		{ "Stations",      "GetModStations",      "RegisterModStations",      "FinishModStations" },
		{ "Routes",        "GetModRoutes",        "RegisterModRoutes",        "FinishModRoutes" },
		{ "Assets",        "GetModAssets",        "RegisterModAssets",        "FinishModAssets" },
		{ "Puzzles",       "GetModPuzzles",       "RegisterModPuzzles",       "FinishModPuzzles" },
		{ "Apps",          "GetModApps",          "RegisterModApps",          "FinishModApps" },
		{ "Weapons",       "GetModWeapons",       "RegisterModWeapons",       "FinishModWeapons" },
		{ "Parties",       "GetModParties",       "RegisterModParties",       "FinishModParties" },
	};
}

const vector<ModConceptDescriptor>& GetModConceptDescriptors() {
	return kModConceptDescriptors;
}

const ModConceptDescriptor* FindModConceptDescriptor(const string& conceptKey) {
	for (const ModConceptDescriptor& descriptor : kModConceptDescriptors) {
		if (conceptKey == descriptor.conceptKey) return &descriptor;
	}
	return nullptr;
}

string ModConceptJsonKey(const string& conceptKey) {
	string singular;
	if (conceptKey.size() > 3 && conceptKey.compare(conceptKey.size() - 3, 3, "ies") == 0) {
		singular = conceptKey.substr(0, conceptKey.size() - 3) + "y";
	}
	else {
		singular = conceptKey.empty() ? conceptKey : conceptKey.substr(0, conceptKey.size() - 1);
	}
	for (char& c : singular) {
		c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
	}
	return singular + "_mods";
}

namespace {
	// 只读文件的PE解析，给ProbeModDll用。全程只是读字节：不LoadLibrary，dll里的代码(DllMain、全局构造)一行都不执行。
	// 只认64位(x64的PE32+)：32位dll本来也加载不进64位进程。字段偏移见微软的PE格式文档。
	class PeReader {
	public:
		explicit PeReader(const string& path) : file(path, ios::binary) {}

		bool Open() {
			if (!file) return false;
			uint16_t mz = 0, machine = 0, sectionCount = 0, optionalSize = 0, magic = 0;
			uint32_t peOffset = 0, signature = 0, directoryCount = 0;
			if (!ReadFile(0, &mz, 2) || mz != 0x5A4D) return false; // "MZ"
			if (!ReadFile(0x3C, &peOffset, 4) || !ReadFile(peOffset, &signature, 4) || signature != 0x00004550) return false; // "PE\0\0"
			if (!ReadFile(peOffset + 4, &machine, 2) || !ReadFile(peOffset + 6, &sectionCount, 2) ||
				!ReadFile(peOffset + 20, &optionalSize, 2)) return false;
			const uint64_t optional = static_cast<uint64_t>(peOffset) + 24;
			if (!ReadFile(optional, &magic, 2) || machine != 0x8664 || magic != 0x20B) return false;
			if (!ReadFile(optional + 108, &directoryCount, 4)) return false;
			directoryCount = min<uint32_t>(directoryCount, 16);
			for (uint32_t i = 0; i < directoryCount; i++) {
				uint32_t entry[2] = { 0, 0 }; // RVA, Size
				if (!ReadFile(optional + 112 + i * 8ull, entry, 8)) return false;
				directories.push_back({ entry[0], entry[1] });
			}
			if (sectionCount > 96) return false; // PE规定的上限
			for (uint16_t i = 0; i < sectionCount; i++) {
				uint32_t header[10] = {}; // Name(8字节), VirtualSize, VirtualAddress, SizeOfRawData, PointerToRawData, ...
				if (!ReadFile(optional + optionalSize + i * 40ull, header, 40)) return false;
				sections.push_back({ header[3], header[4], header[5] });
			}
			return true;
		}

		// 数据目录(0=导出表，1=导入表)的RVA，没有返回0。
		uint32_t Directory(size_t index) const {
			return index < directories.size() && directories[index].second > 0 ? directories[index].first : 0;
		}

		// 按RVA读：先换算成文件偏移(落在哪个节的原始数据里)。
		bool Read(uint32_t rva, void* out, size_t size) {
			for (const Section& section : sections) {
				if (rva < section.virtualAddress) continue;
				const uint64_t delta = static_cast<uint64_t>(rva) - section.virtualAddress;
				if (delta + size > section.rawSize) continue;
				return ReadFile(section.rawOffset + delta, out, size);
			}
			return false;
		}

		// 读一个以0结尾的字符串(符号名/dll名)，最长255字节，更长的截断。
		bool ReadString(uint32_t rva, string& out) {
			out.clear();
			for (const Section& section : sections) {
				if (rva < section.virtualAddress) continue;
				const uint64_t delta = static_cast<uint64_t>(rva) - section.virtualAddress;
				if (delta >= section.rawSize) continue;
				char buffer[256] = {};
				const size_t size = static_cast<size_t>(min<uint64_t>(sizeof(buffer) - 1, section.rawSize - delta));
				if (!ReadFile(section.rawOffset + delta, buffer, size)) return false;
				out.assign(buffer, strnlen(buffer, size));
				return true;
			}
			return false;
		}

	private:
		struct Section { uint32_t virtualAddress, rawSize, rawOffset; };

		bool ReadFile(uint64_t offset, void* out, size_t size) {
			file.clear();
			file.seekg(static_cast<streamoff>(offset), ios::beg);
			file.read(static_cast<char*>(out), static_cast<streamsize>(size));
			return file.gcount() == static_cast<streamsize>(size);
		}

		ifstream file;
		vector<pair<uint32_t, uint32_t>> directories;
		vector<Section> sections;
	};

	// 调试版CRT的dll：名字是Release版后面加个d(msvcp140d.dll/vcruntime140d.dll/vcruntime140_1d.dll/ucrtbased.dll)。
	bool IsDebugRuntime(string name) {
		for (char& c : name) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
		if (name.size() < 6 || name.compare(name.size() - 5, 5, "d.dll") != 0) return false;
		for (const char* prefix : { "msvcp", "vcruntime", "ucrtbase", "concrt" }) {
			if (name.compare(0, strlen(prefix), prefix) == 0) return true;
		}
		return false;
	}
}

ModDllProbe ProbeModDll(const string& path) {
	ModDllProbe probe;
	PeReader pe(path);
	if (!pe.Open()) return probe;
	probe.readable = true;

	// ---- 导出表：有没有任何一个GetMod<Concepts> ----
	// 导出名字表按名字升序排(链接器保证，加载器靠它二分查找)，我们的符号都以"GetMod"开头：二分找到第一个>= "GetMod"的名字，
	// 再往后看以它开头的那几个。这样导出几万个符号的无关大dll也只读十几次文件。
	if (const uint32_t exportRva = pe.Directory(0)) {
		uint32_t directory[10] = {}; // ..., NumberOfNames在+24, AddressOfNames在+32
		if (pe.Read(exportRva, directory, 40)) {
			const uint32_t nameCount = directory[6], namesRva = directory[8];
			auto nameAt = [&](uint32_t index, string& out) {
				uint32_t nameRva = 0;
				return pe.Read(namesRva + index * 4, &nameRva, 4) && pe.ReadString(nameRva, out);
			};
			const string prefix = "GetMod";
			uint32_t low = 0, high = nameCount;
			string name;
			bool ok = true;
			while (ok && low < high) {
				const uint32_t middle = low + (high - low) / 2;
				ok = nameAt(middle, name);
				if (ok && name < prefix) low = middle + 1;
				else high = middle;
			}
			for (uint32_t index = low; ok && index < nameCount && !probe.isMod; index++) {
				if (!nameAt(index, name) || name.compare(0, prefix.size(), prefix) != 0) break;
				for (const ModConceptDescriptor& descriptor : kModConceptDescriptors) {
					if (name == descriptor.getModSymbol) { probe.isMod = true; break; }
				}
			}
		}
	}

	// ---- 导入表：有没有调试版CRT ----
	if (const uint32_t importRva = pe.Directory(1)) {
		for (uint32_t i = 0; i < 4096; i++) {
			uint32_t descriptor[5] = {}; // 每条20字节，Name(dll名的RVA)在+12；全0结束
			if (!pe.Read(importRva + i * 20, descriptor, 20) || descriptor[3] == 0) break;
			string name;
			if (pe.ReadString(descriptor[3], name) && IsDebugRuntime(name)) { probe.isDebug = true; break; }
		}
	}
	return probe;
}

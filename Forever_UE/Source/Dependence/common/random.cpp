#include "common/random.h"

#include "common/mod_args.h"
#include "common/utility.h"

#include <cstdlib>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <unordered_map>


using namespace std;

namespace {
	RandomHost ownHost;
	RandomHost* currentHost = &ownHost;

	struct Registry {
		bool hasGlobalSeed = false;
		uint64_t globalSeed = 0;
		map<string, pair<bool, uint64_t>> modSeeds;
		// unique_ptr：流的地址在Reset之前必须稳定(Factory/Core会缓存指针)。
		map<string, unique_ptr<RandomStream>> mods;
		map<string, unique_ptr<RandomStream>> domains;
		unordered_map<const void*, string> instances; // Reset不清：实例比一局活得久
	};
	Registry& Get() {
		static Registry registry;
		return registry;
	}

	uint64_t RandomSeed() {
		random_device device;
		return (static_cast<uint64_t>(device()) << 32) ^ static_cast<uint64_t>(device());
	}

	uint64_t HashDomain(uint64_t seed, const string& name) {
		uint64_t h = 1469598103934665603ULL; // FNV-1a
		for (unsigned char c : name) { h ^= c; h *= 1099511628211ULL; }
		return h ^ (seed * 0x9e3779b97f4a7c15ULL + 0x7f4a7c15ULL);
	}
}

RandomHost* GetRandomHost() { return currentHost; }

void SetRandomHost(RandomHost* host) { currentHost = host ? host : &ownHost; }

void RandomRegistry::Reset() {
	Registry& registry = Get();
	// 流对象可能被宿主缓存指针，Reset只在新开局/读档前调用，此时没有mod代码在跑。
	registry.hasGlobalSeed = false;
	registry.globalSeed = 0;
	registry.modSeeds.clear();
	registry.mods.clear();
	registry.domains.clear();
	if (currentHost) currentHost->current = nullptr;
}

void RandomRegistry::SetGlobalSeed(bool has, uint64_t seed) {
	Registry& registry = Get();
	registry.hasGlobalSeed = has;
	registry.globalSeed = seed;
}

void RandomRegistry::SetModSeed(const string& modId, bool has, uint64_t seed) {
	Get().modSeeds[modId] = { has, seed };
}

RandomStream* RandomRegistry::GetModStream(const string& modId) {
	Registry& registry = Get();
	auto it = registry.mods.find(modId);
	if (it != registry.mods.end()) return it->second.get();
	auto stream = make_unique<RandomStream>();
	auto configured = registry.modSeeds.find(modId);
	const bool fixed = configured != registry.modSeeds.end() && configured->second.first;
	stream->Seed(fixed ? configured->second.second : RandomSeed());
	debugf("Random: mod %s seed=%llu (%s)\n", modId.c_str(), static_cast<unsigned long long>(stream->seed), fixed ? "fixed" : "random");
	RandomStream* raw = stream.get();
	registry.mods[modId] = move(stream);
	return raw;
}

RandomStream* RandomRegistry::GetDomainStream(const string& domain) {
	Registry& registry = Get();
	auto it = registry.domains.find(domain);
	if (it != registry.domains.end()) return it->second.get();
	auto stream = make_unique<RandomStream>();
	stream->Seed(registry.hasGlobalSeed ? HashDomain(registry.globalSeed, domain) : RandomSeed());
	debugf("Random: domain %s seed=%llu (%s)\n", domain.c_str(), static_cast<unsigned long long>(stream->seed),
		registry.hasGlobalSeed ? "derived" : "random");
	RandomStream* raw = stream.get();
	registry.domains[domain] = move(stream);
	return raw;
}

vector<RandomRegistry::Entry> RandomRegistry::Snapshot() {
	Registry& registry = Get();
	vector<Entry> entries;
	auto add = [&entries](const string& prefix, const map<string, unique_ptr<RandomStream>>& streams) {
		for (const auto& [name, stream] : streams) {
			Entry entry;
			entry.key = prefix + name;
			entry.seed = stream->seed;
			for (int i = 0; i < 4; i++) entry.state[i] = stream->s[i];
			entries.push_back(entry);
		}
	};
	add("mod:", registry.mods);
	add("domain:", registry.domains);
	return entries;
}

void RandomRegistry::Restore(const vector<Entry>& entries) {
	for (const Entry& entry : entries) {
		RandomStream* stream = nullptr;
		if (entry.key.rfind("mod:", 0) == 0) stream = GetModStream(entry.key.substr(4));
		else if (entry.key.rfind("domain:", 0) == 0) stream = GetDomainStream(entry.key.substr(7));
		if (!stream) continue;
		stream->seed = entry.seed;
		for (int i = 0; i < 4; i++) stream->s[i] = entry.state[i];
	}
}

void RandomRegistry::BindInstance(const void* instance, const string& modId) {
	if (instance) Get().instances[instance] = modId;
}

void RandomRegistry::UnbindInstance(const void* instance) {
	Get().instances.erase(instance);
}

RandomStream* RandomRegistry::GetInstanceStream(const void* instance) {
	if (!instance) return nullptr;
	Registry& registry = Get();
	auto it = registry.instances.find(instance);
	return it == registry.instances.end() ? nullptr : GetModStream(it->second);
}

uint64_t RandomRegistry::UnscopedDraws() { return currentHost ? currentHost->unscopedDraws : 0; }

string RandomRegistry::ExtractSeed(const string& args, bool& hasSeed, uint64_t& seed) {
	hasSeed = false;
	seed = 0;
	ModArgs parsed(args);
	if (!parsed.Has("seed")) return args; // 没有--seed：参数串原样返回(不动引号里的空白)
	string text = parsed.GetString("seed");
	char* end = nullptr;
	unsigned long long value = strtoull(text.c_str(), &end, 10);
	if (text.empty() || end == text.c_str() || *end != '\0') return args; // 值不是数字：当没写，原样留给mod
	bool found = false;
	string removed;
	string rest = ModArgs::Remove(args, "seed", found, removed);
	hasSeed = true;
	seed = value;
	return rest;
}

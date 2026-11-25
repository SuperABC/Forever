#pragma once

#include <cstdint>
#include <string>
#include <vector>


// 带种子的随机流(详见random.md)：xoshiro256**，POD、无堆分配——流对象由Core分配，各mod DLL里的GetRandom
// 也会直接操作它，所以不能含std容器/虚函数(跨DLL的CRT/分配器问题)。
struct RandomStream {
	uint64_t s[4] = { 0, 0, 0, 0 };
	uint64_t seed = 0; // 最近一次Seed的值(存档/日志用)

	void Seed(uint64_t newSeed) {
		seed = newSeed;
		uint64_t x = newSeed;
		for (int i = 0; i < 4; i++) s[i] = SplitMix(x);
		if ((s[0] | s[1] | s[2] | s[3]) == 0) s[0] = 1; // 全0是xoshiro的不动点
	}

	uint64_t NextU64() {
		const uint64_t result = Rotl(s[1] * 5, 7) * 9;
		const uint64_t t = s[1] << 17;
		s[2] ^= s[0];
		s[3] ^= s[1];
		s[1] ^= s[2];
		s[0] ^= s[3];
		s[2] ^= t;
		s[3] = Rotl(s[3], 45);
		return result;
	}

	// [0, n-1]，n<=0返回0。Lemire乘法取高位，偏差在n<<2^32时可忽略。
	int Range(int n) {
		if (n <= 0) return 0;
		const uint64_t x = NextU64() >> 32;
		return static_cast<int>((x * static_cast<uint64_t>(n)) >> 32);
	}

	// [0, 1)
	double NextDouble() { return static_cast<double>(NextU64() >> 11) * (1.0 / 9007199254740992.0); }

private:
	static uint64_t Rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
	static uint64_t SplitMix(uint64_t& x) {
		uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
		z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
		return z ^ (z >> 31);
	}
};

// 当前流的共享槽：Core持有唯一一份，加载每个mod DLL时通过它导出的BindRandomHost把指针交给该DLL自己那份
// Dependence(每个DLL静态链接各自的Dependence，GetRandom看不到别的模块的全局)。mod单线程调用，不加锁。
struct RandomHost {
	RandomStream* current = nullptr; // 非空：GetRandom从这条流取；空：退回不可控的random_device
	uint64_t unscopedDraws = 0;      // 没有作用域时的抽取次数(排查漏包用)
};

// 本模块当前使用的host：默认指向本模块自己的静态host，SetRandomHost可换成Core的(mod DLL加载时)。
RandomHost* GetRandomHost();
void SetRandomHost(RandomHost* host);

// 把当前流切到stream，析构时还原成之前的(栈式，可嵌套)。stream为空=什么都不做(沿用外层)。
class RandomScope {
public:
	explicit RandomScope(RandomStream* stream) : host(GetRandomHost()), previous(host->current) {
		if (stream) host->current = stream;
	}
	~RandomScope() { host->current = previous; }
	RandomScope(const RandomScope&) = delete;
	RandomScope& operator=(const RandomScope&) = delete;

private:
	RandomHost* host;
	RandomStream* previous;
};

// 由宿主(Core)维护的流表：mod流按mod id(只用id做键，不带concept：两个concept注册了同名id会共用一条流)，Core域流按域名("core.map"等)。
// 只有宿主模块(Core所在)那份会被用到——Factory/Core都在宿主里调它，mod DLL不碰。
class RandomRegistry {
public:
	// 新开局/读档前重置：清空所有流和已登记的种子。
	static void Reset();

	// 登记种子(未登记=首次取流时用random_device)；必须在取流之前调用。
	static void SetGlobalSeed(bool has, uint64_t seed);
	static void SetModSeed(const std::string& modId, bool has, uint64_t seed);

	// 取流(没有就按登记的种子/随机创建并记日志)。指针在Reset之前稳定。
	static RandomStream* GetModStream(const std::string& modId);
	static RandomStream* GetDomainStream(const std::string& domain); // "core.map"...

	struct Entry {
		std::string key;   // "mod:<id>" 或 "domain:<name>"
		uint64_t seed = 0;
		uint64_t state[4] = { 0, 0, 0, 0 };
	};
	// 存档：当前所有已创建的流；读档：把存档的状态覆盖到同key的流上(流不存在就按存档创建)。
	static std::vector<Entry> Snapshot();
	static void Restore(const std::vector<Entry>& entries);

	// 排查：没有作用域的抽取次数(本模块host)。
	static uint64_t UnscopedDraws();

	// mod实例 -> mod id：各Factory在Create/Destroy时登记，宿主包装类(Terrain/Roadnet/...)调mod虚函数前用实例取流。
	// 实例没登记(空指针/未知)返回nullptr，RandomScope对nullptr是空操作。
	static void BindInstance(const void* instance, const std::string& modId);
	static void UnbindInstance(const void* instance);
	static RandomStream* GetInstanceStream(const void* instance);

	// 解析并剥离参数串里的"--seed N"：返回剥离后的参数串，hasSeed/seed给出解析结果。
	static std::string ExtractSeed(const std::string& args, bool& hasSeed, uint64_t& seed);
};

// Core域级入口用：在函数开头写 CORE_RANDOM_SCOPE("core.map"); 整个函数里的GetRandom都取该域的流。
#define CORE_RANDOM_SCOPE(domain) RandomScope coreRandomScope(RandomRegistry::GetDomainStream(domain))

// mod DLL导出：Core加载DLL后调用，把宿主的host指针交给DLL自己那份Dependence。
// 每个mod DLL的入口cpp写一行 FOREVER_MOD_BIND_RANDOM 即可。
#define FOREVER_MOD_BIND_RANDOM \
	extern "C" __declspec(dllexport) void BindRandomHost(RandomHost* host) { SetRandomHost(host); }

inline constexpr const char* kBindRandomHostSymbol = "BindRandomHost";

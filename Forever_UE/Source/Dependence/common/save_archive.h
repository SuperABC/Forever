#pragma once

#include <cstdint>
#include <string>
#include <vector>


// 存档文件外层格式版本(文件头里紧跟magic的那个号)——管外壳布局：头字段、压缩方式、段表结构。
// 外层头(magic "FEVR" + 这个版本 + 未压缩长度 + 压缩体)由UE层写，因为压缩用UE的FCompression；
// 这里只放常量和"压缩体"内部的分段容器。见save_archive.md"版本号"一节。
constexpr int SAVE_FILE_VERSION = 1;
// 当前代码还能读的最旧外层版本，比它还旧的存档直接拒绝。
constexpr int SAVE_FILE_MIN_VERSION = 1;
// 外层头的magic，4字节。
constexpr char SAVE_FILE_MAGIC[4] = { 'F', 'E', 'V', 'R' };

// 一个命名段：小结构用JSON文本(kind==Json)，大数组用紧凑二进制(kind==Binary)。version是这个段
// 自己的数据布局版本(段版本)，由写这个段的域维护，见Source/Core/common/save.h的版本表。
struct SaveSection {
	enum class Kind : uint8_t { Json = 0, Binary = 1 };

	std::string name;
	int version = 0;
	Kind kind = Kind::Binary;
	std::vector<uint8_t> data;
};

// 存档"压缩体"内部的分段容器：段表+若干命名段，纯C++、不依赖压缩库。只在宿主侧(Core/UE)使用，
// mod不直接接触(mod只通过mod_state.h的StateWriter/StateReader读写自己的数据)。
class SaveArchive {
public:
	// 同名段会被替换。
	void SetSection(SaveSection section);
	void SetJson(const std::string& name, int version, const std::string& jsonText);
	void SetBinary(const std::string& name, int version, std::vector<uint8_t> data);

	// 找不到返回nullptr。
	const SaveSection* Find(const std::string& name) const;
	const std::vector<SaveSection>& GetSections() const;

	// 序列化成字节流 / 从字节流解析。解析失败返回false并给出原因。
	std::vector<uint8_t> Serialize() const;
	bool Parse(const uint8_t* data, size_t size, std::string& error);

private:
	std::vector<SaveSection> sections;
};

// 二进制段的读写小工具：小端序，定长整数/浮点/字符串(长度前缀)。读越界时置failed、后续读返回0。
class ByteWriter {
public:
	void U8(uint8_t v);
	void U16(uint16_t v);
	void U32(uint32_t v);
	void I32(int32_t v);
	void U64(uint64_t v);
	void I64(int64_t v);
	void F32(float v);
	void F64(double v);
	void Str(const std::string& v); // u32长度+字节
	void Bytes(const void* data, size_t size);

	std::vector<uint8_t>& Data();
	std::vector<uint8_t> Take();

private:
	std::vector<uint8_t> buffer;
};

class ByteReader {
public:
	ByteReader(const uint8_t* data, size_t size);
	explicit ByteReader(const std::vector<uint8_t>& data);

	uint8_t U8();
	uint16_t U16();
	uint32_t U32();
	int32_t I32();
	uint64_t U64();
	int64_t I64();
	float F32();
	double F64();
	std::string Str();
	bool Bytes(void* out, size_t size);

	bool Failed() const;
	bool AtEnd() const;
	size_t Remaining() const;

private:
	bool Take(void* out, size_t size);

	const uint8_t* data;
	size_t size;
	size_t offset = 0;
	bool failed = false;
};

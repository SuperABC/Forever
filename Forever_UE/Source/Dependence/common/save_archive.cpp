#include "save_archive.h"

#include <cstring>


using namespace std;

void SaveArchive::SetSection(SaveSection section) {
	for (SaveSection& existing : sections) {
		if (existing.name == section.name) {
			existing = move(section);
			return;
		}
	}
	sections.push_back(move(section));
}

void SaveArchive::SetJson(const string& name, int version, const string& jsonText) {
	SaveSection section;
	section.name = name;
	section.version = version;
	section.kind = SaveSection::Kind::Json;
	section.data.assign(jsonText.begin(), jsonText.end());
	SetSection(move(section));
}

void SaveArchive::SetBinary(const string& name, int version, vector<uint8_t> data) {
	SaveSection section;
	section.name = name;
	section.version = version;
	section.kind = SaveSection::Kind::Binary;
	section.data = move(data);
	SetSection(move(section));
}

const SaveSection* SaveArchive::Find(const string& name) const {
	for (const SaveSection& section : sections) {
		if (section.name == name) return &section;
	}
	return nullptr;
}

const vector<SaveSection>& SaveArchive::GetSections() const {
	return sections;
}

vector<uint8_t> SaveArchive::Serialize() const {
	ByteWriter writer;
	writer.U32(static_cast<uint32_t>(sections.size()));
	for (const SaveSection& section : sections) {
		writer.Str(section.name);
		writer.I32(section.version);
		writer.U8(static_cast<uint8_t>(section.kind));
		writer.U64(section.data.size());
		writer.Bytes(section.data.data(), section.data.size());
	}
	return writer.Take();
}

bool SaveArchive::Parse(const uint8_t* data, size_t size, string& error) {
	sections.clear();
	ByteReader reader(data, size);
	uint32_t count = reader.U32();
	for (uint32_t i = 0; i < count && !reader.Failed(); i++) {
		SaveSection section;
		section.name = reader.Str();
		section.version = reader.I32();
		uint8_t kind = reader.U8();
		if (kind > static_cast<uint8_t>(SaveSection::Kind::Binary)) {
			error = "unknown section kind in section '" + section.name + "'";
			return false;
		}
		section.kind = static_cast<SaveSection::Kind>(kind);
		uint64_t length = reader.U64();
		if (reader.Failed() || length > reader.Remaining()) {
			error = "section '" + section.name + "' is truncated";
			return false;
		}
		section.data.resize(static_cast<size_t>(length));
		reader.Bytes(section.data.data(), section.data.size());
		sections.push_back(move(section));
	}
	if (reader.Failed()) {
		error = "save archive is truncated";
		return false;
	}
	return true;
}

// ---- ByteWriter ----

void ByteWriter::U8(uint8_t v) { buffer.push_back(v); }
void ByteWriter::U16(uint16_t v) { for (int i = 0; i < 2; i++) buffer.push_back(static_cast<uint8_t>(v >> (8 * i))); }
void ByteWriter::U32(uint32_t v) { for (int i = 0; i < 4; i++) buffer.push_back(static_cast<uint8_t>(v >> (8 * i))); }
void ByteWriter::I32(int32_t v) { U32(static_cast<uint32_t>(v)); }
void ByteWriter::U64(uint64_t v) { for (int i = 0; i < 8; i++) buffer.push_back(static_cast<uint8_t>(v >> (8 * i))); }
void ByteWriter::I64(int64_t v) { U64(static_cast<uint64_t>(v)); }
void ByteWriter::F32(float v) { uint32_t bits; memcpy(&bits, &v, 4); U32(bits); }
void ByteWriter::F64(double v) { uint64_t bits; memcpy(&bits, &v, 8); U64(bits); }
void ByteWriter::Str(const string& v) { U32(static_cast<uint32_t>(v.size())); Bytes(v.data(), v.size()); }
void ByteWriter::Bytes(const void* data, size_t size) {
	const uint8_t* p = static_cast<const uint8_t*>(data);
	buffer.insert(buffer.end(), p, p + size);
}
vector<uint8_t>& ByteWriter::Data() { return buffer; }
vector<uint8_t> ByteWriter::Take() { return move(buffer); }

// ---- ByteReader ----

ByteReader::ByteReader(const uint8_t* data, size_t size) : data(data), size(size) {}
ByteReader::ByteReader(const vector<uint8_t>& data) : data(data.data()), size(data.size()) {}

bool ByteReader::Take(void* out, size_t n) {
	if (failed || n > size - offset) {
		failed = true;
		memset(out, 0, n);
		return false;
	}
	memcpy(out, data + offset, n);
	offset += n;
	return true;
}

uint8_t ByteReader::U8() { uint8_t b[1]; Take(b, 1); return b[0]; }
uint16_t ByteReader::U16() { uint8_t b[2]; Take(b, 2); return static_cast<uint16_t>(b[0] | (b[1] << 8)); }
uint32_t ByteReader::U32() {
	uint8_t b[4]; Take(b, 4);
	return static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) | (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
}
int32_t ByteReader::I32() { return static_cast<int32_t>(U32()); }
uint64_t ByteReader::U64() { uint64_t lo = U32(); uint64_t hi = U32(); return lo | (hi << 32); }
int64_t ByteReader::I64() { return static_cast<int64_t>(U64()); }
float ByteReader::F32() { uint32_t bits = U32(); float v; memcpy(&v, &bits, 4); return v; }
double ByteReader::F64() { uint64_t bits = U64(); double v; memcpy(&v, &bits, 8); return v; }
string ByteReader::Str() {
	uint32_t n = U32();
	if (failed || n > size - offset) { failed = true; return string(); }
	string s(reinterpret_cast<const char*>(data + offset), n);
	offset += n;
	return s;
}
bool ByteReader::Bytes(void* out, size_t n) { return Take(out, n); }
bool ByteReader::Failed() const { return failed; }
bool ByteReader::AtEnd() const { return offset >= size; }
size_t ByteReader::Remaining() const { return size - offset; }

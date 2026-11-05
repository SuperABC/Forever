#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>


// 零依赖PNG写出器(deflate用"存储块"，不压缩)——验证用截图足够，文件大一点无所谓。
namespace png {
	inline uint32_t Crc32(const uint8_t* data, size_t size) {
		static uint32_t table[256];
		static bool ready = false;
		if (!ready) {
			for (uint32_t i = 0; i < 256; i++) {
				uint32_t c = i;
				for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
				table[i] = c;
			}
			ready = true;
		}
		uint32_t crc = 0xFFFFFFFFu;
		for (size_t i = 0; i < size; i++) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
		return ~crc;
	}

	inline void PutBE32(std::vector<uint8_t>& out, uint32_t v) {
		out.push_back(static_cast<uint8_t>(v >> 24)); out.push_back(static_cast<uint8_t>(v >> 16));
		out.push_back(static_cast<uint8_t>(v >> 8)); out.push_back(static_cast<uint8_t>(v));
	}

	inline void Chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& body) {
		PutBE32(out, static_cast<uint32_t>(body.size()));
		std::vector<uint8_t> tagged(type, type + 4);
		tagged.insert(tagged.end(), body.begin(), body.end());
		out.insert(out.end(), tagged.begin(), tagged.end());
		PutBE32(out, Crc32(tagged.data(), tagged.size()));
	}

	// bgra：width*height*4字节，写成RGB的PNG。
	inline bool Write(const std::string& path, const uint8_t* bgra, int width, int height) {
		std::vector<uint8_t> raw;
		raw.reserve(static_cast<size_t>(width * 3 + 1) * height);
		for (int y = 0; y < height; y++) {
			raw.push_back(0);
			for (int x = 0; x < width; x++) {
				const uint8_t* p = bgra + (static_cast<size_t>(y) * width + x) * 4;
				raw.push_back(p[2]); raw.push_back(p[1]); raw.push_back(p[0]);
			}
		}

		std::vector<uint8_t> z = { 0x78, 0x01 };
		uint32_t a = 1, b = 0;
		for (uint8_t byte : raw) { a = (a + byte) % 65521; b = (b + a) % 65521; }
		size_t pos = 0;
		while (true) {
			size_t len = std::min<size_t>(65535, raw.size() - pos);
			bool last = pos + len >= raw.size();
			z.push_back(last ? 1 : 0);
			z.push_back(static_cast<uint8_t>(len & 0xFF)); z.push_back(static_cast<uint8_t>(len >> 8));
			z.push_back(static_cast<uint8_t>(~len & 0xFF)); z.push_back(static_cast<uint8_t>((~len >> 8) & 0xFF));
			z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + len);
			pos += len;
			if (last) break;
		}
		PutBE32(z, (b << 16) | a);

		std::vector<uint8_t> out = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
		std::vector<uint8_t> ihdr;
		PutBE32(ihdr, width); PutBE32(ihdr, height);
		ihdr.push_back(8); ihdr.push_back(2); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
		Chunk(out, "IHDR", ihdr);
		Chunk(out, "IDAT", z);
		Chunk(out, "IEND", {});

		FILE* file = nullptr;
		if (fopen_s(&file, path.c_str(), "wb") != 0 || !file) return false;
		fwrite(out.data(), 1, out.size(), file);
		fclose(file);
		return true;
	}
}

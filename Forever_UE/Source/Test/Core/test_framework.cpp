#include "test_framework.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>


using namespace std;

TestRun& TestRun::Get() {
	static TestRun run;
	return run;
}

void TestRun::Begin(const string& suiteName, unsigned long long seedValue, const string& reproCommand) {
	Reset();
	suite = suiteName;
	seed = seedValue;
	repro = reproCommand;
	printf("==== [%s]%s ====\n", suite.c_str(), seed ? (" seed=" + to_string(seed)).c_str() : "");
}

void TestRun::Reset() {
	checks.clear();
	metrics.clear();
	notes.clear();
}

void TestRun::Check(bool passed, const string& name, const string& detail) {
	checks.push_back({ name, passed, detail });
	if (!passed) printf("  [FAIL] %s%s%s\n", name.c_str(), detail.empty() ? "" : "  <- ", detail.c_str());
}

void TestRun::Metric(const string& name, double value, const string& unit) {
	metrics.push_back({ name, value, unit });
	printf("  [metric] %s = %.3f %s\n", name.c_str(), value, unit.c_str());
}

void TestRun::Note(const string& line) {
	notes.push_back(line);
	printf("  %s\n", line.c_str());
}

int TestRun::Failures() const {
	int count = 0;
	for (const TestCheck& check : checks) if (!check.passed) count++;
	return count;
}

int TestRun::Passes() const {
	return static_cast<int>(checks.size()) - Failures();
}

bool TestRun::Write(const string& dir) const {
	error_code error;
	filesystem::create_directories(dir, error);
	{
		ofstream out(filesystem::path(dir) / (suite + ".json"), ios::binary);
		if (!out) return false;
		out << "{\n  \"suite\": \"" << JsonEscape(suite) << "\",\n  \"seed\": " << seed
			<< ",\n  \"repro\": \"" << JsonEscape(repro) << "\",\n  \"passed\": " << Passes()
			<< ",\n  \"failed\": " << Failures() << ",\n  \"checks\": [\n";
		for (size_t i = 0; i < checks.size(); i++) {
			const TestCheck& c = checks[i];
			out << "    {\"name\": \"" << JsonEscape(c.name) << "\", \"passed\": " << (c.passed ? "true" : "false")
				<< ", \"detail\": \"" << JsonEscape(c.detail) << "\"}" << (i + 1 < checks.size() ? "," : "") << "\n";
		}
		out << "  ],\n  \"metrics\": [\n";
		for (size_t i = 0; i < metrics.size(); i++) {
			const TestMetric& m = metrics[i];
			out << "    {\"name\": \"" << JsonEscape(m.name) << "\", \"value\": " << m.value << ", \"unit\": \""
				<< JsonEscape(m.unit) << "\"}" << (i + 1 < metrics.size() ? "," : "") << "\n";
		}
		out << "  ]\n}\n";
	}
	{
		ofstream out(filesystem::path(dir) / (suite + ".txt"), ios::binary);
		if (!out) return false;
		out << "suite " << suite << "  seed " << seed << "  repro: " << repro << "\n";
		out << "passed " << Passes() << "  failed " << Failures() << "\n";
		for (const TestCheck& c : checks) if (!c.passed) out << "[FAIL] " << c.name << "  <- " << c.detail << "\n";
		for (const TestMetric& m : metrics) out << "[metric] " << m.name << " = " << m.value << " " << m.unit << "\n";
		for (const string& note : notes) out << note << "\n";
	}
	return true;
}

namespace {
	string OneLine(const string& text) {
		string out = text;
		for (char& c : out) if (c == '\t' || c == '\n' || c == '\r') c = ' ';
		return out;
	}
}

bool TestRun::Export(const string& path) const {
	ofstream out(path, ios::binary);
	if (!out) return false;
	for (const TestCheck& c : checks) out << "check\t" << (c.passed ? 1 : 0) << "\t" << OneLine(c.name) << "\t" << OneLine(c.detail) << "\n";
	for (const TestMetric& m : metrics) out << "metric\t" << OneLine(m.name) << "\t" << m.value << "\t" << OneLine(m.unit) << "\n";
	for (const string& n : notes) out << "note\t" << OneLine(n) << "\n";
	return true;
}

void TestRun::Import(const string& path, const string& tag) {
	ifstream in(path);
	string line;
	while (getline(in, line)) {
		vector<string> parts;
		string part;
		stringstream ss(line);
		while (getline(ss, part, '\t')) parts.push_back(part);
		if (parts.empty()) continue;
		if (parts[0] == "check" && parts.size() >= 3) Check(parts[1] == "1", parts[2] + " " + tag, parts.size() >= 4 ? parts[3] : "");
		else if (parts[0] == "metric" && parts.size() >= 3) Metric(parts[1] + " " + tag, atof(parts[2].c_str()), parts.size() >= 4 ? parts[3] : "");
		else if (parts[0] == "note" && parts.size() >= 2) notes.push_back(parts[1]);
	}
}

void TestPrintLine(const char* line) {
	TestRun::Get().Note(line ? line : "");
}

string JsonEscape(const string& text) {
	string out;
	for (unsigned char c : text) {
		switch (c) {
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (c < 0x20) { char buf[8]; snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; }
			else out += static_cast<char>(c);
		}
	}
	return out;
}

unsigned long long Fnv1a(const void* data, size_t size, unsigned long long seed) {
	const unsigned char* bytes = static_cast<const unsigned char*>(data);
	unsigned long long hash = seed;
	for (size_t i = 0; i < size; i++) {
		hash ^= bytes[i];
		hash *= 1099511628211ULL;
	}
	return hash;
}

string HexString(unsigned long long value) {
	char buf[24];
	snprintf(buf, sizeof(buf), "%016llx", value);
	return buf;
}

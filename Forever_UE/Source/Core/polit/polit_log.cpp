#include "polit/polit_log.h"

#include "common/utility.h"

#include <cstdio>


using namespace std;

namespace {
	PolitLogSink& Sink() {
		static PolitLogSink sink;
		return sink;
	}
}

void SetPolitLogSink(PolitLogSink sink) {
	Sink() = std::move(sink);
}

bool HasPolitLogSink() {
	return static_cast<bool>(Sink());
}

void PolitLog(const Time& now, const string& category, const string& body) {
	if (!Sink()) return;
	string line = "[polit] " + now.Format("YYYY-MM-DD HH:mm") + " " + category;
	if (!body.empty()) line += " " + body;
	Sink()(line);
}

string PolitNum(double value) {
	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%.3f", value);
	return buffer;
}

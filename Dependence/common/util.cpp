#include "util.h"

#include <cmath>
#include <random>


using namespace std;

void debugf(LPCSTR format, ...) {
	va_list args;
	va_start(args, format);
	CHAR buf[1024] = { 0 };
	StringCchVPrintfA(buf, 1023, format, args);
	OutputDebugStringA(buf);
}

int GetRandom(int range) {
	if (range <= 0)return 0;

	mt19937 rng(random_device{}());
	uniform_int_distribution<int> dist(0, range - 1);
	int ret = dist(rng);

	return ret;
}
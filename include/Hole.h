#pragma once

struct Hole {
	int baseAddress;
	int limit;

	Hole(int base, int lim) : baseAddress(base), limit(lim) {}
};
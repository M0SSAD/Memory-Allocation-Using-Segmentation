#pragma once

#include <stack>
#include <string>
#include <vector>

struct segment {
	std::string name;
	int baseAddress = -1;
	int limit = 0;
};

struct Process {
	inline static int count = 0;
	int pid = count++;
	int numberOfSegments;
	std::vector<segment> segments;

	Process() {}
	Process(int numberOfSegments, std::vector<segment> segs)
	    : numberOfSegments(numberOfSegments), segments(segs) {}
};
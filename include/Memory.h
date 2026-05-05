#pragma once

#include "Hole.h"
#include "Process.h"
#include <Record.h>
#include <list>
#include <memory>
#include <variant>

enum methodOfAllocation {
	FF, // First Fit
	BF, // Best Fit
};

// For drawing Memory Layout in the GUI.
struct MemoryBlock {
	int baseAddress;
	int limit;
	bool isFree;
	std::string label;
};

// for checking the allocation and deallocation.
struct ErrorData {
	std::string message;
};

class Memory {
  private:
	int size;
	std::list<Hole> holes; // list of the free holes in the memory
	std::vector<std::unique_ptr<Process>>
	    processes; // Pointer to processes objects.
	methodOfAllocation
	    allocationAlgorithm; // which algorithm to use while allocating.

  public:
	Memory(int s, std::list<Hole> c, methodOfAllocation alg)
	    : size(s), holes(c), allocationAlgorithm(alg) {}
	std::variant<bool, ErrorData> allocate(std::unique_ptr<Process> p);
	std::variant<bool, ErrorData> deallocate(int target_pid);
	std::vector<MemoryBlock> getMemorySnapshot() const;
	ErrorData rollback(std::stack<UndoRecord> &ledger, int failed_limit);
	std::variant<Process *, ErrorData>
	getProcess(int pid) const; // To get the process to draw the
	                           // Segment table on the gui
};
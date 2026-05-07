#include <Memory.h>
#include <Record.h>
#include <algorithm>

// function to revert the holes to the previous state if the process addition
// fails.
ErrorData Memory::rollback(std::stack<UndoRecord> &ledger, int failed_limit) {
    while (!ledger.empty()) {
        UndoRecord rec = ledger.top();
        ledger.pop();

        auto freed_base = rec.base_address;
        auto freed_limit = rec.allocated_size;
        
		// search for the place where this segment should have been.
        auto hole_it = holes.begin();
        while (hole_it != holes.end() && hole_it->baseAddress < freed_base) {
            hole_it++;
        }

		// insert the hole.
        auto new_hole = holes.insert(hole_it, {freed_base, freed_limit});

		// now check if it can merge with a hole before it or a hole after it.
        auto next_hole = std::next(new_hole);
        if (next_hole != holes.end() && (new_hole->baseAddress + new_hole->limit == next_hole->baseAddress)) {
            new_hole->limit += next_hole->limit; 
            holes.erase(next_hole);              
        }

        if (new_hole != holes.begin()) {
            auto prev_hole = std::prev(new_hole);
            if (prev_hole->baseAddress + prev_hole->limit == new_hole->baseAddress) {
                prev_hole->limit += new_hole->limit;   
                holes.erase(new_hole); 
            }
        }
    }
    
    return ErrorData{"There is no free space in memory that can contain " +
                     std::to_string(failed_limit) + "."};
}

std::variant<bool, ErrorData> Memory::allocate(std::unique_ptr<Process> p) {
	std::stack<UndoRecord> ledger;

	for (int i = 0; i < p->segments.size(); i++) {
		// Choose the algorithm that we will use for allocating.
		if (allocationAlgorithm == FF) {
			// find the first fit in holes
			auto it = std::find_if(
			    holes.begin(), holes.end(), [&p, i](const Hole &hole) {
				    return hole.limit >= p->segments[i].limit;
			    });
			// if not found roll back to the previous state and remove all the
			// added segments of this process.
			if (it == holes.end()) {
				return rollback(ledger, p->segments[i].limit);
			} else {
				// update the state to reflect the addition of the current
				// segment.
				p->segments[i].baseAddress = it->baseAddress;
				it->baseAddress += p->segments[i].limit;
				it->limit -= p->segments[i].limit;
				if (it->limit == 0) {
					holes.erase(it);
				}
				ledger.push({p->segments[i].baseAddress, p->segments[i].limit});
			}
		} else if (allocationAlgorithm == BF) {
			auto best_it = holes.end();
			for (auto it = holes.begin(); it != holes.end(); ++it) {
				if (it->limit >= p->segments[i].limit) {
					if (best_it == holes.end() || it->limit < best_it->limit) {
						best_it = it;
					}
				}
			}
			if (best_it == holes.end()) {
				return rollback(ledger, p->segments[i].limit);
			} else {
				// update the state to reflect the addition of the current
				// segment.
				p->segments[i].baseAddress = best_it->baseAddress;
				best_it->baseAddress += p->segments[i].limit;
				best_it->limit -= p->segments[i].limit;
				if (best_it->limit == 0) {
					holes.erase(best_it);
				}
				ledger.push({p->segments[i].baseAddress, p->segments[i].limit});
			}
		}
	}
	processes.push_back(std::move(p));
	return true;
}
std::variant<bool, ErrorData> Memory::deallocate(int target_pid) {
	auto it = std::find_if(processes.begin(), processes.end(),
	                       [target_pid](const std::unique_ptr<Process> &p) {
		                       return p->pid == target_pid;
	                       });

	if (it == processes.end()) {
		return ErrorData{"Process ID " + std::to_string(target_pid) +
		                 " not found."};
	}

	Process *p = it->get();

	for (int i = 0; i < p->segments.size(); i++) {
		auto freed_base = p->segments[i].baseAddress;
		auto freed_limit = p->segments[i].limit;
		auto hole_it = holes.begin();
		while (hole_it != holes.end() &&
		       hole_it->baseAddress < p->segments[i].baseAddress) {
			hole_it++;
		}

		auto new_hole = holes.insert(hole_it, {freed_base, freed_limit});

		// Forward Merge Check
		auto next_hole = std::next(new_hole);
		if (next_hole != holes.end() &&
		    (new_hole->baseAddress + new_hole->limit ==
		     next_hole->baseAddress)) {
			new_hole->limit += next_hole->limit; // stretch the size
			holes.erase(next_hole);              // Delete the redundant node
		}

		// Backward Merge Check
		if (new_hole != holes.begin()) {
			auto prev_hole = std::prev(new_hole);
			if (prev_hole->baseAddress + prev_hole->limit ==
			    new_hole->baseAddress) {
				prev_hole->limit +=
				    new_hole->limit;   // stretch the size backwards
				holes.erase(new_hole); // Delete the redundant node
			}
		}
	}

	if (it != processes.end() - 1) {
		*it = std::move(processes.back());
	}
	processes.pop_back();

	return true;
}
std::vector<MemoryBlock> Memory::getMemorySnapshot() const {
	std::vector<MemoryBlock> raw_snapshot;

	for (const auto &hole : holes) {
		raw_snapshot.push_back({hole.baseAddress, hole.limit, true, "Hole"});
	}

	// Gather all allocated segments from active processes
	for (const auto &p : processes) {
		for (const auto &seg : p->segments) {
			if (seg.baseAddress != -1) {
				std::string label =
				    "P" + std::to_string(p->pid) + ": " + seg.name;
				raw_snapshot.push_back(
				    {seg.baseAddress, seg.limit, false, label});
			}
		}
	}

	std::sort(raw_snapshot.begin(), raw_snapshot.end(),
	          [](const MemoryBlock &a, const MemoryBlock &b) {
		          return a.baseAddress < b.baseAddress;
	          });

	std::vector<MemoryBlock> final_snapshot;
	int expected_address = 0;

	for (const auto &block : raw_snapshot) {
		// The Gap Check & Fill
		if (block.baseAddress > expected_address) {
			int gap_limit = block.baseAddress - expected_address;
			final_snapshot.push_back(
			    {expected_address, gap_limit, false, "Reserved"});
		}

		// Push the actual block (Hole or Segment)
		final_snapshot.push_back(block);

		// The Update
		expected_address = block.baseAddress + block.limit;
	}

	// The Tail End Check
	if (expected_address < size) {
		int tail_limit = size - expected_address;
		final_snapshot.push_back(
		    {expected_address, tail_limit, false, "Reserved"});
	}

	return final_snapshot;
}

std::variant<Process *, ErrorData> Memory::getProcess(int target_pid) const {
	auto it = std::find_if(processes.begin(), processes.end(),
	                       [target_pid](const std::unique_ptr<Process> &p) {
		                       return p->pid == target_pid;
	                       });
	if (it == processes.end()) {
		return ErrorData{"Process ID " + std::to_string(target_pid) +
		                 " not found."};
	}
	Process *p = it->get();
	return p;
}

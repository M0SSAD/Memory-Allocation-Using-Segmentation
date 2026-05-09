# Memory Allocation Simulator

This is a C++ program that simulates how an Operating System manages computer memory (RAM). It uses segmentation to place processes into memory and shows a visual map of the physical RAM.

## Features

* **Algorithms:** Uses First-Fit and Best-Fit to find space in memory.
* **Visual Map:** Draws the memory in real-time using Dear ImGui.
* **Safe Memory:** Uses modern C++ pointers to stop memory leaks.
* **Rollback System:** If a process needs 3 parts and only 2 fit, the system cancels the whole process so the memory does not break.

## Requirements

To build this project, you need:
1. A C++ Compiler (like MSVC for Windows, or GCC for Linux)
2. CMake (version 3.15 or higher)
3. An internet connection (CMake will download the visual tools automatically)

## How to Build

Open your terminal or command prompt in the project folder and type these commands:

1. Make a build folder:
   `mkdir build`

2. Go into the folder:
   `cd build`

3. Setup the project:
   `cmake ..`

4. Build the program:
   `cmake --build . --config Release`

## How to Run

After building, go to the `build` folder (or the `build/Release` folder on Windows). You will find the executable file there. Double-click it or run it from the terminal to start the simulator.
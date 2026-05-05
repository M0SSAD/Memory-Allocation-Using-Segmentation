#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "Memory.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <memory>
#include <string>
#include <vector>

enum class AppState { INIT, DASHBOARD };

static AppState g_state = AppState::INIT;
static std::unique_ptr<Memory> g_memory;

static int g_totalMemory = 1024;
static int g_algoIdx = 0;
static const char *g_algoNames[] = {"First-Fit", "Best-Fit"};

struct HoleInput {
	int baseAddress = 0;
	int size = 100;
};
static std::vector<HoleInput> g_holeInputs;

struct SegInput {
	char name[64] = "";
	int size = 100;
};

static int g_numSegs = 1;
static std::vector<SegInput> g_segs(1);
static std::vector<int> g_pids;
static char g_err[512] = "";
static bool g_hasErr = false;
static float g_dpiScale = 1.0f;

static constexpr float L_INIT_COL_W = 600.0f;
static constexpr float L_DASH_PANEL_W = 400.0f;
static constexpr float L_BTN_BIG = 38.0f;
static constexpr float L_BTN_MED = 30.0f;
static constexpr float L_MAP_PAD = 10.0f;
static constexpr float L_MAP_TEXT_THRESH = 12.0f;
static constexpr float L_FONT_BASE = 16.0f;
static constexpr float L_WIN_RATIO = 0.75f;

static float S(float base) { return base * g_dpiScale; }

static constexpr ImU32 C_FREE = IM_COL32(144, 238, 144, 255);
static constexpr ImU32 C_RESERVED = IM_COL32(64, 64, 64, 255);
static constexpr ImU32 C_BORDER = IM_COL32(30, 30, 30, 255);
static constexpr ImU32 C_TEXT_D = IM_COL32(0, 0, 0, 255);
static constexpr ImU32 C_TEXT_L = IM_COL32(255, 255, 255, 255);

static const ImU32 C_PROC[] = {
    IM_COL32(70, 130, 180, 255),  IM_COL32(255, 140, 0, 255),
    IM_COL32(147, 112, 219, 255), IM_COL32(205, 92, 92, 255),
    IM_COL32(0, 191, 255, 255),   IM_COL32(218, 165, 32, 255),
    IM_COL32(186, 85, 211, 255),  IM_COL32(60, 179, 113, 255),
};
static constexpr int NC = sizeof(C_PROC) / sizeof(C_PROC[0]);

static ImU32 blockColor(const MemoryBlock &b) {
	if (b.isFree)
		return C_FREE;
	if (b.label == "Reserved")
		return C_RESERVED;
	int pid = 0;
	if (b.label.size() > 1 && b.label[0] == 'P')
		pid = std::atoi(b.label.c_str() + 1);
	return C_PROC[((unsigned)pid) % NC];
}

static void glfwErr(int e, const char *d) {
	fprintf(stderr, "GLFW %d: %s\n", e, d);
}

static void renderInit() {
	ImGuiIO &io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
	ImGui::SetNextWindowSize(io.DisplaySize);
	ImGui::Begin("##setup", nullptr,
	             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
	                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
	                 ImGuiWindowFlags_NoScrollbar |
	                 ImGuiWindowFlags_NoScrollWithMouse);

	float colW = S(L_INIT_COL_W);
	float winW = io.DisplaySize.x;
	float padX = (winW - colW) * 0.5f;
	if (padX < S(20))
		padX = S(20);

	ImGui::SetCursorPosX(padX);
	ImGui::BeginGroup();

	ImGui::SetCursorPosX(
	    padX +
	    (colW - ImGui::CalcTextSize("Memory Allocation Simulator").x) * 0.5f);
	ImGui::Text("Memory Allocation Simulator");
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::Text("Total Memory Size");
	ImGui::SetNextItemWidth(colW);
	ImGui::InputInt("##totalmem", &g_totalMemory);

	ImGui::Spacing();

	ImGui::Text("Algorithm");
	ImGui::SetNextItemWidth(colW);
	ImGui::Combo("##algo", &g_algoIdx, g_algoNames, 2);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::Text("Initial Holes");
	ImGui::Spacing();

	float bottomSpace = S(L_BTN_BIG) + S(L_BTN_MED) + S(40);
	float holeListH = ImGui::GetContentRegionAvail().y - bottomSpace;
	if (holeListH < S(100))
		holeListH = S(100);

	if (ImGui::BeginChild("##holes", ImVec2(colW, holeListH),
	                      ImGuiChildFlags_Border)) {
		for (int i = 0; i < (int)g_holeInputs.size(); i++) {
			ImGui::PushID(i);

			ImGui::Text("Hole %d", i + 1);
			ImGui::SameLine();
			bool removed = false;
			ImGui::PushStyleColor(ImGuiCol_Button,
			                      ImVec4(0.55f, 0.15f, 0.15f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
			                      ImVec4(0.75f, 0.25f, 0.25f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive,
			                      ImVec4(0.85f, 0.35f, 0.35f, 1.0f));
			if (ImGui::SmallButton("Remove")) {
				g_holeInputs.erase(g_holeInputs.begin() + i);
				removed = true;
			}
			ImGui::PopStyleColor(3);

			if (!removed) {
				ImGui::Text("Starting Address");
				ImGui::SetNextItemWidth(colW - S(16));
				ImGui::InputInt("##addr", &g_holeInputs[i].baseAddress);

				ImGui::Text("Size");
				ImGui::SetNextItemWidth(colW - S(16));
				ImGui::InputInt("##size", &g_holeInputs[i].size);
			}

			if (i < (int)g_holeInputs.size() - 1)
				ImGui::Separator();
			ImGui::PopID();
			if (removed)
				break;
		}
	}
	ImGui::EndChild();

	ImGui::Spacing();
	if (ImGui::Button("+ Add Hole", ImVec2(colW, S(L_BTN_MED)))) {
		int nextBase = 0;
		for (const auto &h : g_holeInputs) {
			int end = h.baseAddress + h.size;
			if (end > nextBase)
				nextBase = end;
		}
		if (nextBase >= g_totalMemory)
			nextBase = 0;
		g_holeInputs.push_back(HoleInput{nextBase, 100});
	}

	ImGui::Spacing();

	if (g_hasErr) {
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", g_err);
		ImGui::Spacing();
	}

	if (ImGui::Button("Start Simulator", ImVec2(colW, S(L_BTN_BIG)))) {
		g_hasErr = false;

		if (g_totalMemory <= 0) {
			snprintf(g_err, sizeof(g_err), "Total memory must be > 0.");
			g_hasErr = true;
		} else if (g_holeInputs.empty()) {
			snprintf(g_err, sizeof(g_err), "Add at least one hole.");
			g_hasErr = true;
		} else {
			bool valid = true;
			for (int i = 0; i < (int)g_holeInputs.size(); i++) {
				auto &h = g_holeInputs[i];
				if (h.baseAddress < 0) {
					snprintf(g_err, sizeof(g_err),
					         "Hole %d: starting address must be >= 0.", i + 1);
					g_hasErr = true;
					valid = false;
					break;
				}
				if (h.size <= 0) {
					snprintf(g_err, sizeof(g_err), "Hole %d: size must be > 0.",
					         i + 1);
					g_hasErr = true;
					valid = false;
					break;
				}
				if (h.baseAddress + h.size > g_totalMemory) {
					snprintf(g_err, sizeof(g_err),
					         "Hole %d: exceeds total memory boundary.", i + 1);
					g_hasErr = true;
					valid = false;
					break;
				}
			}

			if (valid) {
				std::sort(g_holeInputs.begin(), g_holeInputs.end(),
				          [](const HoleInput &a, const HoleInput &b) {
					          return a.baseAddress < b.baseAddress;
				          });
				for (int i = 1; i < (int)g_holeInputs.size(); i++) {
					int prevEnd = g_holeInputs[i - 1].baseAddress +
					              g_holeInputs[i - 1].size;
					if (g_holeInputs[i].baseAddress < prevEnd) {
						snprintf(g_err, sizeof(g_err),
						         "Holes overlap detected at address %d.",
						         g_holeInputs[i].baseAddress);
						g_hasErr = true;
						valid = false;
						break;
					}
				}
			}

			if (valid) {
				for (int i = 0; i < (int)g_holeInputs.size() - 1;) {
					if (g_holeInputs[i].baseAddress + g_holeInputs[i].size ==
					    g_holeInputs[i + 1].baseAddress) {
						g_holeInputs[i].size += g_holeInputs[i + 1].size;
						g_holeInputs.erase(g_holeInputs.begin() + i + 1);
					} else {
						i++;
					}
				}

				std::list<Hole> holes;
				for (auto &h : g_holeInputs)
					holes.push_back(Hole(h.baseAddress, h.size));
				g_memory = std::make_unique<Memory>(g_totalMemory, holes,
				                                    g_algoIdx == 0 ? FF : BF);
				g_state = AppState::DASHBOARD;
			}
		}
	}

	ImGui::EndGroup();
	ImGui::End();
}

static void renderPanel() {
	ImGui::BeginChild("##panel", ImVec2(S(L_DASH_PANEL_W), 0),
	                  ImGuiChildFlags_Border);

	if (ImGui::Button("New Simulation", ImVec2(-1, S(L_BTN_MED)))) {
		g_memory.reset();
		g_pids.clear();
		g_numSegs = 1;
		g_segs.assign(1, SegInput());
		g_hasErr = false;
		g_holeInputs = {};
		g_totalMemory = 1024;
		g_algoIdx = 0;
		Process::count = 0;
		g_state = AppState::INIT;
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (ImGui::CollapsingHeader("Add Process",
	                            ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Spacing();

		ImGui::Text("Number of Segments");
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::InputInt("##numsegs", &g_numSegs)) {
			if (g_numSegs < 1)
				g_numSegs = 1;
			if (g_numSegs > 20)
				g_numSegs = 20;
			g_segs.resize(g_numSegs);
		}

		ImGui::Separator();

		for (int i = 0; i < g_numSegs; i++) {
			ImGui::PushID(i);
			ImGui::Text("Segment %d", i + 1);
			ImGui::Text("Name");
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputText("##name", g_segs[i].name, sizeof(SegInput::name));
			ImGui::Text("Size");
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputInt("##size", &g_segs[i].size);
			if (i < g_numSegs - 1)
				ImGui::Separator();
			ImGui::PopID();
		}

		ImGui::Spacing();

		if (ImGui::Button("Allocate", ImVec2(-1, S(L_BTN_MED)))) {
			g_hasErr = false;
			bool valid = true;
			for (int i = 0; i < g_numSegs; i++) {
				if (g_segs[i].size <= 0) {
					snprintf(g_err, sizeof(g_err),
					         "Segment %d: size must be > 0.", i + 1);
					g_hasErr = true;
					valid = false;
					break;
				}
			}

			if (valid) {
				auto p = std::make_unique<Process>();
				std::vector<segment> sv;
				for (int i = 0; i < g_numSegs; i++) {
					std::string nm = g_segs[i].name[0]
					                     ? std::string(g_segs[i].name)
					                     : ("Seg" + std::to_string(i));
					sv.push_back({nm, -1, g_segs[i].size});
				}
				p->numberOfSegments = g_numSegs;
				p->segments = sv;
				int pid = p->pid;
				auto r = g_memory->allocate(std::move(p));
				if (std::holds_alternative<bool>(r)) {
					g_pids.push_back(pid);
					g_numSegs = 1;
					g_segs.assign(1, SegInput());
					g_hasErr = false;
				} else {
					snprintf(g_err, sizeof(g_err), "%s",
					         std::get<ErrorData>(r).message.c_str());
					g_hasErr = true;
				}
			}
		}

		if (g_hasErr) {
			ImGui::Spacing();
			ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s",
			                   g_err);
		}
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (ImGui::CollapsingHeader("Active Processes",
	                            ImGuiTreeNodeFlags_DefaultOpen)) {
		if (g_pids.empty())
			ImGui::TextDisabled("No active processes.");

		for (int i = 0; i < (int)g_pids.size(); i++) {
			int pid = g_pids[i];
			ImGui::PushID(i);

			bool open = ImGui::TreeNode("##proc", "P%d", pid);

			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button,
			                      ImVec4(0.55f, 0.15f, 0.15f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
			                      ImVec4(0.75f, 0.25f, 0.25f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive,
			                      ImVec4(0.85f, 0.35f, 0.35f, 1.0f));

			bool killed = false;
			if (ImGui::SmallButton("Kill")) {
				auto r = g_memory->deallocate(pid);
				if (std::holds_alternative<bool>(r)) {
					g_pids.erase(g_pids.begin() + i);
					killed = true;
				} else {
					snprintf(g_err, sizeof(g_err), "%s",
					         std::get<ErrorData>(r).message.c_str());
					g_hasErr = true;
				}
			}
			ImGui::PopStyleColor(3);

			if (open) {
				if (!killed) {
					auto pr = g_memory->getProcess(pid);
					if (std::holds_alternative<Process *>(pr)) {
						Process *proc = std::get<Process *>(pr);
						if (ImGui::BeginTable("##segs", 3,
						                      ImGuiTableFlags_Borders |
						                          ImGuiTableFlags_RowBg)) {
							ImGui::TableSetupColumn("Segment");
							ImGui::TableSetupColumn("Base");
							ImGui::TableSetupColumn("Limit");
							ImGui::TableHeadersRow();
							for (const auto &s : proc->segments) {
								ImGui::TableNextRow();
								ImGui::TableNextColumn();
								ImGui::TextUnformatted(s.name.c_str());
								ImGui::TableNextColumn();
								ImGui::Text("%d", s.baseAddress);
								ImGui::TableNextColumn();
								ImGui::Text("%d", s.limit);
							}
							ImGui::EndTable();
						}
					}
				}
				ImGui::TreePop();
			}

			ImGui::PopID();
			if (killed)
				i--;
		}
	}

	ImGui::EndChild();
}

static void renderMap() {
	ImGui::SameLine();
	ImGui::BeginChild("##map", ImVec2(0, 0), ImGuiChildFlags_Border);

	ImGui::Text("Memory Map   |   Algorithm: %s   |   Total: %d bytes",
	            g_algoNames[g_algoIdx], g_totalMemory);
	ImGui::Separator();

	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImVec2 avail = ImGui::GetContentRegionAvail();
	float pad = S(L_MAP_PAD);
	float dx = pos.x + pad;
	float dy = pos.y + pad;
	float dw = avail.x - pad * 2;
	float dh = avail.y - pad * 2;

	if (dw > 0.0f && dh > 0.0f && g_memory) {
		ImDrawList *dl = ImGui::GetWindowDrawList();
		auto snap = g_memory->getMemorySnapshot();

		dl->AddRectFilled(ImVec2(dx, dy), ImVec2(dx + dw, dy + dh),
		                  IM_COL32(20, 20, 20, 255));

		float cy = dy;
		for (size_t i = 0; i < snap.size(); i++) {
			const auto &b = snap[i];
			float bh = ((float)b.limit / (float)g_totalMemory) * dh;
			if (i == snap.size() - 1)
				bh = (dy + dh) - cy;

			if (bh < 0.5f)
				bh = 0.5f;

			ImU32 col = blockColor(b);
			ImVec2 bmin(dx, cy);
			ImVec2 bmax(dx + dw, cy + bh);

			dl->AddRectFilled(bmin, bmax, col);
			dl->AddRect(bmin, bmax, C_BORDER);

			if (bh > S(L_MAP_TEXT_THRESH)) {
				char txt[256];
				snprintf(txt, sizeof(txt), "%s [%d-%d]", b.label.c_str(),
				         b.baseAddress, b.baseAddress + b.limit);
				ImVec2 ts = ImGui::CalcTextSize(txt);
				ImU32 tc = (col == C_FREE) ? C_TEXT_D : C_TEXT_L;

				if (ts.x < dw - 8.0f && ts.y < bh - 4.0f) {
					dl->AddText(ImVec2(dx + (dw - ts.x) * 0.5f,
					                   cy + (bh - ts.y) * 0.5f),
					            tc, txt);
				} else {
					snprintf(txt, sizeof(txt), "%s", b.label.c_str());
					ts = ImGui::CalcTextSize(txt);
					if (ts.x < dw - 8.0f && ts.y < bh - 4.0f)
						dl->AddText(ImVec2(dx + (dw - ts.x) * 0.5f,
						                   cy + (bh - ts.y) * 0.5f),
						            tc, txt);
				}
			}

			cy += bh;
		}
	}

	ImGui::Dummy(avail);
	ImGui::EndChild();
}

static void renderDashboard() {
	ImGuiIO &io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
	ImGui::SetNextWindowSize(io.DisplaySize);
	ImGui::Begin("##dashboard", nullptr,
	             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
	                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
	                 ImGuiWindowFlags_NoScrollbar |
	                 ImGuiWindowFlags_NoScrollWithMouse);

	renderPanel();
	renderMap();

	ImGui::End();
}

int main() {
	glfwSetErrorCallback(glfwErr);
	if (!glfwInit())
		return 1;

	GLFWmonitor *primaryMon = glfwGetPrimaryMonitor();
	int monX, monY, monW, monH;
	glfwGetMonitorWorkarea(primaryMon, &monX, &monY, &monW, &monH);
	int winW = (int)(monW * L_WIN_RATIO);
	int winH = (int)(monH * L_WIN_RATIO);

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	GLFWwindow *window = glfwCreateWindow(
	    winW, winH, "Memory Allocation Simulator", nullptr, nullptr);
	if (!window) {
		glfwTerminate();
		return 1;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.IniFilename = nullptr;

	ImGuiStyle &style = ImGui::GetStyle();
	style.WindowRounding = 5.0f;
	style.FrameRounding = 3.0f;
	style.GrabRounding = 2.0f;
	style.WindowPadding = ImVec2(12, 12);
	style.FramePadding = ImVec2(8, 4);
	style.ItemSpacing = ImVec2(8, 6);
	ImGui::StyleColorsDark();

	float contentScale = 1.0f;
	glfwGetWindowContentScale(window, &contentScale, nullptr);

	if (contentScale <= 1.01f) {
		const GLFWvidmode *mode = glfwGetVideoMode(primaryMon);
		int mmW, mmH;
		glfwGetMonitorPhysicalSize(primaryMon, &mmW, &mmH);
		if (mmW > 0 && mode) {
			float dpi = mode->width / (mmW / 25.4f);
			contentScale = dpi / 96.0f;
			if (contentScale < 1.0f)
				contentScale = 1.0f;
		}
	}

	g_dpiScale = contentScale;

	{
		io.Fonts->Clear();
		const char *fontPath = "/usr/share/fonts/noto/NotoSans-Regular.ttf";
		float fontSize = L_FONT_BASE * g_dpiScale;
		ImFont *font = io.Fonts->AddFontFromFileTTF(fontPath, fontSize);
		if (!font) {
			ImFontConfig fallbackCfg;
			fallbackCfg.SizePixels = fontSize;
			io.Fonts->AddFontDefault(&fallbackCfg);
		}
		io.Fonts->Build();
	}

	if (g_dpiScale > 1.01f)
		ImGui::GetStyle().ScaleAllSizes(g_dpiScale);

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 130");

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		if (g_state == AppState::INIT)
			renderInit();
		else
			renderDashboard();

		ImGui::Render();
		int fw, fh;
		glfwGetFramebufferSize(window, &fw, &fh);
		glViewport(0, 0, fw, fh);
		glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(window);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}

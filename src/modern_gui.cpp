#include "gui_window.h"

#include "checklist_ui_model.h"
#include "imgui.h"
#include "imgui_impl_opengl2.h"
#include "plugin_dl.h"
#include "utils.h"

#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMUtilities.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

bool g_initialized = false;
float g_text_scale = 1.0f;
float g_sidebar_width = 180.0f;
bool g_settings_open = false;
XPLMWindowID g_window = nullptr;
ImFont* g_body_font = nullptr;
ImFont* g_title_font = nullptr;
int g_last_scrolled_checklist = -1;
int g_last_scrolled_item = -1;
std::chrono::steady_clock::time_point g_last_frame;

struct desktop_bounds_t {
  int left;
  int top;
  int right;
  int bottom;
};

desktop_bounds_t desktop_bounds()
{
  desktop_bounds_t bounds = {0, 0, 0, 0};
  if(XPLMGetScreenBoundsGlobal_ptr){
    XPLMGetScreenBoundsGlobal_ptr(&bounds.left, &bounds.top,
                                  &bounds.right, &bounds.bottom);
  }else{
    XPLMGetScreenSize(&bounds.right, &bounds.top);
  }
  return bounds;
}

void load_ui_preferences()
{
  char* path = pluginPath("Xchecklist-ui.prf");
  if(!path){
    return;
  }
  std::ifstream input(path);
  if(input.is_open()){
    input >> g_text_scale >> g_sidebar_width;
    g_text_scale = std::clamp(g_text_scale, 0.85f, 2.0f);
    g_sidebar_width = std::clamp(g_sidebar_width, 150.0f, 600.0f);
  }
  free(path);
}

void save_ui_preferences()
{
  char* path = pluginPath("Xchecklist-ui.prf");
  if(!path){
    return;
  }
  std::ofstream output(path);
  if(output.is_open()){
    output << g_text_scale << " " << g_sidebar_width << std::endl;
  }
  free(path);
}

void apply_style()
{
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowPadding = ImVec2(18.0f, 16.0f);
  style.FramePadding = ImVec2(12.0f, 9.0f);
  style.ItemSpacing = ImVec2(10.0f, 10.0f);
  style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
  style.ScrollbarSize = 12.0f;
  style.WindowRounding = 9.0f;
  style.ChildRounding = 7.0f;
  style.FrameRounding = 6.0f;
  style.GrabRounding = 6.0f;
  style.WindowBorderSize = 1.0f;
  style.ChildBorderSize = 1.0f;

  ImVec4* colors = style.Colors;
  colors[ImGuiCol_Text] = ImVec4(0.89f, 0.93f, 0.98f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.46f, 0.54f, 0.64f, 1.00f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.035f, 0.060f, 0.085f, 0.98f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.050f, 0.080f, 0.110f, 0.96f);
  colors[ImGuiCol_Border] = ImVec4(0.19f, 0.27f, 0.35f, 0.85f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.085f, 0.13f, 0.18f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.10f, 0.23f, 0.31f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.08f, 0.32f, 0.44f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.12f, 0.20f, 0.28f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.05f, 0.45f, 0.70f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.03f, 0.35f, 0.58f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.06f, 0.28f, 0.39f, 0.90f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.07f, 0.38f, 0.53f, 1.00f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.05f, 0.46f, 0.66f, 1.00f);
  colors[ImGuiCol_CheckMark] = ImVec4(0.28f, 0.92f, 0.53f, 1.00f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.06f, 0.62f, 0.91f, 1.00f);
  colors[ImGuiCol_Separator] = ImVec4(0.16f, 0.24f, 0.31f, 1.00f);
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.04f, 0.06f, 0.65f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.35f, 0.44f, 0.85f);
}

ImFont* load_first_font(ImGuiIO& io, const std::vector<std::string>& paths,
                        float pixels)
{
  for(const std::string& path : paths){
    if(access(path.c_str(), R_OK) == 0){
      ImFont* font = io.Fonts->AddFontFromFileTTF(path.c_str(), pixels);
      if(font){
        xcDebug("Xchecklist: modern UI loaded font %s at %.0f px\n",
                path.c_str(), pixels);
        return font;
      }
    }
  }
  return nullptr;
}

void ensure_initialized()
{
  if(g_initialized){
    return;
  }
  xcDebug("Xchecklist: modern UI creating ImGui context\n");
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  char system_path[1024] = {};
  XPLMGetSystemPath(system_path);
  const std::string fonts_path = std::string(system_path) + "Resources/fonts/";
  const std::vector<std::string> regular_fonts = {
    fonts_path + "Roboto-Regular.ttf",
    fonts_path + "DejaVuSans.ttf",
    "/usr/share/fonts/open-sans/OpenSans-Regular.ttf",
    "/usr/share/fonts/google-noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf"
  };
  const std::vector<std::string> title_fonts = {
    fonts_path + "Roboto-Bold.ttf",
    "/usr/share/fonts/open-sans/OpenSans-Semibold.ttf",
    "/usr/share/fonts/google-noto/NotoSans-SemiBold.ttf",
    "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Bold.ttf"
  };
  g_body_font = load_first_font(io, regular_fonts, 16.0f);
  g_title_font = load_first_font(io, title_fonts, 21.0f);
  if(!g_body_font){
    g_body_font = io.Fonts->AddFontDefault();
    xcDebug("Xchecklist: modern UI using built-in fallback font\n");
  }
  if(!g_title_font){
    g_title_font = g_body_font;
  }
  io.FontDefault = g_body_font;
  load_ui_preferences();
  apply_style();
  ImGui_ImplOpenGL2_Init();
  g_last_frame = std::chrono::steady_clock::now();
  g_initialized = true;
  xcDebug("Xchecklist: modern UI ImGui context ready\n");
}

void draw_sidebar(float width)
{
  ImGui::BeginChild("##checklist-sidebar", ImVec2(width, 0.0f), true);
  ImGui::TextDisabled("CHECKLISTS");
  ImGui::Spacing();
  const std::vector<std::string>& names = g_checklist_ui_model.checklist_names();
  const std::vector<int>& indexes = g_checklist_ui_model.checklist_indexes();
  for(size_t i = 0; i < names.size(); ++i){
    bool selected = indexes[i] == g_checklist_ui_model.checklist_index();
    float entry_height = std::max(38.0f, ImGui::GetTextLineHeight() + 18.0f);
    if(ImGui::Selectable(names[i].c_str(), selected, 0,
                         ImVec2(0.0f, entry_height))){
      open_checklist(indexes[i]);
    }
    if(g_checklist_ui_model.checklist_completed(indexes[i])){
      ImVec2 row_min = ImGui::GetItemRectMin();
      ImVec2 row_max = ImGui::GetItemRectMax();
      ImVec2 center(row_max.x - 14.0f, (row_min.y + row_max.y) * 0.5f);
      ImDrawList* draw = ImGui::GetWindowDrawList();
      const ImU32 green = IM_COL32(77, 224, 135, 255);
      draw->AddCircle(center, 8.0f, green, 20, 2.0f);
      draw->AddLine(ImVec2(center.x - 4.0f, center.y),
                    ImVec2(center.x - 1.0f, center.y + 3.0f), green, 2.0f);
      draw->AddLine(ImVec2(center.x - 1.0f, center.y + 3.0f),
                    ImVec2(center.x + 5.0f, center.y - 4.0f), green, 2.0f);
    }
  }
  ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(),
                                ImGui::GetWindowHeight() - 100.0f));
  ImGui::Separator();
  ImGui::TextDisabled("TEXT SIZE");
  ImGui::SetNextItemWidth(-1.0f);
  ImGui::SliderFloat("##text-scale", &g_text_scale, 0.85f, 2.0f, "%.2fx");
  bool auto_hide = modern_ui_auto_hide_enabled();
  if(ImGui::Checkbox("Auto-hide", &auto_hide)){
    modern_ui_set_auto_hide_enabled(auto_hide);
  }
  ImGui::Spacing();
  if(ImGui::Button("Complete category", ImVec2(-1.0f, 0.0f))){
    if(complete_checklist()){
      g_checklist_ui_model.set_active(-1);
    }
  }
  ImGui::EndChild();
}

void draw_sidebar_splitter(float available_width)
{
  const float splitter_width = 7.0f;
  const float min_sidebar = 150.0f;
  const float max_sidebar = std::max(min_sidebar, available_width - 360.0f);
  ImGui::SameLine(0.0f, 0.0f);
  ImGui::InvisibleButton("##sidebar-splitter",
                         ImVec2(splitter_width, -1.0f));
  if(ImGui::IsItemActive()){
    g_sidebar_width = std::clamp(g_sidebar_width +
      ImGui::GetIO().MouseDelta.x, min_sidebar, max_sidebar);
  }
  if(ImGui::IsItemHovered() || ImGui::IsItemActive()){
    ImVec2 top = ImGui::GetItemRectMin();
    ImVec2 bottom = ImGui::GetItemRectMax();
    float x = (top.x + bottom.x) * 0.5f;
    ImGui::GetWindowDrawList()->AddLine(
      ImVec2(x, top.y), ImVec2(x, bottom.y),
      IM_COL32(24, 185, 238, 220), 2.0f);
  }
  ImGui::SameLine(0.0f, 0.0f);
}

void draw_checklist_rows()
{
  const std::vector<checklist_ui_item_t>& items = g_checklist_ui_model.items();
  int active = g_checklist_ui_model.active_item();
  int checklist = g_checklist_ui_model.checklist_index();
  const bool active_item_changed = active != g_last_scrolled_item ||
                                   checklist != g_last_scrolled_checklist;
  ImGui::BeginChild("##checklist-items", ImVec2(0.0f, -68.0f), true);
  for(size_t i = 0; i < items.size(); ++i){
    const checklist_ui_item_t& item = items[i];
    if(item.item_void){
      ImGui::Spacing();
      ImGui::TextDisabled("%s", item.text.c_str());
      ImGui::Separator();
      continue;
    }

    bool is_active = static_cast<int>(i) == active;
    ImVec4 row = item.checked ? ImVec4(0.06f, 0.20f, 0.16f, 0.95f) :
                 is_active ? ImVec4(0.06f, 0.25f, 0.34f, 0.98f) :
                             ImVec4(0.07f, 0.105f, 0.14f, 0.98f);
    ImVec4 hovered = is_active ? ImVec4(0.07f, 0.36f, 0.48f, 1.0f) : row;
    ImGui::PushStyleColor(ImGuiCol_Header, row);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hovered);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, hovered);
    char id[32];
    std::snprintf(id, sizeof(id), "##item-%zu", i);
    float row_height = std::max(48.0f, ImGui::GetTextLineHeight() + 28.0f);
    if(ImGui::Selectable(id, false, 0, ImVec2(0.0f, row_height)) && is_active){
      check_item(active);
    }
    ImGui::PopStyleColor(3);

    ImVec2 row_min = ImGui::GetItemRectMin();
    ImVec2 row_max = ImGui::GetItemRectMax();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImU32 status_color = item.checked ? IM_COL32(77, 224, 135, 255) :
                         is_active ? IM_COL32(255, 188, 54, 255) :
                                     IM_COL32(111, 132, 154, 255);
    ImVec2 center(row_min.x + 22.0f, (row_min.y + row_max.y) * 0.5f);
    draw->AddCircle(center, 10.0f, status_color, 24, 2.0f);
    if(item.checked){
      draw->AddLine(ImVec2(center.x - 5.0f, center.y),
                    ImVec2(center.x - 1.0f, center.y + 4.0f), status_color, 2.0f);
      draw->AddLine(ImVec2(center.x - 1.0f, center.y + 4.0f),
                    ImVec2(center.x + 6.0f, center.y - 5.0f), status_color, 2.0f);
    }
    float text_y = row_min.y + (row_max.y - row_min.y -
                                ImGui::GetTextLineHeight()) * 0.5f;
    const float item_font_size = ImGui::GetFontSize();
    draw->AddText(g_body_font, item_font_size,
                  ImVec2(row_min.x + 45.0f, text_y),
                  IM_COL32(230, 238, 248, 255), item.text.c_str());
    ImVec2 suffix_size = ImGui::CalcTextSize(item.suffix.c_str());
    draw->AddText(g_body_font, item_font_size,
                  ImVec2(row_max.x - suffix_size.x - 18.0f, text_y),
                  status_color, item.suffix.c_str());
    if(is_active){
      draw->AddRect(row_min, row_max, IM_COL32(24, 185, 238, 255),
                    6.0f, 0, 2.0f);
      if(active_item_changed){
        ImGui::SetScrollHereY(0.5f);
      }
    }
  }
  ImGui::EndChild();
  if(active_item_changed){
    g_last_scrolled_checklist = checklist;
    g_last_scrolled_item = active;
  }
}

void draw_footer()
{
  float available = ImGui::GetContentRegionAvail().x;
  float toggle_width = ImGui::CalcTextSize("Copilot").x + 54.0f;
  float button_width = std::max(80.0f,
    (available - toggle_width - 30.0f) / 3.0f);
  float button_height = std::max(44.0f, ImGui::GetTextLineHeight() + 22.0f);
  bool can_previous = g_checklist_ui_model.checklist_index() > 0;
  bool can_next = g_checklist_ui_model.checklist_index() + 1 <
                  g_checklist_ui_model.checklist_count();
  bool can_check = g_checklist_ui_model.active_item() >= 0;

  ImGui::BeginDisabled(!can_previous);
  if(ImGui::Button("Previous", ImVec2(button_width, button_height))){
    prev_checklist();
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!can_check);
  if(ImGui::Button("Check item", ImVec2(button_width, button_height))){
    check_item(g_checklist_ui_model.active_item());
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!can_next);
  if(ImGui::Button("Next", ImVec2(button_width, button_height))){
    next_checklist(true);
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  bool copilot = modern_ui_copilot_enabled();
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
  if(ImGui::Checkbox("Copilot", &copilot)){
    modern_ui_set_copilot_enabled(copilot);
  }
}

void draw_settings_page()
{
  ImGui::PushFont(g_title_font, 21.0f * g_text_scale);
  ImGui::TextUnformatted("Settings");
  ImGui::PopFont();
  ImGui::TextDisabled("Changes apply immediately and are saved when you close this page.");
  ImGui::Spacing();
  ImGui::BeginChild("##settings-page", ImVec2(0.0f, -56.0f), true);
  ImGui::TextDisabled("BEHAVIOR");
  bool show_checklist = modern_ui_show_checklist_enabled();
  if(ImGui::Checkbox("Open when a checklist is available", &show_checklist)){
    modern_ui_set_show_checklist_enabled(show_checklist);
  }
  bool copilot = modern_ui_copilot_enabled();
  if(ImGui::Checkbox("Enable copilot automation", &copilot)){
    modern_ui_set_copilot_enabled(copilot);
  }
  bool auto_hide = modern_ui_auto_hide_enabled();
  if(ImGui::Checkbox("Hide completed categories", &auto_hide)){
    modern_ui_set_auto_hide_enabled(auto_hide);
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  ImGui::TextDisabled("ACCESSIBILITY");
  ImGui::TextUnformatted("Text size");
  ImGui::SetNextItemWidth(-1.0f);
  ImGui::SliderFloat("##settings-text-scale", &g_text_scale,
                     0.85f, 2.0f, "%.2fx");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  ImGui::TextDisabled("AUDIO");
#if LIN
  ImGui::BeginDisabled();
  bool voice_available = false;
  ImGui::Checkbox("Voice prompts", &voice_available);
  ImGui::EndDisabled();
  ImGui::TextDisabled("Unavailable while the legacy Linux speech helper is disabled.");
#else
  ImGui::TextDisabled("Voice prompts continue to use the platform speech backend.");
#endif
  ImGui::EndChild();
  ImGui::Spacing();
  if(ImGui::Button("Back to checklist", ImVec2(-1.0f, 44.0f))){
    save_ui_preferences();
    save_prefs();
    g_settings_open = false;
  }
}

void draw_content()
{
  if(g_settings_open){
    draw_settings_page();
    return;
  }

  float available = ImGui::GetContentRegionAvail().x;
  if(available >= 620.0f){
    g_sidebar_width = std::clamp(g_sidebar_width, 150.0f,
      std::max(150.0f, available - 360.0f));
    draw_sidebar(g_sidebar_width);
    draw_sidebar_splitter(available);
  }
  ImGui::BeginGroup();
  ImGui::TextDisabled("XCHECKLIST");
  const float settings_width = ImGui::CalcTextSize("Settings").x + 16.0f;
  const float hide_width = ImGui::CalcTextSize("Hide").x + 16.0f;
  ImGui::SameLine(ImGui::GetContentRegionMax().x - settings_width -
                  hide_width - ImGui::GetStyle().ItemSpacing.x);
  if(ImGui::SmallButton("Settings")){
    g_settings_open = true;
  }
  ImGui::SameLine();
  if(ImGui::SmallButton("Hide") && g_window){
    XPLMSetWindowIsVisible(g_window, 0);
  }
  ImGui::PushFont(g_title_font, 21.0f * g_text_scale);
  ImGui::TextUnformatted(g_checklist_ui_model.title().c_str());
  ImGui::PopFont();
  int completed = g_checklist_ui_model.completed_items();
  int total = g_checklist_ui_model.actionable_items();
  float progress = total > 0 ? static_cast<float>(completed) / total : 0.0f;
  char overlay[32];
  std::snprintf(overlay, sizeof(overlay), "%d of %d", completed, total);
  ImGui::SameLine();
  float status_width = ImGui::CalcTextSize(overlay).x;
  ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - status_width);
  ImGui::TextDisabled("%s", overlay);
  ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
                        ImVec4(0.20f, 0.82f, 0.48f, 1.0f));
  ImGui::ProgressBar(progress, ImVec2(-1.0f, 7.0f), "");
  ImGui::PopStyleColor();
  ImGui::Dummy(ImVec2(0.0f, 7.0f));
  draw_checklist_rows();
  ImGui::Spacing();
  draw_footer();
  ImGui::EndGroup();
}

} // namespace

void modern_ui_show_settings()
{
  g_settings_open = true;
}

void xcvr_draw(XPLMWindowID window, void *refcon)
{
  static bool first_frame = true;
  (void)refcon;
  if(first_frame){
    xcDebug("Xchecklist: modern UI first draw begin\n");
  }
  ensure_initialized();
  g_window = window;
  desktop_bounds_t desktop = desktop_bounds();
  int left, top, right, bottom;
  XPLMGetWindowGeometry(window, &left, &top, &right, &bottom);

  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(static_cast<float>(desktop.right - desktop.left),
                          static_cast<float>(desktop.top - desktop.bottom));
  auto now = std::chrono::steady_clock::now();
  io.DeltaTime = std::max(1.0f / 240.0f,
    std::chrono::duration<float>(now - g_last_frame).count());
  g_last_frame = now;

  XPLMSetGraphicsState(0, 1, 0, 0, 1, 0, 0);
  ImGui_ImplOpenGL2_NewFrame();
  if(first_frame){
    xcDebug("Xchecklist: modern UI renderer frame ready\n");
  }
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(static_cast<float>(left - desktop.left),
    static_cast<float>(desktop.top - top)));
  ImGui::SetNextWindowSize(ImVec2(static_cast<float>(right - left),
                                  static_cast<float>(top - bottom)));
  ImGui::SetNextWindowBgAlpha(0.98f);
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                           ImGuiWindowFlags_NoMove |
                           ImGuiWindowFlags_NoResize |
                           ImGuiWindowFlags_NoSavedSettings |
                           ImGuiWindowFlags_NoBringToFrontOnFocus;
  ImGui::Begin("##xchecklist-modern", nullptr, flags);
  ImGui::PushFont(g_body_font, 16.0f * g_text_scale);
  draw_content();
  ImGui::PopFont();
  ImGui::End();
  ImGui::Render();
  ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
  if(first_frame){
    xcDebug("Xchecklist: modern UI first draw complete\n");
    first_frame = false;
  }
}

int xcvr_handle_mouse(XPLMWindowID window, int x, int y, int status,
                      void *refcon)
{
  (void)window;
  (void)refcon;
  ensure_initialized();
  desktop_bounds_t desktop = desktop_bounds();
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(static_cast<float>(x - desktop.left),
                      static_cast<float>(desktop.top - y));
  if(status == xplm_MouseDown){
    io.AddMouseButtonEvent(0, true);
  }else if(status == xplm_MouseUp){
    io.AddMouseButtonEvent(0, false);
  }
  return 1;
}

int xcvr_handle_wheel(XPLMWindowID window, int x, int y, int wheel,
                      int clicks, void *refcon)
{
  (void)window;
  (void)wheel;
  (void)refcon;
  ensure_initialized();
  desktop_bounds_t desktop = desktop_bounds();
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(static_cast<float>(x - desktop.left),
                      static_cast<float>(desktop.top - y));
  io.AddMouseWheelEvent(0.0f, static_cast<float>(clicks));
  return 1;
}

void xcvr_shutdown_ui()
{
  if(!g_initialized){
    return;
  }
  save_ui_preferences();
  ImGui_ImplOpenGL2_Shutdown();
  ImGui::DestroyContext();
  g_initialized = false;
  g_window = nullptr;
  g_body_font = nullptr;
  g_title_font = nullptr;
  g_last_scrolled_checklist = -1;
  g_last_scrolled_item = -1;
}

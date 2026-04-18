#pragma once

#include "imgui.h"

#include <cstdarg>
#include <cstddef>

// ImGuiWindow — a typed OOP facade over the Dear ImGui free-function API.
//
// Every public method maps 1-to-1 to an ImGui:: function so callers never
// need to touch the raw namespace.  The class is deliberately stateless
// (except for the `IsOpen` flag and optional `Flags` that govern the next
// Begin() call) so it can be embedded as a value member in any window owner.
//
// Usage pattern:
//   class MyPanel {
//       ImGuiWindow m_win;
//   public:
//       void Draw() {
//           m_win.SetNextSize({480, 300});
//           if (!m_win.Begin("My Panel", &m_visible)) return;
//           m_win.Label("Hello, world!");
//           m_win.End();
//       }
//   };
class ImGuiWindow {
public:
	ImGuiWindow();

	// Convenience flag surfaced for callers that build per-window flag combos.
	ImGuiWindowFlags Flags;

	// ── Window lifecycle ──────────────────────────────────────────────────────

	// Maps to ImGui::Begin().  Returns true when the window is visible/expanded.
	// Must always be paired with End(), regardless of the return value.
	bool Begin(const char* title, bool* p_open = nullptr,
		ImGuiWindowFlags flags = ImGuiWindowFlags_None);
	void End();

	// ── Pre-Begin hints (call before Begin each frame) ────────────────────────

	void SetNextPos(ImVec2 pos, ImGuiCond cond = ImGuiCond_Always,
		ImVec2 pivot = ImVec2(0.0f, 0.0f));
	void SetNextSize(ImVec2 size, ImGuiCond cond = ImGuiCond_Always);
	void SetNextSizeConstraints(ImVec2 size_min, ImVec2 size_max,
		ImGuiSizeCallback custom_callback = nullptr, void* custom_callback_data = nullptr);
	void SetNextContentSize(ImVec2 size);
	void SetNextCollapsed(bool collapsed, ImGuiCond cond = ImGuiCond_Always);
	void SetNextFocus();
	void SetNextScroll(ImVec2 scroll);
	void SetNextBgAlpha(float alpha);

	// ── Child windows ─────────────────────────────────────────────────────────

	bool BeginChild(const char* str_id, ImVec2 size = ImVec2(0.0f, 0.0f),
		ImGuiChildFlags child_flags = 0, ImGuiWindowFlags window_flags = 0);
	bool BeginChild(ImGuiID id, ImVec2 size = ImVec2(0.0f, 0.0f),
		ImGuiChildFlags child_flags = 0, ImGuiWindowFlags window_flags = 0);
	void EndChild();

	// ── Window queries ────────────────────────────────────────────────────────

	bool IsWindowAppearing() const;
	bool IsWindowCollapsed() const;
	bool IsWindowFocused(ImGuiFocusedFlags flags = 0) const;
	bool IsWindowHovered(ImGuiHoveredFlags flags = 0) const;
	ImVec2 GetWindowPos() const;
	ImVec2 GetWindowSize() const;
	float GetWindowWidth() const;
	float GetWindowHeight() const;
	ImVec2 GetContentRegionAvail() const;
	ImVec2 GetContentRegionMax() const;
	ImVec2 GetWindowContentRegionMin() const;
	ImVec2 GetWindowContentRegionMax() const;

	// ── Text / labels ─────────────────────────────────────────────────────────

	// Plain formatted text (wraps ImGui::Text).
	void Label(const char* fmt, ...) IM_FMTARGS(2);
	// Non-formatted text — preferred for literal strings (wraps TextUnformatted).
	void LabelUnformatted(const char* text, const char* text_end = nullptr);
	// Colored formatted text.
	void LabelColored(ImVec4 col, const char* fmt, ...) IM_FMTARGS(3);
	// Disabled (greyed-out) formatted text.
	void LabelDisabled(const char* fmt, ...) IM_FMTARGS(2);
	// Word-wrapped formatted text.
	void LabelWrapped(const char* fmt, ...) IM_FMTARGS(2);
	// "label: value" shorthand (wraps ImGui::LabelText).
	void LabelText(const char* label, const char* fmt, ...) IM_FMTARGS(3);
	// Bullet point + formatted text.
	void LabelBullet(const char* fmt, ...) IM_FMTARGS(2);

	// ── Buttons ───────────────────────────────────────────────────────────────

	bool Button(const char* label, ImVec2 size = ImVec2(0.0f, 0.0f));
	bool SmallButton(const char* label);
	bool InvisibleButton(const char* str_id, ImVec2 size, ImGuiButtonFlags flags = 0);
	bool ArrowButton(const char* str_id, ImGuiDir dir);
	bool ImageButton(const char* str_id, ImTextureID texture_id, ImVec2 image_size,
		ImVec2 uv0 = ImVec2(0.0f, 0.0f), ImVec2 uv1 = ImVec2(1.0f, 1.0f),
		ImVec4 bg_col = ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
		ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
	bool RadioButton(const char* label, bool active);
	bool RadioButton(const char* label, int* v, int v_button);
	void ProgressBar(float fraction, ImVec2 size_arg = ImVec2(-FLT_MIN, 0.0f),
		const char* overlay = nullptr);
	void Bullet();

	// ── Checkboxes ───────────────────────────────────────────────────────────

	bool Checkbox(const char* label, bool* v);
	bool CheckboxFlags(const char* label, int* flags, int flags_value);
	bool CheckboxFlags(const char* label, unsigned int* flags, unsigned int flags_value);

	// ── Combo boxes ──────────────────────────────────────────────────────────

	bool BeginCombo(const char* label, const char* preview_value,
		ImGuiComboFlags flags = 0);
	void EndCombo();
	bool Combo(const char* label, int* current_item, const char* const items[],
		int items_count, int popup_max_height_in_items = -1);
	bool Combo(const char* label, int* current_item,
		const char* items_separated_by_zeros, int popup_max_height_in_items = -1);

	// ── Drag widgets ─────────────────────────────────────────────────────────

	bool DragFloat(const char* label, float* v, float v_speed = 1.0f,
		float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f",
		ImGuiSliderFlags flags = 0);
	bool DragFloat2(const char* label, float v[2], float v_speed = 1.0f,
		float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f",
		ImGuiSliderFlags flags = 0);
	bool DragFloat3(const char* label, float v[3], float v_speed = 1.0f,
		float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f",
		ImGuiSliderFlags flags = 0);
	bool DragFloat4(const char* label, float v[4], float v_speed = 1.0f,
		float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f",
		ImGuiSliderFlags flags = 0);
	bool DragFloatRange2(const char* label, float* v_current_min, float* v_current_max,
		float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f,
		const char* format = "%.3f", const char* format_max = nullptr,
		ImGuiSliderFlags flags = 0);
	bool DragInt(const char* label, int* v, float v_speed = 1.0f, int v_min = 0,
		int v_max = 0, const char* format = "%d", ImGuiSliderFlags flags = 0);
	bool DragInt2(const char* label, int v[2], float v_speed = 1.0f, int v_min = 0,
		int v_max = 0, const char* format = "%d", ImGuiSliderFlags flags = 0);
	bool DragInt3(const char* label, int v[3], float v_speed = 1.0f, int v_min = 0,
		int v_max = 0, const char* format = "%d", ImGuiSliderFlags flags = 0);
	bool DragInt4(const char* label, int v[4], float v_speed = 1.0f, int v_min = 0,
		int v_max = 0, const char* format = "%d", ImGuiSliderFlags flags = 0);
	bool DragIntRange2(const char* label, int* v_current_min, int* v_current_max,
		float v_speed = 1.0f, int v_min = 0, int v_max = 0, const char* format = "%d",
		const char* format_max = nullptr, ImGuiSliderFlags flags = 0);
	bool DragScalar(const char* label, ImGuiDataType data_type, void* p_data,
		float v_speed = 1.0f, const void* p_min = nullptr, const void* p_max = nullptr,
		const char* format = nullptr, ImGuiSliderFlags flags = 0);
	bool DragScalarN(const char* label, ImGuiDataType data_type, void* p_data,
		int components, float v_speed = 1.0f, const void* p_min = nullptr,
		const void* p_max = nullptr, const char* format = nullptr,
		ImGuiSliderFlags flags = 0);

	// ── Slider widgets ───────────────────────────────────────────────────────

	bool SliderFloat(const char* label, float* v, float v_min, float v_max,
		const char* format = "%.3f", ImGuiSliderFlags flags = 0);
	bool SliderFloat2(const char* label, float v[2], float v_min, float v_max,
		const char* format = "%.3f", ImGuiSliderFlags flags = 0);
	bool SliderFloat3(const char* label, float v[3], float v_min, float v_max,
		const char* format = "%.3f", ImGuiSliderFlags flags = 0);
	bool SliderFloat4(const char* label, float v[4], float v_min, float v_max,
		const char* format = "%.3f", ImGuiSliderFlags flags = 0);
	bool SliderAngle(const char* label, float* v_rad,
		float v_degrees_min = -360.0f, float v_degrees_max = +360.0f,
		const char* format = "%.0f deg", ImGuiSliderFlags flags = 0);
	bool SliderInt(const char* label, int* v, int v_min, int v_max,
		const char* format = "%d", ImGuiSliderFlags flags = 0);
	bool SliderInt2(const char* label, int v[2], int v_min, int v_max,
		const char* format = "%d", ImGuiSliderFlags flags = 0);
	bool SliderInt3(const char* label, int v[3], int v_min, int v_max,
		const char* format = "%d", ImGuiSliderFlags flags = 0);
	bool SliderInt4(const char* label, int v[4], int v_min, int v_max,
		const char* format = "%d", ImGuiSliderFlags flags = 0);
	bool SliderScalar(const char* label, ImGuiDataType data_type, void* p_data,
		const void* p_min, const void* p_max, const char* format = nullptr,
		ImGuiSliderFlags flags = 0);
	bool SliderScalarN(const char* label, ImGuiDataType data_type, void* p_data,
		int components, const void* p_min, const void* p_max,
		const char* format = nullptr, ImGuiSliderFlags flags = 0);
	bool VSliderFloat(const char* label, ImVec2 size, float* v, float v_min, float v_max,
		const char* format = "%.3f", ImGuiSliderFlags flags = 0);
	bool VSliderInt(const char* label, ImVec2 size, int* v, int v_min, int v_max,
		const char* format = "%d", ImGuiSliderFlags flags = 0);
	bool VSliderScalar(const char* label, ImVec2 size, ImGuiDataType data_type, void* p_data,
		const void* p_min, const void* p_max, const char* format = nullptr,
		ImGuiSliderFlags flags = 0);

	// ── Input text ───────────────────────────────────────────────────────────

	bool InputText(const char* label, char* buf, size_t buf_size,
		ImGuiInputTextFlags flags = 0,
		ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr);
	bool InputTextMultiline(const char* label, char* buf, size_t buf_size,
		ImVec2 size = ImVec2(0.0f, 0.0f), ImGuiInputTextFlags flags = 0,
		ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr);
	bool InputTextWithHint(const char* label, const char* hint, char* buf, size_t buf_size,
		ImGuiInputTextFlags flags = 0,
		ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr);
	bool InputFloat(const char* label, float* v, float step = 0.0f,
		float step_fast = 0.0f, const char* format = "%.3f",
		ImGuiInputTextFlags flags = 0);
	bool InputFloat2(const char* label, float v[2], const char* format = "%.3f",
		ImGuiInputTextFlags flags = 0);
	bool InputFloat3(const char* label, float v[3], const char* format = "%.3f",
		ImGuiInputTextFlags flags = 0);
	bool InputFloat4(const char* label, float v[4], const char* format = "%.3f",
		ImGuiInputTextFlags flags = 0);
	bool InputInt(const char* label, int* v, int step = 1, int step_fast = 100,
		ImGuiInputTextFlags flags = 0);
	bool InputInt2(const char* label, int v[2], ImGuiInputTextFlags flags = 0);
	bool InputInt3(const char* label, int v[3], ImGuiInputTextFlags flags = 0);
	bool InputInt4(const char* label, int v[4], ImGuiInputTextFlags flags = 0);
	bool InputDouble(const char* label, double* v, double step = 0.0,
		double step_fast = 0.0, const char* format = "%.6f",
		ImGuiInputTextFlags flags = 0);
	bool InputScalar(const char* label, ImGuiDataType data_type, void* p_data,
		const void* p_step = nullptr, const void* p_step_fast = nullptr,
		const char* format = nullptr, ImGuiInputTextFlags flags = 0);
	bool InputScalarN(const char* label, ImGuiDataType data_type, void* p_data,
		int components, const void* p_step = nullptr,
		const void* p_step_fast = nullptr, const char* format = nullptr,
		ImGuiInputTextFlags flags = 0);

	// ── Color editors / pickers ───────────────────────────────────────────────

	bool ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags = 0);
	bool ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags = 0);
	bool ColorPicker3(const char* label, float col[3], ImGuiColorEditFlags flags = 0);
	bool ColorPicker4(const char* label, float col[4], ImGuiColorEditFlags flags = 0,
		const float* ref_col = nullptr);
	bool ColorButton(const char* desc_id, ImVec4 col,
		ImGuiColorEditFlags flags = 0, ImVec2 size = ImVec2(0.0f, 0.0f));
	void SetColorEditOptions(ImGuiColorEditFlags flags);

	// ── Trees ────────────────────────────────────────────────────────────────

	bool TreeNode(const char* label);
	bool TreeNode(const char* str_id, const char* fmt, ...) IM_FMTARGS(3);
	bool TreeNode(const void* ptr_id, const char* fmt, ...) IM_FMTARGS(3);
	bool TreeNodeEx(const char* label, ImGuiTreeNodeFlags flags = 0);
	bool TreeNodeEx(const char* str_id, ImGuiTreeNodeFlags flags,
		const char* fmt, ...) IM_FMTARGS(4);
	bool TreeNodeEx(const void* ptr_id, ImGuiTreeNodeFlags flags,
		const char* fmt, ...) IM_FMTARGS(4);
	void TreePush(const char* str_id);
	void TreePush(const void* ptr_id = nullptr);
	void TreePop();
	float GetTreeNodeToLabelSpacing() const;
	bool CollapsingHeader(const char* label, ImGuiTreeNodeFlags flags = 0);
	bool CollapsingHeader(const char* label, bool* p_visible,
		ImGuiTreeNodeFlags flags = 0);
	void SetNextItemOpen(bool is_open, ImGuiCond cond = 0);
	void SetNextItemStorageID(ImGuiID storage_id);

	// ── Selectables ──────────────────────────────────────────────────────────

	bool Selectable(const char* label, bool selected = false,
		ImGuiSelectableFlags flags = 0, ImVec2 size = ImVec2(0.0f, 0.0f));
	bool Selectable(const char* label, bool* p_selected,
		ImGuiSelectableFlags flags = 0, ImVec2 size = ImVec2(0.0f, 0.0f));

	// ── List boxes ───────────────────────────────────────────────────────────

	bool BeginListBox(const char* label, ImVec2 size = ImVec2(0.0f, 0.0f));
	void EndListBox();
	bool ListBox(const char* label, int* current_item,
		const char* const items[], int items_count, int height_in_items = -1);

	// ── Images ───────────────────────────────────────────────────────────────

	void Image(ImTextureID user_texture_id, ImVec2 image_size,
		ImVec2 uv0 = ImVec2(0.0f, 0.0f), ImVec2 uv1 = ImVec2(1.0f, 1.0f),
		ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
		ImVec4 border_col = ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

	// ── Plot widgets ─────────────────────────────────────────────────────────

	void PlotLines(const char* label, const float* values, int values_count,
		int values_offset = 0, const char* overlay_text = nullptr,
		float scale_min = FLT_MAX, float scale_max = FLT_MAX,
		ImVec2 graph_size = ImVec2(0.0f, 0.0f), int stride = sizeof(float));
	void PlotHistogram(const char* label, const float* values, int values_count,
		int values_offset = 0, const char* overlay_text = nullptr,
		float scale_min = FLT_MAX, float scale_max = FLT_MAX,
		ImVec2 graph_size = ImVec2(0.0f, 0.0f), int stride = sizeof(float));

	// ── Menu bars ────────────────────────────────────────────────────────────

	bool BeginMenuBar();
	void EndMenuBar();
	bool BeginMainMenuBar();
	void EndMainMenuBar();
	bool BeginMenu(const char* label, bool enabled = true);
	void EndMenu();
	bool MenuItem(const char* label, const char* shortcut = nullptr,
		bool selected = false, bool enabled = true);
	bool MenuItem(const char* label, const char* shortcut, bool* p_selected,
		bool enabled = true);

	// ── Tooltips ─────────────────────────────────────────────────────────────

	bool BeginTooltip();
	void EndTooltip();
	void SetItemTooltip(const char* fmt, ...) IM_FMTARGS(2);

	// ── Popups ───────────────────────────────────────────────────────────────

	void OpenPopup(const char* str_id, ImGuiPopupFlags popup_flags = 0);
	void OpenPopup(ImGuiID id, ImGuiPopupFlags popup_flags = 0);
	void OpenPopupOnItemClick(const char* str_id = nullptr,
		ImGuiPopupFlags popup_flags = 1);
	void CloseCurrentPopup();
	bool BeginPopup(const char* str_id, ImGuiWindowFlags flags = 0);
	bool BeginPopupContextItem(const char* str_id = nullptr,
		ImGuiPopupFlags popup_flags = 1);
	bool BeginPopupContextWindow(const char* str_id = nullptr,
		ImGuiPopupFlags popup_flags = 1);
	bool BeginPopupContextVoid(const char* str_id = nullptr,
		ImGuiPopupFlags popup_flags = 1);
	bool BeginPopupModal(const char* name, bool* p_open = nullptr,
		ImGuiWindowFlags flags = 0);
	void EndPopup();
	bool IsPopupOpen(const char* str_id, ImGuiPopupFlags flags = 0) const;

	// ── Tab bars / items ─────────────────────────────────────────────────────

	bool BeginTabBar(const char* str_id, ImGuiTabBarFlags flags = 0);
	void EndTabBar();
	bool BeginTabItem(const char* label, bool* p_open = nullptr,
		ImGuiTabItemFlags flags = 0);
	void EndTabItem();
	bool TabItemButton(const char* label, ImGuiTabItemFlags flags = 0);
	void SetTabItemClosed(const char* tab_or_docked_window_label);

	// ── Tables ───────────────────────────────────────────────────────────────

	bool BeginTable(const char* str_id, int columns, ImGuiTableFlags flags = 0,
		ImVec2 outer_size = ImVec2(0.0f, 0.0f), float inner_width = 0.0f);
	void EndTable();
	void TableNextRow(ImGuiTableRowFlags row_flags = 0,
		float min_row_height = 0.0f);
	bool TableNextColumn();
	bool TableSetColumnIndex(int column_n);
	void TableSetupColumn(const char* label, ImGuiTableColumnFlags flags = 0,
		float init_width_or_weight = 0.0f, ImGuiID user_id = 0);
	void TableSetupScrollFreeze(int cols, int rows);
	void TableHeader(const char* label);
	void TableHeadersRow();
	void TableAngledHeadersRow();
	ImGuiTableSortSpecs* TableGetSortSpecs();
	int TableGetColumnCount() const;
	int TableGetColumnIndex() const;
	int TableGetRowIndex() const;
	const char* TableGetColumnName(int column_n = -1) const;
	ImGuiTableColumnFlags TableGetColumnFlags(int column_n = -1) const;
	void TableSetColumnEnabled(int column_n, bool v);
	ImGuiID TableGetInstanceID(ImGuiID table_id, int instance_no) const;
	void TableSetBgColor(ImGuiTableBgTarget target, ImU32 color,
		int column_n = -1);

	// ── Groups ───────────────────────────────────────────────────────────────

	void BeginGroup();
	void EndGroup();

	// ── Cursor / layout ──────────────────────────────────────────────────────

	ImVec2 GetCursorPos() const;
	float GetCursorPosX() const;
	float GetCursorPosY() const;
	void SetCursorPos(ImVec2 local_pos);
	void SetCursorPosX(float local_x);
	void SetCursorPosY(float local_y);
	ImVec2 GetCursorStartPos() const;
	ImVec2 GetCursorScreenPos() const;
	void SetCursorScreenPos(ImVec2 pos);
	void AlignTextToFramePadding();
	float GetTextLineHeight() const;
	float GetTextLineHeightWithSpacing() const;
	float GetFrameHeight() const;
	float GetFrameHeightWithSpacing() const;

	// ── Layout helpers ────────────────────────────────────────────────────────

	void Separator();
	void SeparatorText(const char* label);
	void SameLine(float offset_from_start_x = 0.0f, float spacing = -1.0f);
	void NewLine();
	void Spacing();
	void Dummy(ImVec2 size);
	void Indent(float indent_w = 0.0f);
	void Unindent(float indent_w = 0.0f);

	// ── Item-width / next-item hints ─────────────────────────────────────────

	void SetNextItemWidth(float item_width);
	void PushItemWidth(float item_width);
	void PopItemWidth();
	float CalcItemWidth() const;
	void PushTextWrapPos(float wrap_local_pos_x = 0.0f);
	void PopTextWrapPos();

	// ── ID stack ─────────────────────────────────────────────────────────────

	void PushID(const char* str_id);
	void PushID(const char* str_id_begin, const char* str_id_end);
	void PushID(const void* ptr_id);
	void PushID(int int_id);
	void PopID();
	ImGuiID GetID(const char* str_id) const;
	ImGuiID GetID(const char* str_id_begin, const char* str_id_end) const;
	ImGuiID GetID(const void* ptr_id) const;
	ImGuiID GetID(int int_id) const;

	// ── Scroll ───────────────────────────────────────────────────────────────

	float GetScrollX() const;
	float GetScrollY() const;
	void SetScrollX(float scroll_x);
	void SetScrollY(float scroll_y);
	float GetScrollMaxX() const;
	float GetScrollMaxY() const;
	void SetScrollHereX(float center_x_ratio = 0.5f);
	void SetScrollHereY(float center_y_ratio = 0.5f);
	void SetScrollFromPosX(float local_x, float center_x_ratio = 0.5f);
	void SetScrollFromPosY(float local_y, float center_y_ratio = 0.5f);

	// ── Focus / activation ────────────────────────────────────────────────────

	void SetItemDefaultFocus();
	void SetKeyboardFocusHere(int offset = 0);
	void SetNextItemAllowOverlap();

	// ── Item queries ─────────────────────────────────────────────────────────

	bool IsItemHovered(ImGuiHoveredFlags flags = 0) const;
	bool IsItemActive() const;
	bool IsItemFocused() const;
	bool IsItemClicked(ImGuiMouseButton mouse_button = 0) const;
	bool IsItemVisible() const;
	bool IsItemEdited() const;
	bool IsItemActivated() const;
	bool IsItemDeactivated() const;
	bool IsItemDeactivatedAfterEdit() const;
	bool IsItemToggledOpen() const;
	ImVec2 GetItemRectMin() const;
	ImVec2 GetItemRectMax() const;
	ImVec2 GetItemRectSize() const;

	// ── Miscellaneous ─────────────────────────────────────────────────────────

	ImDrawList* GetWindowDrawList() const;
	const char* GetClipboardText() const;
	void SetClipboardText(const char* text);
	void PushClipRect(ImVec2 clip_rect_min, ImVec2 clip_rect_max,
		bool intersect_with_current_clip_rect);
	void PopClipRect();
	ImVec2 CalcTextSize(const char* text, const char* text_end = nullptr,
		bool hide_text_after_double_hash = false, float wrap_width = -1.0f) const;

	// ── Columns (legacy) ─────────────────────────────────────────────────────

	void Columns(int count = 1, const char* id = nullptr, bool borders = true);
	void NextColumn();
	int GetColumnIndex() const;
	float GetColumnWidth(int column_index = -1) const;
	void SetColumnWidth(int column_index, float width);
	float GetColumnOffset(int column_index = -1) const;
	void SetColumnOffset(int column_index, float offset_x);
	int GetColumnsCount() const;
};

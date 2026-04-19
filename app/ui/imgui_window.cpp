#include "app/ui/imgui_window.hpp"

#include "imgui.h"

#include <cstdarg>
#include <cstddef>

// ── Constructor ───────────────────────────────────────────────────────────────

ImGuiWindow::ImGuiWindow()
    : Flags { ImGuiWindowFlags_None }
{
}

// ── Window lifecycle ──────────────────────────────────────────────────────────

bool ImGuiWindow::Begin(const char* title, bool* p_open, ImGuiWindowFlags flags)
{
	return ImGui::Begin(title, p_open, flags);
}

void ImGuiWindow::End()
{
	ImGui::End();
}

// ── Pre-Begin hints ───────────────────────────────────────────────────────────

void ImGuiWindow::SetNextPos(ImVec2 pos, ImGuiCond cond, ImVec2 pivot)
{
	ImGui::SetNextWindowPos(pos, cond, pivot);
}

void ImGuiWindow::SetNextSize(ImVec2 size, ImGuiCond cond)
{
	ImGui::SetNextWindowSize(size, cond);
}

void ImGuiWindow::SetNextSizeConstraints(ImVec2 size_min, ImVec2 size_max,
	ImGuiSizeCallback custom_callback, void* custom_callback_data)
{
	ImGui::SetNextWindowSizeConstraints(size_min, size_max, custom_callback,
		custom_callback_data);
}

void ImGuiWindow::SetNextContentSize(ImVec2 size)
{
	ImGui::SetNextWindowContentSize(size);
}

void ImGuiWindow::SetNextCollapsed(bool collapsed, ImGuiCond cond)
{
	ImGui::SetNextWindowCollapsed(collapsed, cond);
}

void ImGuiWindow::SetNextFocus()
{
	ImGui::SetNextWindowFocus();
}

void ImGuiWindow::SetNextScroll(ImVec2 scroll)
{
	ImGui::SetNextWindowScroll(scroll);
}

void ImGuiWindow::SetNextBgAlpha(float alpha)
{
	ImGui::SetNextWindowBgAlpha(alpha);
}

// ── Child windows ─────────────────────────────────────────────────────────────

bool ImGuiWindow::BeginChild(const char* str_id, ImVec2 size,
	ImGuiChildFlags child_flags, ImGuiWindowFlags window_flags)
{
	return ImGui::BeginChild(str_id, size, child_flags, window_flags);
}

bool ImGuiWindow::BeginChild(ImGuiID id, ImVec2 size,
	ImGuiChildFlags child_flags, ImGuiWindowFlags window_flags)
{
	return ImGui::BeginChild(id, size, child_flags, window_flags);
}

void ImGuiWindow::EndChild()
{
	ImGui::EndChild();
}

// ── Window queries ────────────────────────────────────────────────────────────

bool ImGuiWindow::IsWindowAppearing() const
{
	return ImGui::IsWindowAppearing();
}

bool ImGuiWindow::IsWindowCollapsed() const
{
	return ImGui::IsWindowCollapsed();
}

bool ImGuiWindow::IsWindowFocused(ImGuiFocusedFlags flags) const
{
	return ImGui::IsWindowFocused(flags);
}

bool ImGuiWindow::IsWindowHovered(ImGuiHoveredFlags flags) const
{
	return ImGui::IsWindowHovered(flags);
}

ImVec2 ImGuiWindow::GetWindowPos() const
{
	return ImGui::GetWindowPos();
}

ImVec2 ImGuiWindow::GetWindowSize() const
{
	return ImGui::GetWindowSize();
}

float ImGuiWindow::GetWindowWidth() const
{
	return ImGui::GetWindowWidth();
}

float ImGuiWindow::GetWindowHeight() const
{
	return ImGui::GetWindowHeight();
}

ImVec2 ImGuiWindow::GetContentRegionAvail() const
{
	return ImGui::GetContentRegionAvail();
}

ImVec2 ImGuiWindow::GetContentRegionMax() const
{
	return ImGui::GetContentRegionMax();
}

ImVec2 ImGuiWindow::GetWindowContentRegionMin() const
{
	return ImGui::GetWindowContentRegionMin();
}

ImVec2 ImGuiWindow::GetWindowContentRegionMax() const
{
	return ImGui::GetWindowContentRegionMax();
}

// ── Text / labels ─────────────────────────────────────────────────────────────

void ImGuiWindow::Label(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	ImGui::TextV(fmt, args);
	va_end(args);
}

void ImGuiWindow::LabelUnformatted(const char* text, const char* text_end)
{
	ImGui::TextUnformatted(text, text_end);
}

void ImGuiWindow::LabelColored(ImVec4 col, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	ImGui::TextColoredV(col, fmt, args);
	va_end(args);
}

void ImGuiWindow::LabelDisabled(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	ImGui::TextDisabledV(fmt, args);
	va_end(args);
}

void ImGuiWindow::LabelWrapped(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	ImGui::TextWrappedV(fmt, args);
	va_end(args);
}

void ImGuiWindow::LabelText(const char* label, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	ImGui::LabelTextV(label, fmt, args);
	va_end(args);
}

void ImGuiWindow::LabelBullet(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	ImGui::BulletTextV(fmt, args);
	va_end(args);
}

// ── Buttons ───────────────────────────────────────────────────────────────────

bool ImGuiWindow::Button(const char* label, ImVec2 size)
{
	return ImGui::Button(label, size);
}

bool ImGuiWindow::SmallButton(const char* label)
{
	return ImGui::SmallButton(label);
}

bool ImGuiWindow::InvisibleButton(const char* str_id, ImVec2 size,
	ImGuiButtonFlags flags)
{
	return ImGui::InvisibleButton(str_id, size, flags);
}

bool ImGuiWindow::ArrowButton(const char* str_id, ImGuiDir dir)
{
	return ImGui::ArrowButton(str_id, dir);
}

bool ImGuiWindow::ImageButton(const char* str_id, ImTextureID texture_id,
	ImVec2 image_size, ImVec2 uv0, ImVec2 uv1, ImVec4 bg_col, ImVec4 tint_col)
{
	return ImGui::ImageButton(str_id, texture_id, image_size, uv0, uv1, bg_col,
		tint_col);
}

bool ImGuiWindow::RadioButton(const char* label, bool active)
{
	return ImGui::RadioButton(label, active);
}

bool ImGuiWindow::RadioButton(const char* label, int* v, int v_button)
{
	return ImGui::RadioButton(label, v, v_button);
}

void ImGuiWindow::ProgressBar(float fraction, ImVec2 size_arg,
	const char* overlay)
{
	ImGui::ProgressBar(fraction, size_arg, overlay);
}

void ImGuiWindow::Bullet()
{
	ImGui::Bullet();
}

// ── Checkboxes ────────────────────────────────────────────────────────────────

bool ImGuiWindow::Checkbox(const char* label, bool* v)
{
	return ImGui::Checkbox(label, v);
}

bool ImGuiWindow::CheckboxFlags(const char* label, int* flags, int flags_value)
{
	return ImGui::CheckboxFlags(label, flags, flags_value);
}

bool ImGuiWindow::CheckboxFlags(const char* label, unsigned int* flags,
	unsigned int flags_value)
{
	return ImGui::CheckboxFlags(label, flags, flags_value);
}

// ── Combo boxes ───────────────────────────────────────────────────────────────

bool ImGuiWindow::BeginCombo(const char* label, const char* preview_value,
	ImGuiComboFlags flags)
{
	return ImGui::BeginCombo(label, preview_value, flags);
}

void ImGuiWindow::EndCombo()
{
	ImGui::EndCombo();
}

bool ImGuiWindow::Combo(const char* label, int* current_item,
	const char* const items[], int items_count, int popup_max_height_in_items)
{
	return ImGui::Combo(label, current_item, items, items_count,
		popup_max_height_in_items);
}

bool ImGuiWindow::Combo(const char* label, int* current_item,
	const char* items_separated_by_zeros, int popup_max_height_in_items)
{
	return ImGui::Combo(label, current_item, items_separated_by_zeros,
		popup_max_height_in_items);
}

// ── Drag widgets ──────────────────────────────────────────────────────────────

bool ImGuiWindow::DragFloat(const char* label, float* v, float v_speed,
	float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
	return ImGui::DragFloat(label, v, v_speed, v_min, v_max, format, flags);
}

bool ImGuiWindow::DragFloat2(const char* label, float v[2], float v_speed,
	float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
	return ImGui::DragFloat2(label, v, v_speed, v_min, v_max, format, flags);
}

bool ImGuiWindow::DragFloat3(const char* label, float v[3], float v_speed,
	float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
	return ImGui::DragFloat3(label, v, v_speed, v_min, v_max, format, flags);
}

bool ImGuiWindow::DragFloat4(const char* label, float v[4], float v_speed,
	float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
	return ImGui::DragFloat4(label, v, v_speed, v_min, v_max, format, flags);
}

bool ImGuiWindow::DragFloatRange2(const char* label, float* v_current_min,
	float* v_current_max, float v_speed, float v_min, float v_max,
	const char* format, const char* format_max, ImGuiSliderFlags flags)
{
	return ImGui::DragFloatRange2(label, v_current_min, v_current_max, v_speed,
		v_min, v_max, format, format_max, flags);
}

bool ImGuiWindow::DragInt(const char* label, int* v, float v_speed, int v_min,
	int v_max, const char* format, ImGuiSliderFlags flags)
{
	return ImGui::DragInt(label, v, v_speed, v_min, v_max, format, flags);
}

bool ImGuiWindow::DragInt2(const char* label, int v[2], float v_speed,
	int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
{
	return ImGui::DragInt2(label, v, v_speed, v_min, v_max, format, flags);
}

bool ImGuiWindow::DragInt3(const char* label, int v[3], float v_speed,
	int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
{
	return ImGui::DragInt3(label, v, v_speed, v_min, v_max, format, flags);
}

bool ImGuiWindow::DragInt4(const char* label, int v[4], float v_speed,
	int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
{
	return ImGui::DragInt4(label, v, v_speed, v_min, v_max, format, flags);
}

bool ImGuiWindow::DragIntRange2(const char* label, int* v_current_min,
	int* v_current_max, float v_speed, int v_min, int v_max,
	const char* format, const char* format_max, ImGuiSliderFlags flags)
{
	return ImGui::DragIntRange2(label, v_current_min, v_current_max, v_speed,
		v_min, v_max, format, format_max, flags);
}

bool ImGuiWindow::DragScalar(const char* label, ImGuiDataType data_type,
	void* p_data, float v_speed, const void* p_min, const void* p_max,
	const char* format, ImGuiSliderFlags flags)
{
	return ImGui::DragScalar(label, data_type, p_data, v_speed, p_min, p_max,
		format, flags);
}

bool ImGuiWindow::DragScalarN(const char* label, ImGuiDataType data_type,
	void* p_data, int components, float v_speed, const void* p_min,
	const void* p_max, const char* format, ImGuiSliderFlags flags)
{
	return ImGui::DragScalarN(label, data_type, p_data, components, v_speed,
		p_min, p_max, format, flags);
}

// ── Slider widgets ────────────────────────────────────────────────────────────

bool ImGuiWindow::SliderFloat(const char* label, float* v, float v_min,
	float v_max, const char* format, ImGuiSliderFlags flags)
{
	return ImGui::SliderFloat(label, v, v_min, v_max, format, flags);
}

bool ImGuiWindow::SliderFloat2(const char* label, float v[2], float v_min,
	float v_max, const char* format, ImGuiSliderFlags flags)
{
	return ImGui::SliderFloat2(label, v, v_min, v_max, format, flags);
}

bool ImGuiWindow::SliderFloat3(const char* label, float v[3], float v_min,
	float v_max, const char* format, ImGuiSliderFlags flags)
{
	return ImGui::SliderFloat3(label, v, v_min, v_max, format, flags);
}

bool ImGuiWindow::SliderFloat4(const char* label, float v[4], float v_min,
	float v_max, const char* format, ImGuiSliderFlags flags)
{
	return ImGui::SliderFloat4(label, v, v_min, v_max, format, flags);
}

bool ImGuiWindow::SliderAngle(const char* label, float* v_rad,
	float v_degrees_min, float v_degrees_max, const char* format,
	ImGuiSliderFlags flags)
{
	return ImGui::SliderAngle(label, v_rad, v_degrees_min, v_degrees_max,
		format, flags);
}

bool ImGuiWindow::SliderInt(const char* label, int* v, int v_min, int v_max,
	const char* format, ImGuiSliderFlags flags)
{
	return ImGui::SliderInt(label, v, v_min, v_max, format, flags);
}

bool ImGuiWindow::SliderInt2(const char* label, int v[2], int v_min, int v_max,
	const char* format, ImGuiSliderFlags flags)
{
	return ImGui::SliderInt2(label, v, v_min, v_max, format, flags);
}

bool ImGuiWindow::SliderInt3(const char* label, int v[3], int v_min, int v_max,
	const char* format, ImGuiSliderFlags flags)
{
	return ImGui::SliderInt3(label, v, v_min, v_max, format, flags);
}

bool ImGuiWindow::SliderInt4(const char* label, int v[4], int v_min, int v_max,
	const char* format, ImGuiSliderFlags flags)
{
	return ImGui::SliderInt4(label, v, v_min, v_max, format, flags);
}

bool ImGuiWindow::SliderScalar(const char* label, ImGuiDataType data_type,
	void* p_data, const void* p_min, const void* p_max, const char* format,
	ImGuiSliderFlags flags)
{
	return ImGui::SliderScalar(label, data_type, p_data, p_min, p_max, format,
		flags);
}

bool ImGuiWindow::SliderScalarN(const char* label, ImGuiDataType data_type,
	void* p_data, int components, const void* p_min, const void* p_max,
	const char* format, ImGuiSliderFlags flags)
{
	return ImGui::SliderScalarN(label, data_type, p_data, components, p_min,
		p_max, format, flags);
}

bool ImGuiWindow::VSliderFloat(const char* label, ImVec2 size, float* v,
	float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
	return ImGui::VSliderFloat(label, size, v, v_min, v_max, format, flags);
}

bool ImGuiWindow::VSliderInt(const char* label, ImVec2 size, int* v, int v_min,
	int v_max, const char* format, ImGuiSliderFlags flags)
{
	return ImGui::VSliderInt(label, size, v, v_min, v_max, format, flags);
}

bool ImGuiWindow::VSliderScalar(const char* label, ImVec2 size,
	ImGuiDataType data_type, void* p_data, const void* p_min,
	const void* p_max, const char* format, ImGuiSliderFlags flags)
{
	return ImGui::VSliderScalar(label, size, data_type, p_data, p_min, p_max,
		format, flags);
}

// ── Input text ────────────────────────────────────────────────────────────────

bool ImGuiWindow::InputText(const char* label, char* buf, size_t buf_size,
	ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
	return ImGui::InputText(label, buf, buf_size, flags, callback, user_data);
}

bool ImGuiWindow::InputTextMultiline(const char* label, char* buf,
	size_t buf_size, ImVec2 size, ImGuiInputTextFlags flags,
	ImGuiInputTextCallback callback, void* user_data)
{
	return ImGui::InputTextMultiline(label, buf, buf_size, size, flags,
		callback, user_data);
}

bool ImGuiWindow::InputTextWithHint(const char* label, const char* hint,
	char* buf, size_t buf_size, ImGuiInputTextFlags flags,
	ImGuiInputTextCallback callback, void* user_data)
{
	return ImGui::InputTextWithHint(label, hint, buf, buf_size, flags, callback,
		user_data);
}

bool ImGuiWindow::InputFloat(const char* label, float* v, float step,
	float step_fast, const char* format, ImGuiInputTextFlags flags)
{
	return ImGui::InputFloat(label, v, step, step_fast, format, flags);
}

bool ImGuiWindow::InputFloat2(const char* label, float v[2],
	const char* format, ImGuiInputTextFlags flags)
{
	return ImGui::InputFloat2(label, v, format, flags);
}

bool ImGuiWindow::InputFloat3(const char* label, float v[3],
	const char* format, ImGuiInputTextFlags flags)
{
	return ImGui::InputFloat3(label, v, format, flags);
}

bool ImGuiWindow::InputFloat4(const char* label, float v[4],
	const char* format, ImGuiInputTextFlags flags)
{
	return ImGui::InputFloat4(label, v, format, flags);
}

bool ImGuiWindow::InputInt(const char* label, int* v, int step, int step_fast,
	ImGuiInputTextFlags flags)
{
	return ImGui::InputInt(label, v, step, step_fast, flags);
}

bool ImGuiWindow::InputInt2(const char* label, int v[2],
	ImGuiInputTextFlags flags)
{
	return ImGui::InputInt2(label, v, flags);
}

bool ImGuiWindow::InputInt3(const char* label, int v[3],
	ImGuiInputTextFlags flags)
{
	return ImGui::InputInt3(label, v, flags);
}

bool ImGuiWindow::InputInt4(const char* label, int v[4],
	ImGuiInputTextFlags flags)
{
	return ImGui::InputInt4(label, v, flags);
}

bool ImGuiWindow::InputDouble(const char* label, double* v, double step,
	double step_fast, const char* format, ImGuiInputTextFlags flags)
{
	return ImGui::InputDouble(label, v, step, step_fast, format, flags);
}

bool ImGuiWindow::InputScalar(const char* label, ImGuiDataType data_type,
	void* p_data, const void* p_step, const void* p_step_fast,
	const char* format, ImGuiInputTextFlags flags)
{
	return ImGui::InputScalar(label, data_type, p_data, p_step, p_step_fast,
		format, flags);
}

bool ImGuiWindow::InputScalarN(const char* label, ImGuiDataType data_type,
	void* p_data, int components, const void* p_step, const void* p_step_fast,
	const char* format, ImGuiInputTextFlags flags)
{
	return ImGui::InputScalarN(label, data_type, p_data, components, p_step,
		p_step_fast, format, flags);
}

// ── Color editors / pickers ───────────────────────────────────────────────────

bool ImGuiWindow::ColorEdit3(const char* label, float col[3],
	ImGuiColorEditFlags flags)
{
	return ImGui::ColorEdit3(label, col, flags);
}

bool ImGuiWindow::ColorEdit4(const char* label, float col[4],
	ImGuiColorEditFlags flags)
{
	return ImGui::ColorEdit4(label, col, flags);
}

bool ImGuiWindow::ColorPicker3(const char* label, float col[3],
	ImGuiColorEditFlags flags)
{
	return ImGui::ColorPicker3(label, col, flags);
}

bool ImGuiWindow::ColorPicker4(const char* label, float col[4],
	ImGuiColorEditFlags flags, const float* ref_col)
{
	return ImGui::ColorPicker4(label, col, flags, ref_col);
}

bool ImGuiWindow::ColorButton(const char* desc_id, ImVec4 col,
	ImGuiColorEditFlags flags, ImVec2 size)
{
	return ImGui::ColorButton(desc_id, col, flags, size);
}

void ImGuiWindow::SetColorEditOptions(ImGuiColorEditFlags flags)
{
	ImGui::SetColorEditOptions(flags);
}

// ── Trees ─────────────────────────────────────────────────────────────────────

bool ImGuiWindow::TreeNode(const char* label)
{
	return ImGui::TreeNode(label);
}

bool ImGuiWindow::TreeNode(const char* str_id, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const bool result = ImGui::TreeNodeV(str_id, fmt, args);
	va_end(args);
	return result;
}

bool ImGuiWindow::TreeNode(const void* ptr_id, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const bool result = ImGui::TreeNodeV(ptr_id, fmt, args);
	va_end(args);
	return result;
}

bool ImGuiWindow::TreeNodeEx(const char* label, ImGuiTreeNodeFlags flags)
{
	return ImGui::TreeNodeEx(label, flags);
}

bool ImGuiWindow::TreeNodeEx(const char* str_id, ImGuiTreeNodeFlags flags,
	const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const bool result = ImGui::TreeNodeExV(str_id, flags, fmt, args);
	va_end(args);
	return result;
}

bool ImGuiWindow::TreeNodeEx(const void* ptr_id, ImGuiTreeNodeFlags flags,
	const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const bool result = ImGui::TreeNodeExV(ptr_id, flags, fmt, args);
	va_end(args);
	return result;
}

void ImGuiWindow::TreePush(const char* str_id)
{
	ImGui::TreePush(str_id);
}

void ImGuiWindow::TreePush(const void* ptr_id)
{
	ImGui::TreePush(ptr_id);
}

void ImGuiWindow::TreePop()
{
	ImGui::TreePop();
}

float ImGuiWindow::GetTreeNodeToLabelSpacing() const
{
	return ImGui::GetTreeNodeToLabelSpacing();
}

bool ImGuiWindow::CollapsingHeader(const char* label, ImGuiTreeNodeFlags flags)
{
	return ImGui::CollapsingHeader(label, flags);
}

bool ImGuiWindow::CollapsingHeader(const char* label, bool* p_visible,
	ImGuiTreeNodeFlags flags)
{
	return ImGui::CollapsingHeader(label, p_visible, flags);
}

void ImGuiWindow::SetNextItemOpen(bool is_open, ImGuiCond cond)
{
	ImGui::SetNextItemOpen(is_open, cond);
}

void ImGuiWindow::SetNextItemStorageID(ImGuiID storage_id)
{
	ImGui::SetNextItemStorageID(storage_id);
}

// ── Selectables ───────────────────────────────────────────────────────────────

bool ImGuiWindow::Selectable(const char* label, bool selected,
	ImGuiSelectableFlags flags, ImVec2 size)
{
	return ImGui::Selectable(label, selected, flags, size);
}

bool ImGuiWindow::Selectable(const char* label, bool* p_selected,
	ImGuiSelectableFlags flags, ImVec2 size)
{
	return ImGui::Selectable(label, p_selected, flags, size);
}

// ── List boxes ────────────────────────────────────────────────────────────────

bool ImGuiWindow::BeginListBox(const char* label, ImVec2 size)
{
	return ImGui::BeginListBox(label, size);
}

void ImGuiWindow::EndListBox()
{
	ImGui::EndListBox();
}

bool ImGuiWindow::ListBox(const char* label, int* current_item,
	const char* const items[], int items_count, int height_in_items)
{
	return ImGui::ListBox(label, current_item, items, items_count,
		height_in_items);
}

// ── Images ────────────────────────────────────────────────────────────────────

void ImGuiWindow::Image(ImTextureID user_texture_id, ImVec2 image_size,
	ImVec2 uv0, ImVec2 uv1, ImVec4 tint_col, ImVec4 border_col)
{
	ImGui::Image(user_texture_id, image_size, uv0, uv1, tint_col, border_col);
}

// ── Plot widgets ──────────────────────────────────────────────────────────────

void ImGuiWindow::PlotLines(const char* label, const float* values,
	int values_count, int values_offset, const char* overlay_text,
	float scale_min, float scale_max, ImVec2 graph_size, int stride)
{
	ImGui::PlotLines(label, values, values_count, values_offset, overlay_text,
		scale_min, scale_max, graph_size, stride);
}

void ImGuiWindow::PlotHistogram(const char* label, const float* values,
	int values_count, int values_offset, const char* overlay_text,
	float scale_min, float scale_max, ImVec2 graph_size, int stride)
{
	ImGui::PlotHistogram(label, values, values_count, values_offset,
		overlay_text, scale_min, scale_max, graph_size, stride);
}

// ── Menu bars ─────────────────────────────────────────────────────────────────

bool ImGuiWindow::BeginMenuBar()
{
	return ImGui::BeginMenuBar();
}

void ImGuiWindow::EndMenuBar()
{
	ImGui::EndMenuBar();
}

bool ImGuiWindow::BeginMainMenuBar()
{
	return ImGui::BeginMainMenuBar();
}

void ImGuiWindow::EndMainMenuBar()
{
	ImGui::EndMainMenuBar();
}

bool ImGuiWindow::BeginMenu(const char* label, bool enabled)
{
	return ImGui::BeginMenu(label, enabled);
}

void ImGuiWindow::EndMenu()
{
	ImGui::EndMenu();
}

bool ImGuiWindow::MenuItem(const char* label, const char* shortcut,
	bool selected, bool enabled)
{
	return ImGui::MenuItem(label, shortcut, selected, enabled);
}

bool ImGuiWindow::MenuItem(const char* label, const char* shortcut,
	bool* p_selected, bool enabled)
{
	return ImGui::MenuItem(label, shortcut, p_selected, enabled);
}

// ── Tooltips ──────────────────────────────────────────────────────────────────

bool ImGuiWindow::BeginTooltip()
{
	return ImGui::BeginTooltip();
}

void ImGuiWindow::EndTooltip()
{
	ImGui::EndTooltip();
}

void ImGuiWindow::SetItemTooltip(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	ImGui::SetItemTooltipV(fmt, args);
	va_end(args);
}

// ── Popups ────────────────────────────────────────────────────────────────────

void ImGuiWindow::OpenPopup(const char* str_id, ImGuiPopupFlags popup_flags)
{
	ImGui::OpenPopup(str_id, popup_flags);
}

void ImGuiWindow::OpenPopup(ImGuiID id, ImGuiPopupFlags popup_flags)
{
	ImGui::OpenPopup(id, popup_flags);
}

void ImGuiWindow::OpenPopupOnItemClick(const char* str_id,
	ImGuiPopupFlags popup_flags)
{
	ImGui::OpenPopupOnItemClick(str_id, popup_flags);
}

void ImGuiWindow::CloseCurrentPopup()
{
	ImGui::CloseCurrentPopup();
}

bool ImGuiWindow::BeginPopup(const char* str_id, ImGuiWindowFlags flags)
{
	return ImGui::BeginPopup(str_id, flags);
}

bool ImGuiWindow::BeginPopupContextItem(const char* str_id,
	ImGuiPopupFlags popup_flags)
{
	return ImGui::BeginPopupContextItem(str_id, popup_flags);
}

bool ImGuiWindow::BeginPopupContextWindow(const char* str_id,
	ImGuiPopupFlags popup_flags)
{
	return ImGui::BeginPopupContextWindow(str_id, popup_flags);
}

bool ImGuiWindow::BeginPopupContextVoid(const char* str_id,
	ImGuiPopupFlags popup_flags)
{
	return ImGui::BeginPopupContextVoid(str_id, popup_flags);
}

bool ImGuiWindow::BeginPopupModal(const char* name, bool* p_open,
	ImGuiWindowFlags flags)
{
	return ImGui::BeginPopupModal(name, p_open, flags);
}

void ImGuiWindow::EndPopup()
{
	ImGui::EndPopup();
}

bool ImGuiWindow::IsPopupOpen(const char* str_id, ImGuiPopupFlags flags) const
{
	return ImGui::IsPopupOpen(str_id, flags);
}

// ── Tab bars / items ──────────────────────────────────────────────────────────

bool ImGuiWindow::BeginTabBar(const char* str_id, ImGuiTabBarFlags flags)
{
	return ImGui::BeginTabBar(str_id, flags);
}

void ImGuiWindow::EndTabBar()
{
	ImGui::EndTabBar();
}

bool ImGuiWindow::BeginTabItem(const char* label, bool* p_open,
	ImGuiTabItemFlags flags)
{
	return ImGui::BeginTabItem(label, p_open, flags);
}

void ImGuiWindow::EndTabItem()
{
	ImGui::EndTabItem();
}

bool ImGuiWindow::TabItemButton(const char* label, ImGuiTabItemFlags flags)
{
	return ImGui::TabItemButton(label, flags);
}

void ImGuiWindow::SetTabItemClosed(const char* tab_or_docked_window_label)
{
	ImGui::SetTabItemClosed(tab_or_docked_window_label);
}

// ── Tables ────────────────────────────────────────────────────────────────────

bool ImGuiWindow::BeginTable(const char* str_id, int columns,
	ImGuiTableFlags flags, ImVec2 outer_size, float inner_width)
{
	return ImGui::BeginTable(str_id, columns, flags, outer_size, inner_width);
}

void ImGuiWindow::EndTable()
{
	ImGui::EndTable();
}

void ImGuiWindow::TableNextRow(ImGuiTableRowFlags row_flags,
	float min_row_height)
{
	ImGui::TableNextRow(row_flags, min_row_height);
}

bool ImGuiWindow::TableNextColumn()
{
	return ImGui::TableNextColumn();
}

bool ImGuiWindow::TableSetColumnIndex(int column_n)
{
	return ImGui::TableSetColumnIndex(column_n);
}

void ImGuiWindow::TableSetupColumn(const char* label,
	ImGuiTableColumnFlags flags, float init_width_or_weight, ImGuiID user_id)
{
	ImGui::TableSetupColumn(label, flags, init_width_or_weight, user_id);
}

void ImGuiWindow::TableSetupScrollFreeze(int cols, int rows)
{
	ImGui::TableSetupScrollFreeze(cols, rows);
}

void ImGuiWindow::TableHeader(const char* label)
{
	ImGui::TableHeader(label);
}

void ImGuiWindow::TableHeadersRow()
{
	ImGui::TableHeadersRow();
}

void ImGuiWindow::TableAngledHeadersRow()
{
	ImGui::TableAngledHeadersRow();
}

ImGuiTableSortSpecs* ImGuiWindow::TableGetSortSpecs()
{
	return ImGui::TableGetSortSpecs();
}

int ImGuiWindow::TableGetColumnCount() const
{
	return ImGui::TableGetColumnCount();
}

int ImGuiWindow::TableGetColumnIndex() const
{
	return ImGui::TableGetColumnIndex();
}

int ImGuiWindow::TableGetRowIndex() const
{
	return ImGui::TableGetRowIndex();
}

const char* ImGuiWindow::TableGetColumnName(int column_n) const
{
	return ImGui::TableGetColumnName(column_n);
}

ImGuiTableColumnFlags ImGuiWindow::TableGetColumnFlags(int column_n) const
{
	return ImGui::TableGetColumnFlags(column_n);
}

void ImGuiWindow::TableSetColumnEnabled(int column_n, bool v)
{
	ImGui::TableSetColumnEnabled(column_n, v);
}

ImGuiID ImGuiWindow::TableGetInstanceID(ImGuiID table_id, int instance_no) const
{
	return ImGui::TableGetInstanceID(table_id, instance_no);
}

void ImGuiWindow::TableSetBgColor(ImGuiTableBgTarget target, ImU32 color,
	int column_n)
{
	ImGui::TableSetBgColor(target, color, column_n);
}

// ── Groups ────────────────────────────────────────────────────────────────────

void ImGuiWindow::BeginGroup()
{
	ImGui::BeginGroup();
}

void ImGuiWindow::EndGroup()
{
	ImGui::EndGroup();
}

// ── Cursor / layout ───────────────────────────────────────────────────────────

ImVec2 ImGuiWindow::GetCursorPos() const
{
	return ImGui::GetCursorPos();
}

float ImGuiWindow::GetCursorPosX() const
{
	return ImGui::GetCursorPosX();
}

float ImGuiWindow::GetCursorPosY() const
{
	return ImGui::GetCursorPosY();
}

void ImGuiWindow::SetCursorPos(ImVec2 local_pos)
{
	ImGui::SetCursorPos(local_pos);
}

void ImGuiWindow::SetCursorPosX(float local_x)
{
	ImGui::SetCursorPosX(local_x);
}

void ImGuiWindow::SetCursorPosY(float local_y)
{
	ImGui::SetCursorPosY(local_y);
}

ImVec2 ImGuiWindow::GetCursorStartPos() const
{
	return ImGui::GetCursorStartPos();
}

ImVec2 ImGuiWindow::GetCursorScreenPos() const
{
	return ImGui::GetCursorScreenPos();
}

void ImGuiWindow::SetCursorScreenPos(ImVec2 pos)
{
	ImGui::SetCursorScreenPos(pos);
}

void ImGuiWindow::AlignTextToFramePadding()
{
	ImGui::AlignTextToFramePadding();
}

float ImGuiWindow::GetTextLineHeight() const
{
	return ImGui::GetTextLineHeight();
}

float ImGuiWindow::GetTextLineHeightWithSpacing() const
{
	return ImGui::GetTextLineHeightWithSpacing();
}

float ImGuiWindow::GetFrameHeight() const
{
	return ImGui::GetFrameHeight();
}

float ImGuiWindow::GetFrameHeightWithSpacing() const
{
	return ImGui::GetFrameHeightWithSpacing();
}

// ── Layout helpers ────────────────────────────────────────────────────────────

void ImGuiWindow::Separator()
{
	ImGui::Separator();
}

void ImGuiWindow::SeparatorText(const char* label)
{
	ImGui::SeparatorText(label);
}

void ImGuiWindow::SameLine(float offset_from_start_x, float spacing)
{
	ImGui::SameLine(offset_from_start_x, spacing);
}

void ImGuiWindow::NewLine()
{
	ImGui::NewLine();
}

void ImGuiWindow::Spacing()
{
	ImGui::Spacing();
}

void ImGuiWindow::Dummy(ImVec2 size)
{
	ImGui::Dummy(size);
}

void ImGuiWindow::Indent(float indent_w)
{
	ImGui::Indent(indent_w);
}

void ImGuiWindow::Unindent(float indent_w)
{
	ImGui::Unindent(indent_w);
}

// ── Item-width / next-item hints ──────────────────────────────────────────────

void ImGuiWindow::SetNextItemWidth(float item_width)
{
	ImGui::SetNextItemWidth(item_width);
}

void ImGuiWindow::PushItemWidth(float item_width)
{
	ImGui::PushItemWidth(item_width);
}

void ImGuiWindow::PopItemWidth()
{
	ImGui::PopItemWidth();
}

float ImGuiWindow::CalcItemWidth() const
{
	return ImGui::CalcItemWidth();
}

void ImGuiWindow::PushTextWrapPos(float wrap_local_pos_x)
{
	ImGui::PushTextWrapPos(wrap_local_pos_x);
}

void ImGuiWindow::PopTextWrapPos()
{
	ImGui::PopTextWrapPos();
}

// ── ID stack ─────────────────────────────────────────────────────────────────

void ImGuiWindow::PushID(const char* str_id)
{
	ImGui::PushID(str_id);
}

void ImGuiWindow::PushID(const char* str_id_begin, const char* str_id_end)
{
	ImGui::PushID(str_id_begin, str_id_end);
}

void ImGuiWindow::PushID(const void* ptr_id)
{
	ImGui::PushID(ptr_id);
}

void ImGuiWindow::PushID(int int_id)
{
	ImGui::PushID(int_id);
}

void ImGuiWindow::PopID()
{
	ImGui::PopID();
}

ImGuiID ImGuiWindow::GetID(const char* str_id) const
{
	return ImGui::GetID(str_id);
}

ImGuiID ImGuiWindow::GetID(const char* str_id_begin,
	const char* str_id_end) const
{
	return ImGui::GetID(str_id_begin, str_id_end);
}

ImGuiID ImGuiWindow::GetID(const void* ptr_id) const
{
	return ImGui::GetID(ptr_id);
}

ImGuiID ImGuiWindow::GetID(int int_id) const
{
	return ImGui::GetID(int_id);
}

// ── Scroll ────────────────────────────────────────────────────────────────────

float ImGuiWindow::GetScrollX() const
{
	return ImGui::GetScrollX();
}

float ImGuiWindow::GetScrollY() const
{
	return ImGui::GetScrollY();
}

void ImGuiWindow::SetScrollX(float scroll_x)
{
	ImGui::SetScrollX(scroll_x);
}

void ImGuiWindow::SetScrollY(float scroll_y)
{
	ImGui::SetScrollY(scroll_y);
}

float ImGuiWindow::GetScrollMaxX() const
{
	return ImGui::GetScrollMaxX();
}

float ImGuiWindow::GetScrollMaxY() const
{
	return ImGui::GetScrollMaxY();
}

void ImGuiWindow::SetScrollHereX(float center_x_ratio)
{
	ImGui::SetScrollHereX(center_x_ratio);
}

void ImGuiWindow::SetScrollHereY(float center_y_ratio)
{
	ImGui::SetScrollHereY(center_y_ratio);
}

void ImGuiWindow::SetScrollFromPosX(float local_x, float center_x_ratio)
{
	ImGui::SetScrollFromPosX(local_x, center_x_ratio);
}

void ImGuiWindow::SetScrollFromPosY(float local_y, float center_y_ratio)
{
	ImGui::SetScrollFromPosY(local_y, center_y_ratio);
}

// ── Focus / activation ────────────────────────────────────────────────────────

void ImGuiWindow::SetItemDefaultFocus()
{
	ImGui::SetItemDefaultFocus();
}

void ImGuiWindow::SetKeyboardFocusHere(int offset)
{
	ImGui::SetKeyboardFocusHere(offset);
}

void ImGuiWindow::SetNextItemAllowOverlap()
{
	ImGui::SetNextItemAllowOverlap();
}

// ── Item queries ──────────────────────────────────────────────────────────────

bool ImGuiWindow::IsItemHovered(ImGuiHoveredFlags flags) const
{
	return ImGui::IsItemHovered(flags);
}

bool ImGuiWindow::IsItemActive() const
{
	return ImGui::IsItemActive();
}

bool ImGuiWindow::IsItemFocused() const
{
	return ImGui::IsItemFocused();
}

bool ImGuiWindow::IsItemClicked(ImGuiMouseButton mouse_button) const
{
	return ImGui::IsItemClicked(mouse_button);
}

bool ImGuiWindow::IsItemVisible() const
{
	return ImGui::IsItemVisible();
}

bool ImGuiWindow::IsItemEdited() const
{
	return ImGui::IsItemEdited();
}

bool ImGuiWindow::IsItemActivated() const
{
	return ImGui::IsItemActivated();
}

bool ImGuiWindow::IsItemDeactivated() const
{
	return ImGui::IsItemDeactivated();
}

bool ImGuiWindow::IsItemDeactivatedAfterEdit() const
{
	return ImGui::IsItemDeactivatedAfterEdit();
}

bool ImGuiWindow::IsItemToggledOpen() const
{
	return ImGui::IsItemToggledOpen();
}

ImVec2 ImGuiWindow::GetItemRectMin() const
{
	return ImGui::GetItemRectMin();
}

ImVec2 ImGuiWindow::GetItemRectMax() const
{
	return ImGui::GetItemRectMax();
}

ImVec2 ImGuiWindow::GetItemRectSize() const
{
	return ImGui::GetItemRectSize();
}

// ── Miscellaneous ─────────────────────────────────────────────────────────────

ImDrawList* ImGuiWindow::GetWindowDrawList() const
{
	return ImGui::GetWindowDrawList();
}

const char* ImGuiWindow::GetClipboardText() const
{
	return ImGui::GetClipboardText();
}

void ImGuiWindow::SetClipboardText(const char* text)
{
	ImGui::SetClipboardText(text);
}

void ImGuiWindow::PushClipRect(ImVec2 clip_rect_min, ImVec2 clip_rect_max,
	bool intersect_with_current_clip_rect)
{
	ImGui::PushClipRect(clip_rect_min, clip_rect_max,
		intersect_with_current_clip_rect);
}

void ImGuiWindow::PopClipRect()
{
	ImGui::PopClipRect();
}

ImVec2 ImGuiWindow::CalcTextSize(const char* text, const char* text_end,
	bool hide_text_after_double_hash, float wrap_width) const
{
	return ImGui::CalcTextSize(text, text_end, hide_text_after_double_hash,
		wrap_width);
}

// ── Columns (legacy) ─────────────────────────────────────────────────────────

void ImGuiWindow::Columns(int count, const char* id, bool borders)
{
	ImGui::Columns(count, id, borders);
}

void ImGuiWindow::NextColumn()
{
	ImGui::NextColumn();
}

int ImGuiWindow::GetColumnIndex() const
{
	return ImGui::GetColumnIndex();
}

float ImGuiWindow::GetColumnWidth(int column_index) const
{
	return ImGui::GetColumnWidth(column_index);
}

void ImGuiWindow::SetColumnWidth(int column_index, float width)
{
	ImGui::SetColumnWidth(column_index, width);
}

float ImGuiWindow::GetColumnOffset(int column_index) const
{
	return ImGui::GetColumnOffset(column_index);
}

void ImGuiWindow::SetColumnOffset(int column_index, float offset_x)
{
	ImGui::SetColumnOffset(column_index, offset_x);
}

int ImGuiWindow::GetColumnsCount() const
{
	return ImGui::GetColumnsCount();
}

#include <alvere/application/window.hpp>

#include "editor/imgui_editor.hpp"
#include "editor/windows/tool_window.hpp"
#include "editor/windows/history_window.hpp"
#include "editor/tool/pan_tool.hpp"
#include "editor/tool/draw_tool.hpp"

ToolWindow::ToolWindow(ImGuiEditor & editor, alvere::Window & window)
	: m_window(window)
	, m_editor(editor)
	, m_currentTool(std::make_unique<PanTool>(editor, window))
{
}

void ToolWindow::Update(float deltaTime)
{
	m_currentTool->Update(deltaTime);
}

void ToolWindow::Draw()
{
	ImGui::Begin(GetName().c_str(), &m_visible, m_windowflags);

	if (ImGui::Button("Pan", m_toolButtonSize))
	{
		m_currentTool = std::make_unique<PanTool>(m_editor, m_window);
	}

	if (ImGui::Button("Draw", m_toolButtonSize))
	{
		EditorWorld & currentWorld = *m_editor.GetFocusedWorld();
		HistoryWindow * historyWindow = m_editor.GetEditorWindow<HistoryWindow>();
		m_currentTool = std::make_unique<DrawTool>(m_editor);
	}

	ImGui::End();

	m_currentTool->Render();
}
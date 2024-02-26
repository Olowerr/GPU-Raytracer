#pragma once

// Technically we don't need these includes here
// but I feel like they should be included when including "ImGuiHelper.h".
// So I'll just leave them here

#include "Window.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_glfw.h"

namespace Okay
{
	void initiateImGui(Window& window);

	void shutdownImGui();

	void newFrameImGui();

	void endFrameImGui();
}
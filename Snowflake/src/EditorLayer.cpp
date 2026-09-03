#include "EditorLayer.hpp"
#include "Theme.hpp"
#include <imgui_internal.h>
#include <cmath>
#include <cstring>
#include <format>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include "LogTags.hpp"
#include "ViewGizmo.hpp"
#include "Utility/ImGuiScale.hpp"

namespace {
	Snowflake::ViewGizmo::Vector3 ToGizmoBasis(const glm::vec3 engineVector) {
		return { engineVector.x, engineVector.z, -engineVector.y };
	}

	glm::vec3 FromGizmoBasis(const Snowflake::ViewGizmo::Vector3 gizmoVector) {
		return { gizmoVector.x, -gizmoVector.z, gizmoVector.y };
	}

	float WrapDegrees(const float degrees) {
		const float wrapped = std::fmod(degrees + 180.0f, 360.0f);

		return wrapped < 0.0f ? wrapped + 180.0f : wrapped - 180.0f;
	}

	void CollectLeafDockNodes(ImGuiDockNode* node, std::vector<ImGuiDockNode*>& out) {
		if (!node) {
			return;
		}

		if (node->IsLeafNode()) {
			if (!node->Windows.empty()) {
				out.push_back(node);
			}
			return;
		}

		CollectLeafDockNodes(node->ChildNodes[0], out);
		CollectLeafDockNodes(node->ChildNodes[1], out);
	}

	void SetupImGuiStyle() {
		using namespace Snowflake::Theme;

		const Palette& flavor = Flavor;
		ImGuiStyle& style = ImGui::GetStyle();

		style.Alpha = 1.0f;
		style.DisabledAlpha = 0.6f;
		style.WindowPadding = ImVec2(6.0f, 6.0f);
		style.WindowRounding = 8.0f;
		style.WindowBorderSize = 1.0f;
		style.WindowMinSize = ImVec2(20.0f, 32.0f);
		style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
		style.WindowMenuButtonPosition = ImGuiDir_Right;
		style.ChildRounding = 4.0f;
		style.ChildBorderSize = 1.0f;
		style.PopupRounding = 6.0f;
		style.PopupBorderSize = 1.0f;
		style.FramePadding = ImVec2(8.0f, 4.0f);
		style.FrameRounding = 4.0f;
		style.FrameBorderSize = 0.0f;
		style.ItemSpacing = ImVec2(6.0f, 4.0f);
		style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
		style.CellPadding = ImVec2(6.0f, 3.0f);
		style.IndentSpacing = 20.0f;
		style.ColumnsMinSpacing = 4.0f;
		style.ScrollbarSize = 12.0f;
		style.ScrollbarRounding = 6.0f;
		style.ScrollbarPadding = 3.0f;
		style.GrabMinSize = 10.0f;
		style.GrabRounding = 5.0f;
		style.TabRounding = 4.0f;
		style.TabBorderSize = 0.0f;
		style.TabBarOverlineSize = 2.0f;
		style.TabCloseButtonMinWidthUnselected = 0.0f;
		style.MenuItemRounding = 4.0f;
		style.SelectableRounding = 4.0f;
		style.SeparatorTextBorderSize = 1.0f;
		style.ColorButtonPosition = ImGuiDir_Right;
		style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
		style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

		style.Colors[ImGuiCol_Text] = flavor.text;
		style.Colors[ImGuiCol_TextDisabled] = flavor.overlay0;
		style.Colors[ImGuiCol_WindowBg] = flavor.base;
		style.Colors[ImGuiCol_ChildBg] = flavor.mantle;
		style.Colors[ImGuiCol_PopupBg] = flavor.mantle;
		style.Colors[ImGuiCol_Border] = AccentTint(0.55f);
		style.Colors[ImGuiCol_BorderShadow] = Transparent;
		style.Colors[ImGuiCol_FrameBg] = flavor.surface0;
		style.Colors[ImGuiCol_FrameBgHovered] = flavor.surface1;
		style.Colors[ImGuiCol_FrameBgActive] = flavor.surface2;
		style.Colors[ImGuiCol_TitleBg] = flavor.mantle;
		style.Colors[ImGuiCol_TitleBgActive] = flavor.surface0;
		style.Colors[ImGuiCol_TitleBgCollapsed] = flavor.crust;
		style.Colors[ImGuiCol_MenuBarBg] = flavor.mantle;
		style.Colors[ImGuiCol_ScrollbarBg] = Transparent;
		style.Colors[ImGuiCol_ScrollbarGrab] = flavor.surface1;
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = flavor.surface2;
		style.Colors[ImGuiCol_ScrollbarGrabActive] = Accent;
		style.Colors[ImGuiCol_CheckMark] = Accent;
		style.Colors[ImGuiCol_CheckboxSelectedBg] = AccentTint(0.25f);
		style.Colors[ImGuiCol_SliderGrab] = Accent;
		style.Colors[ImGuiCol_SliderGrabActive] = flavor.lavender;
		style.Colors[ImGuiCol_Button] = flavor.surface0;
		style.Colors[ImGuiCol_ButtonHovered] = flavor.surface1;
		style.Colors[ImGuiCol_ButtonActive] = AccentTint(0.55f);
		style.Colors[ImGuiCol_Header] = AccentTint(0.65f);
		style.Colors[ImGuiCol_HeaderHovered] = AccentTint(0.45f);
		style.Colors[ImGuiCol_HeaderActive] = AccentTint(0.85f);
		style.Colors[ImGuiCol_Separator] = flavor.surface1;
		style.Colors[ImGuiCol_SeparatorHovered] = AccentTint(0.7f);
		style.Colors[ImGuiCol_SeparatorActive] = Accent;
		style.Colors[ImGuiCol_ResizeGrip] = AccentTint(0.2f);
		style.Colors[ImGuiCol_ResizeGripHovered] = AccentTint(0.5f);
		style.Colors[ImGuiCol_ResizeGripActive] = AccentTint(0.75f);
		style.Colors[ImGuiCol_InputTextCursor] = flavor.rosewater;
		style.Colors[ImGuiCol_TabHovered] = flavor.surface1;
		style.Colors[ImGuiCol_Tab] = flavor.mantle;
		style.Colors[ImGuiCol_TabSelected] = flavor.base;
		style.Colors[ImGuiCol_TabSelectedOverline] = Accent;
		style.Colors[ImGuiCol_TabDimmed] = flavor.crust;
		style.Colors[ImGuiCol_TabDimmedSelected] = flavor.base;
		style.Colors[ImGuiCol_TabDimmedSelectedOverline] = flavor.overlay0;
		style.Colors[ImGuiCol_DockingPreview] = AccentTint(0.35f);
		style.Colors[ImGuiCol_DockingEmptyBg] = flavor.crust;
		style.Colors[ImGuiCol_PlotLines] = flavor.lavender;
		style.Colors[ImGuiCol_PlotLinesHovered] = Accent;
		style.Colors[ImGuiCol_PlotHistogram] = Accent;
		style.Colors[ImGuiCol_PlotHistogramHovered] = flavor.pink;
		style.Colors[ImGuiCol_TableHeaderBg] = flavor.mantle;
		style.Colors[ImGuiCol_TableBorderStrong] = flavor.surface1;
		style.Colors[ImGuiCol_TableBorderLight] = flavor.surface0;
		style.Colors[ImGuiCol_TableRowBg] = Transparent;
		style.Colors[ImGuiCol_TableRowBgAlt] = WithAlpha(flavor.surface0, 0.4f);
		style.Colors[ImGuiCol_TextLink] = Accent;
		style.Colors[ImGuiCol_TextSelectedBg] = AccentTint(0.35f);
		style.Colors[ImGuiCol_TreeLines] = flavor.surface1;
		style.Colors[ImGuiCol_DragDropTarget] = Accent;
		style.Colors[ImGuiCol_DragDropTargetBg] = AccentTint(0.15f);
		style.Colors[ImGuiCol_UnsavedMarker] = flavor.peach;
		style.Colors[ImGuiCol_NavCursor] = Accent;
		style.Colors[ImGuiCol_NavWindowingHighlight] = AccentTint(0.7f);
		style.Colors[ImGuiCol_NavWindowingDimBg] = Scrim;
		style.Colors[ImGuiCol_ModalWindowDimBg] = Scrim;
	}

	void ApplyImGuiScale(const float scale) {
		ImGui::GetStyle() = ImGuiStyle();
		SetupImGuiStyle();

		ImGuiStyle& style = ImGui::GetStyle();
		style._MainScale *= scale;
    	style.WindowPadding = { style.WindowPadding.x * scale, style.WindowPadding.y * scale } ;
    	style.WindowRounding = style.WindowRounding * scale;
    	style.WindowBorderSize = style.WindowBorderSize * scale;
    	style.WindowMinSize = { style.WindowMinSize.x * scale, style.WindowMinSize.y * scale };
    	style.WindowBorderHoverPadding = style.WindowBorderHoverPadding * scale;
    	style.ChildRounding = style.ChildRounding * scale;
    	style.ChildBorderSize = style.ChildBorderSize * scale;
    	style.PopupRounding = style.PopupRounding * scale;
    	style.PopupBorderSize = style.PopupBorderSize * scale;
    	style.FramePadding = { style.FramePadding.x * scale, style.FramePadding.y * scale };
    	style.FrameBorderSize = style.FrameBorderSize * scale;
    	style.FrameRounding = style.FrameRounding * scale;
    	style.ItemSpacing = { style.ItemSpacing.x * scale, style.ItemSpacing.y * scale };
    	style.ItemInnerSpacing = { style.ItemInnerSpacing.x * scale, style.ItemInnerSpacing.y * scale };
    	style.CellPadding = { style.CellPadding.x * scale, style.CellPadding.y * scale };
    	style.TouchExtraPadding = { style.TouchExtraPadding.x * scale, style.TouchExtraPadding.y * scale };
    	style.IndentSpacing = style.IndentSpacing * scale;
    	style.ColumnsMinSpacing = style.ColumnsMinSpacing * scale;
    	style.ScrollbarSize = style.ScrollbarSize * scale;
    	style.ScrollbarRounding = style.ScrollbarRounding * scale;
    	style.ScrollbarPadding = style.ScrollbarPadding * scale;
    	style.GrabMinSize = style.GrabMinSize * scale;
    	style.GrabRounding = style.GrabRounding * scale;
    	style.LogSliderDeadzone = style.LogSliderDeadzone * scale;
    	style.ImageRounding = style.ImageRounding * scale;
    	style.ImageBorderSize = style.ImageBorderSize * scale;
    	style.TabRounding = style.TabRounding * scale;
    	style.TabBorderSize = style.TabBorderSize * scale;
    	style.TabMinWidthBase = style.TabMinWidthBase * scale;
    	style.TabMinWidthShrink = style.TabMinWidthShrink * scale;
    	style.TabCloseButtonMinWidthSelected = (style.TabCloseButtonMinWidthSelected > 0.0f && style.TabCloseButtonMinWidthSelected != FLT_MAX) ? style.TabCloseButtonMinWidthSelected * scale : style.TabCloseButtonMinWidthSelected;
    	style.TabCloseButtonMinWidthUnselected = (style.TabCloseButtonMinWidthUnselected > 0.0f && style.TabCloseButtonMinWidthUnselected != FLT_MAX) ? style.TabCloseButtonMinWidthUnselected * scale : style.TabCloseButtonMinWidthUnselected;
    	style.TabBarBorderSize = style.TabBarBorderSize * scale;
    	style.TabBarOverlineSize = style.TabBarOverlineSize * scale;
    	style.TreeLinesSize = style.TreeLinesSize * scale;
    	style.TreeLinesRounding = style.TreeLinesRounding * scale;
    	style.MenuItemRounding = style.MenuItemRounding * scale;
    	style.SelectableRounding = style.SelectableRounding * scale;
    	style.DragDropTargetRounding = style.DragDropTargetRounding * scale;
    	style.DragDropTargetBorderSize = style.DragDropTargetBorderSize * scale;
    	style.DragDropTargetPadding = style.DragDropTargetPadding * scale;
    	style.ColorMarkerSize = style.ColorMarkerSize * scale;
    	style.InputTextCursorSize = style.InputTextCursorSize * scale;
    	style.SeparatorSize = style.SeparatorSize * scale;
    	style.SeparatorTextBorderSize = style.SeparatorTextBorderSize * scale;
    	style.SeparatorTextPadding = { style.SeparatorTextPadding.x * scale, style.SeparatorTextPadding.y * scale };
    	style.DockingSeparatorSize = style.DockingSeparatorSize * scale;
    	style.DisplayWindowPadding = { style.DisplayWindowPadding.x * scale, style.DisplayWindowPadding.y * scale };
    	style.DisplaySafeAreaPadding = { style.DisplaySafeAreaPadding.x * scale, style.DisplaySafeAreaPadding.y * scale };
    	style.MouseCursorScale = style.MouseCursorScale * scale;
		style.FontScaleDpi = scale;
	}

	struct MnemonicAnchor {
		ImDrawList* drawList;
		ImVec2 textPos;
	};

	MnemonicAnchor CaptureMnemonicAnchor() {
		const ImGuiWindow* window = ImGui::GetCurrentWindowRead();
		const ImGuiStyle& style = ImGui::GetStyle();

		const float labelOffset = window->DC.MenuColumns.OffsetLabel;
		const float barOffset = window->DC.LayoutType == ImGuiLayoutType_Horizontal ? IM_TRUNC(style.ItemSpacing.x * 0.5f) : 0.0f;

		return MnemonicAnchor{
			.drawList = window->DrawList,
			.textPos = ImVec2(window->DC.CursorPos.x + barOffset + labelOffset, window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset)
		};
	}

	void DrawMnemonicUnderline(const MnemonicAnchor& anchor, const char* label, const int32_t mnemonicIndex) {
		if (mnemonicIndex < 0) {
			return;
		}

		const char* labelEnd = ImGui::FindRenderedTextEnd(label);
		if (label + mnemonicIndex >= labelEnd) {
			return;
		}

		const float left = anchor.textPos.x + ImGui::CalcTextSize(label, label + mnemonicIndex).x;
		const float right = anchor.textPos.x + ImGui::CalcTextSize(label, label + mnemonicIndex + 1).x;
		const float top = std::round(anchor.textPos.y + ImGui::GetFontSize() - 1.0f);

		anchor.drawList->AddRectFilled(ImVec2(std::round(left), top), ImVec2(std::round(right), top + 1.0f), ImGui::GetColorU32(ImGuiCol_Text));
	}

	ImGuiKey MnemonicKey(const char character) {
		const char upper = character >= 'a' && character <= 'z' ? static_cast<char>(character - 'a' + 'A') : character;

		if (upper < 'A' || upper > 'Z') {
			return ImGuiKey_None;
		}

		return static_cast<ImGuiKey>(ImGuiKey_A + (upper - 'A'));
	}

	bool BeginMenuMnemonic(const char* label, const int32_t mnemonicIndex) {
		if (const ImGuiKey key = MnemonicKey(label[mnemonicIndex]); key != ImGuiKey_None) {
			ImGui::SetNextItemShortcut(ImGuiMod_Alt | key, ImGuiInputFlags_RouteGlobal);
		}

		const MnemonicAnchor anchor = CaptureMnemonicAnchor();
		const bool open = ImGui::BeginMenu(label);
		DrawMnemonicUnderline(anchor, label, mnemonicIndex);

		return open;
	}

	bool MenuItemMnemonic(const char* label, const int32_t mnemonicIndex, const bool selected = false) {
		if (const ImGuiKey key = MnemonicKey(label[mnemonicIndex]); key != ImGuiKey_None) {
			ImGui::SetNextItemShortcut(key);
		}

		const MnemonicAnchor anchor = CaptureMnemonicAnchor();
		const bool pressed = ImGui::MenuItem(label, nullptr, selected);
		DrawMnemonicUnderline(anchor, label, mnemonicIndex);

		return pressed;
	}

	bool AltOnly(const ImGuiIO& io) {
		return io.KeyAlt && !io.KeyCtrl && !io.KeyShift && !io.KeySuper;
	}

	char ToLowerAscii(const char character) {
		return character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a') : character;
	}

	bool FuzzyMatch(const char* text, const char* query) {
		for (const char* q = query; *q != '\0'; q++) {
			if (*q == ' ') {
				continue;
			}

			const char wanted = ToLowerAscii(*q);

			while (*text != '\0' && ToLowerAscii(*text) != wanted) {
				text++;
			}

			if (*text == '\0') {
				return false;
			}

			text++;
		}

		return true;
	}

	void ApplyDockResize(ImGuiDockNode* node, const ImGuiAxis axis, const float amount, const float minSize) {
		if (amount == 0.0f) {
			return;
		}

		for (ImGuiDockNode* child = node; child->ParentNode != nullptr; child = child->ParentNode) {
			ImGuiDockNode* parent = child->ParentNode;

			if (parent->SplitAxis != axis) {
				continue;
			}

			ImGuiDockNode* sibling = parent->ChildNodes[0] == child ? parent->ChildNodes[1] : parent->ChildNodes[0];
			if (!sibling) {
				continue;
			}

			const float wanted = parent->ChildNodes[0] == child ? amount : -amount;
			const float childSize = child->Size[axis] + wanted;
			const float siblingSize = sibling->Size[axis] - wanted;

			if (childSize < minSize || siblingSize < minSize) {
				return;
			}

			child->SizeRef[axis] = childSize;
			sibling->SizeRef[axis] = siblingSize;
			return;
		}
	}

	void DrawStyleColorsDebugWindow() {
		static bool open = true;
		static char filter[64]{};

		if (!open) {
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(Cori::Utility::ScaleUIUnit(560.0f), Cori::Utility::ScaleUIUnit(640.0f)), ImGuiCond_FirstUseEver);

		if (ImGui::Begin("Style Colors (debug)", &open)) {
			const ImGuiStyle& style = ImGui::GetStyle();

			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputTextWithHint("##stylecolorfilter", "Filter...", filter, sizeof(filter));

			constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY |
				ImGuiTableFlags_SizingFixedFit;

			if (ImGui::BeginTable("##stylecolors", 4, tableFlags)) {
				ImGui::TableSetupColumn("##swatch", ImGuiTableColumnFlags_WidthFixed, 44.0f);
				ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 44.0f);
				ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 240.0f);
				ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableHeadersRow();

				for (int32_t i = 0; i < ImGuiCol_COUNT; i++) {
					const char* name = ImGui::GetStyleColorName(i);

					if (!FuzzyMatch(name, filter)) {
						continue;
					}

					const ImVec4& color = style.Colors[i];

					const int32_t r = static_cast<int32_t>(color.x * 255.0f + 0.5f);
					const int32_t g = static_cast<int32_t>(color.y * 255.0f + 0.5f);
					const int32_t b = static_cast<int32_t>(color.z * 255.0f + 0.5f);
					const int32_t a = static_cast<int32_t>(color.w * 255.0f + 0.5f);

					ImGui::TableNextRow();
					ImGui::PushID(i);

					ImGui::TableSetColumnIndex(0);
					ImGui::ColorButton("##preview", color,
						ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
						ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight()));

					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%d", i);

					ImGui::TableSetColumnIndex(2);
					ImGui::TextUnformatted(name);

					ImGui::TableSetColumnIndex(3);
					ImGui::Text("%.3f %.3f %.3f %.3f", color.x, color.y, color.z, color.w);

					if (ImGui::IsItemClicked()) {
						ImGui::SetClipboardText(std::format("#{:02X}{:02X}{:02X}{:02X}", r, g, b, a).c_str());
					}

					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("%s\n#%02X%02X%02X%02X\nrgba(%d, %d, %d, %d)\nClick to copy hex", name, r, g, b, a, r, g, b, a);
					}

					ImGui::PopID();
				}

				ImGui::EndTable();
			}
		}

		ImGui::End();
	}

	ImGuiWindow* GetSelectedWindow(ImGuiDockNode* node) {
		if (!node || node->Windows.empty()) {
			return nullptr;
		}

		for (ImGuiWindow* window : node->Windows) {
			if (window->TabId == node->SelectedTabId) {
				return window;
			}
		}

		return node->Windows[0];
	}
}

namespace Snowflake {
	EditorLayer::EditorLayer()
		: Layer("Snowflake Editor") {
		ImGuiIO& io = ImGui::GetIO();

		ViewGizmo::Initialize();

		io.Fonts->AddFontFromFileTTF((Cori::FileSystem::PathManager::GetAliasedPath("ASSET_DIR") / "fonts/ttf/JetBrainsMono-Regular.ttf").c_str(), 16.0f);

		Cori::Core::Window& window = Cori::Core::Application::GetWindow();

		if (window.GetWindowMode() != Cori::Core::WindowMode::RESIZABLE) {
			if (const auto result = window.SetWindowMode(Cori::Core::WindowMode::RESIZABLE); !result) {
				CORI_ERROR("Failed to put the editor window into resizable mode. Error: {}", result.error().what());
			}
		}

		const vk::Extent2D initialPRTExtent{static_cast<uint32_t>(window.GetPixelWidth()), static_cast<uint32_t>(window.GetPixelHeight())};

		m_PanelExtent = initialPRTExtent;
		m_PRTExtent = initialPRTExtent;

		//FIXME: move the scene ownership to a different layer completely, also add a way to disable keyboard and mouse event ignoring (e.g. ImGuiLayer::OnEvent) when focused on the viewport
		if (CreateTestScene(initialPRTExtent)) {
			LoadSponza();
		}
		Cori::Graphics::MasterRenderer::ChangeCompositeMode(Cori::Graphics::MasterRenderer::Mode::eDockSpace);
		ApplyImGuiScale(window.GetDisplayScale());
	}

	EditorLayer::~EditorLayer() {
		if (m_CameraCaptureActive) {
			Cori::Core::Input::SetRelativeMouseMode(false);
			ImGui::GetIO().ConfigFlags &= ~(ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange);
			m_CameraCaptureActive = false;
		}
	}

	bool EditorLayer::CreateTestScene(const vk::Extent2D initialPRTExtent) {
		const auto scene = Cori::World::SceneManager::CreateScene(s_SceneName);
		if (!scene) {
			CORI_ERROR("Failed to create scene '{}'. Error: {}", s_SceneName, scene.error().what());
			return false;
		}

		m_MainScene = scene.value();

		auto& camera = m_MainScene.GetActiveCamera();
		camera.CreatePerspectiveCamera(s_CameraFovY, static_cast<float>(initialPRTExtent.width) / static_cast<float>(initialPRTExtent.height), s_CameraNearPlane, s_CameraFarPlane);
		camera.SetPosition3D(m_CameraPosition);
		camera.SetYawPitch(m_CameraYaw, m_CameraPitch);
		camera.RecalculateVP();

		Cori::Graphics::SceneRenderer::CreateInfo info{
			.initialPRTExtent = initialPRTExtent,
			.PRTFormat = vk::Format::eR8G8B8A8Unorm,
			#ifdef DEBUG_BUILD
			.name = "Snowflake viewport",
			#endif
			.registerPRTWithImGui = true
		};

		m_MainScene.RegisterSystem<Cori::World::Systems::RenderSync>(std::move(info));

		auto renderSync = m_MainScene.GetSystem<Cori::World::Systems::RenderSync>();
		if (!renderSync) {
			CORI_ERROR("Failed to retrieve the RenderSync system of '{}'. Error: {}", s_SceneName, renderSync.error().what());
			return false;
		}

		m_RenderSync = renderSync.value();

		if (const auto locked = m_RenderSync.lock()) {
			locked->Bind();
		}

		return true;
	}

	void EditorLayer::LoadSponza() {
		static constexpr uint8_t sponzaMeshMaterials[] = {
			 0,  3,  1,  4,  5,  6,  7,  8,  6,  9,  7,  6, 10,  5,  7,  5,  6,  7,  6,  7,
			 6,  7,  6,  7,  6,  7,  6,  7,  6,  7,  6,  7,  6,  7,  6,  5,  6,  5, 11,  5,
			11,  5, 11,  5, 10,  5,  9,  8,  6, 12,  2,  5, 13,  0, 14, 15, 16, 14, 15, 14,
			16, 15, 13, 17, 18, 19, 18, 19, 18, 17, 19, 18, 17, 20, 21, 20, 21, 20, 21, 20,
			21,  3,  1,  3,  1,  3,  1,  3,  1,  3,  1,  3,  1,  3,  1, 22, 23,  4, 23,  4,
			 5, 24,  5
		};

		static constexpr uint32_t sponzaMaterialCount = 25;

		constexpr float sponzaScale = 1.0f;
		const glm::vec3 sponzaOffset{ 0.0f, 0.0f, 0.0f };

		std::vector<Cori::Core::AssetRef<Cori::Graphics::Material>> sponzaMaterials;
		sponzaMaterials.reserve(sponzaMaterialCount);

		for (uint32_t i = 0; i < sponzaMaterialCount; i++) {
			sponzaMaterials.emplace_back(Cori::Core::AssetManager2::Load<Cori::Graphics::Material>(
				std::format("assets://Sponza/materials/Sponza_Mat_{:02}.json", i).c_str()));
		}

		for (uint32_t i = 0; i < std::size(sponzaMeshMaterials); i++) {
			auto sponzaMesh = Cori::Core::AssetManager2::Load<Cori::Graphics::Mesh>(
				std::format("assets://Sponza/meshes/Sponza_Mesh_{:03}.json", i).c_str());

			auto sponzaEntity = m_MainScene.CreateEntity(std::format("Sponza_{:03}", i));
			sponzaEntity.AddComponent<Cori::World::Components::Entity::Rendering>(
				std::move(sponzaMesh), sponzaMaterials[sponzaMeshMaterials[i]], glm::vec4{ 0.0f, 0.0f, 1.0f, 1.0f });

			auto& sponzaTc = sponzaEntity.GetComponents<Cori::World::Components::Entity::Transform>();
			sponzaTc.SetLocalScale({ sponzaScale, sponzaScale, sponzaScale });
			sponzaTc.SetLocalPosition(sponzaOffset);
		}

		CORI_INFO("Sponza: requested {} meshes over {} materials", std::size(sponzaMeshMaterials), sponzaMaterialCount);
	}

	void EditorLayer::OnEvent(Cori::Core::Event& event) {
		Cori::Core::EventDispatcher dispatcher(event);

		dispatcher.Dispatch<Cori::Core::WindowDisplayScaleChangedEvent>([](const Cori::Core::WindowDisplayScaleChangedEvent& e) -> bool {
			ApplyImGuiScale(e.GetScale());
			return false;
		});
	}

	void EditorLayer::OnImGuiRender([[maybe_unused]] Cori::Core::GameTimer& gameTimer) {
		ImGuizmo::BeginFrame();
		ViewGizmo::BeginFrame();

		UpdateWindowManipulation();
		UpdateShortcuts();
		UpdateGizmoShortcuts();

		DrawDockSpace();
		DrawViewport();
		DrawConsole();
		DrawAssetBrowser();
		DrawInspector();
		DrawWindowSettings();
		DrawLauncher();

		m_Browser.Draw(nullptr);

		UpdateDockNavigation();
		//ImGui::ShowDebugLogWindow();
		//ImGui::ShowIDStackToolWindow();
		//DrawStyleColorsDebugWindow();

		m_MainScene.OnImGuiRender(gameTimer);
	}

	std::array<EditorLayer::PanelEntry, 4> EditorLayer::GetPanels() {
		return {
			PanelEntry{ s_ViewportPanel, nullptr },
			PanelEntry{ Cori::ConsolePanel::s_DefaultName, &m_ShowConsole },
			PanelEntry{ s_InspectorPanel, &m_ShowInspector },
			PanelEntry{ s_WindowSettingsWindow, &m_ShowWindowSettings }
		};
	}

	void EditorLayer::UpdateGizmoShortcuts() {
		if (m_CameraCaptureActive || !m_ViewportFocused || ImGui::GetIO().WantTextInput) {
			return;
		}

		const ImGuiIO& io = ImGui::GetIO();

		if (io.KeyCtrl || io.KeyAlt || io.KeySuper) {
			return;
		}

		if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
			m_GizmoOperation = ImGuizmo::TRANSLATE;
		}

		if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
			m_GizmoOperation = ImGuizmo::ROTATE;
		}

		if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
			m_GizmoOperation = ImGuizmo::SCALE;
		}

		if (ImGui::IsKeyPressed(ImGuiKey_X, false)) {
			m_GizmoAABBCorrection = !m_GizmoAABBCorrection;
		}
	}

	void EditorLayer::UpdateShortcuts() {
		const ImGuiIO& io = ImGui::GetIO();

		if (!AltOnly(io)) {
			return;
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
			ImGui::GetCurrentContext()->NavWindowingToggleLayer = false;

			if (m_ShowLauncher) {
				CloseLauncher();
			}
			else {
				OpenLauncher();
			}
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) {
			ImGui::GetCurrentContext()->NavWindowingToggleLayer = false;
			CloseFocusedWindow();
		}
	}

	void EditorLayer::CloseFocusedWindow() {
		const ImGuiContext& context = *ImGui::GetCurrentContext();

		if (context.NavWindow == nullptr) {
			return;
		}

		for (const PanelEntry& panel : GetPanels()) {
			if (panel.open != nullptr && std::strcmp(panel.name, context.NavWindow->Name) == 0) {
				*panel.open = false;
				return;
			}
		}
	}

	void EditorLayer::OpenLauncher() {
		m_ShowLauncher = true;
		m_LauncherJustOpened = true;
		m_LauncherSelection = 0;
		m_LauncherQuery[0] = '\0';
	}

	void EditorLayer::CloseLauncher() {
		m_ShowLauncher = false;
		m_LauncherJustOpened = false;
		m_LauncherSelection = 0;
		m_LauncherQuery[0] = '\0';
	}

	void EditorLayer::ActivatePanel(const PanelEntry& panel) {
		if (panel.open != nullptr) {
			*panel.open = true;
		}

		ImGui::SetWindowFocus(panel.name);

		CloseLauncher();
	}

	void EditorLayer::DrawLauncher() {
		if (!m_ShowLauncher) {
			return;
		}

		const ImGuiViewport* viewport = ImGui::GetMainViewport();

		ImGui::SetNextWindowPos(
			ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + viewport->WorkSize.y * s_LauncherAnchorRatio),
			ImGuiCond_Always,
			ImVec2(0.5f, 0.0f));

		ImGui::SetNextWindowSize({ Cori::Utility::ScaleUIUnit(s_LauncherWidth), 0.0f }, ImGuiCond_Always);

		if (m_LauncherJustOpened) {
			ImGui::SetNextWindowFocus();
		}

		constexpr ImGuiWindowFlags launcherFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar;

		if (!ImGui::Begin("##SnowflakeLauncher", nullptr, launcherFlags)) {
			ImGui::End();
			return;
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
			ImGui::End();
			CloseLauncher();
			return;
		}

		if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
			m_LauncherSelection++;
		}

		if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
			m_LauncherSelection--;
		}

		if (m_LauncherJustOpened) {
			ImGui::SetKeyboardFocusHere();
			m_LauncherJustOpened = false;
		}

		ImGui::SetNextItemWidth(-FLT_MIN);
		const bool submitted = ImGui::InputTextWithHint("##query", "Open panel...", m_LauncherQuery, sizeof(m_LauncherQuery), ImGuiInputTextFlags_EnterReturnsTrue);

		const std::array<PanelEntry, 4> panels = GetPanels();

		std::array<const PanelEntry*, 4> matches{};
		int32_t matchCount = 0;

		for (const PanelEntry& panel : panels) {
			if (FuzzyMatch(panel.name, m_LauncherQuery)) {
				matches[matchCount] = &panel;
				matchCount++;
			}
		}

		if (matchCount == 0) {
			ImGui::Separator();
			ImGui::TextDisabled("No matches");
			ImGui::End();
			return;
		}

		m_LauncherSelection = std::clamp(m_LauncherSelection, 0, matchCount - 1);

		ImGui::Separator();

		const PanelEntry* chosen = nullptr;

		ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);

		const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
		const bool mouseMoved = mouseDelta.x != 0.0f || mouseDelta.y != 0.0f;

		for (int32_t i = 0; i < matchCount; i++) {
			const PanelEntry& panel = *matches[i];

			if (ImGui::Selectable(panel.name, i == m_LauncherSelection)) {
				chosen = &panel;
			}

			if (mouseMoved && ImGui::IsItemHovered()) {
				m_LauncherSelection = i;
			}

			if (panel.open != nullptr && !*panel.open) {
				ImGui::SameLine();
				ImGui::TextDisabled("(closed)");
			}
		}

		ImGui::PopItemFlag();

		if (submitted) {
			chosen = matches[m_LauncherSelection];
		}

		const bool lostFocus = !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		ImGui::End();

		if (chosen != nullptr) {
			ActivatePanel(*chosen);
		}
		else if (lostFocus) {
			CloseLauncher();
		}
	}

	void EditorLayer::UpdateWindowManipulation() {
		ImGuiContext& context = *ImGui::GetCurrentContext();
		const ImGuiIO& io = ImGui::GetIO();

		if (!AltOnly(io)) {
			m_WindowManipulation = WindowManipulation::eNone;
			m_ManipulatedWindow = 0;
			return;
		}

		if (m_WindowManipulation == WindowManipulation::eNone) {
			const bool move = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
			const bool resize = ImGui::IsMouseClicked(ImGuiMouseButton_Right);

			if (!move && !resize) {
				return;
			}

			if (context.HoveredWindow == nullptr) {
				return;
			}

			m_ManipulatedWindow = context.HoveredWindow->ID;
			m_WindowManipulation = move ? WindowManipulation::eMove : WindowManipulation::eResize;

			context.NavWindowingToggleLayer = false;
		}

		const bool held = m_WindowManipulation == WindowManipulation::eMove
			? ImGui::IsMouseDown(ImGuiMouseButton_Left)
			: ImGui::IsMouseDown(ImGuiMouseButton_Right);

		ImGuiWindow* window = ImGui::FindWindowByID(m_ManipulatedWindow);

		if (!held || window == nullptr) {
			m_WindowManipulation = WindowManipulation::eNone;
			m_ManipulatedWindow = 0;
			return;
		}

		if (m_WindowManipulation == WindowManipulation::eMove) {
			if (context.MovingWindow == nullptr) {
				ImGui::StartMouseMovingWindowOrNode(window, window->DockNode, true);
				return;
			}

			if (const ImGuiWindow* moving = context.MovingWindow->RootWindowDockTree) {
				const float band = ImGui::GetFrameHeight();
				context.ActiveIdClickOffset.x = std::clamp(context.ActiveIdClickOffset.x, 0.0f, std::max(moving->SizeFull.x - 1.0f, 0.0f));
				context.ActiveIdClickOffset.y = std::clamp(context.ActiveIdClickOffset.y, 0.0f, std::max(band - 1.0f, 0.0f));
			}

			return;
		}

		ResizeWindow(window, io.MouseDelta);
	}

	void EditorLayer::ResizeWindow(ImGuiWindow* window, const ImVec2 delta) {
		if (delta.x == 0.0f && delta.y == 0.0f) {
			return;
		}

		if (window->DockNode == nullptr) {
			ImGuiWindow* root = window->RootWindow;
			const ImVec2 size(std::max(root->Size.x + delta.x, Cori::Utility::ScaleUIUnit(s_MinFloatingWindowSize)), std::max(root->Size.y + delta.y, Cori::Utility::ScaleUIUnit(s_MinFloatingWindowSize)));
			ImGui::SetWindowSize(root, size, ImGuiCond_Always);
			return;
		}

		ApplyDockResize(window->DockNode, ImGuiAxis_X, delta.x, Cori::Utility::ScaleUIUnit(s_MinDockNodeSize));
		ApplyDockResize(window->DockNode, ImGuiAxis_Y, delta.y, Cori::Utility::ScaleUIUnit(s_MinDockNodeSize));
	}

	void EditorLayer::UpdateDockNavigation() {
		const ImGuiIO& io = ImGui::GetIO();

		if (!AltOnly(io)) {
			return;
		}

		ImGuiDir direction = ImGuiDir_None;

		if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false)) {
			direction = ImGuiDir_Left;
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)) {
			direction = ImGuiDir_Right;
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) {
			direction = ImGuiDir_Up;
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) {
			direction = ImGuiDir_Down;
		}

		if (direction == ImGuiDir_None) {
			return;
		}

		FocusNeighbourDockNode(direction);
	}

	void EditorLayer::FocusNeighbourDockNode(const ImGuiDir direction) {
		ImGuiDockNode* root = ImGui::DockBuilderGetNode(m_DockSpaceID);
		if (!root) {
			return;
		}

		static std::vector<ImGuiDockNode*> leaves;
		leaves.clear();
		CollectLeafDockNodes(root, leaves);

		if (leaves.size() < 2) {
			return;
		}

		const ImGuiWindow* navWindow = ImGui::GetCurrentContext()->NavWindow;
		ImGuiDockNode* current = navWindow ? navWindow->DockNode : nullptr;

		if (!current || ImGui::DockNodeGetRootNode(current) != root) {
			if (ImGuiWindow* entry = GetSelectedWindow(leaves.front())) {
				ImGui::FocusWindow(entry);
			}
			return;
		}

		const ImVec2 from(current->Pos.x + current->Size.x * 0.5f, current->Pos.y + current->Size.y * 0.5f);

		ImGuiDockNode* best = nullptr;
		float bestScore = std::numeric_limits<float>::max();

		for (ImGuiDockNode* node : leaves) {
			if (node == current) {
				continue;
			}

			const ImVec2 to(node->Pos.x + node->Size.x * 0.5f, node->Pos.y + node->Size.y * 0.5f);

			float along = 0.0f;
			float across = 0.0f;

			switch (direction) {
			case ImGuiDir_Left:
				along = from.x - to.x;
				across = std::fabs(to.y - from.y);
				break;
			case ImGuiDir_Right:
				along = to.x - from.x;
				across = std::fabs(to.y - from.y);
				break;
			case ImGuiDir_Up:
				along = from.y - to.y;
				across = std::fabs(to.x - from.x);
				break;
			case ImGuiDir_Down:
				along = to.y - from.y;
				across = std::fabs(to.x - from.x);
				break;
			default:
				return;
			}

			if (along <= 0.0f) {
				continue;
			}

			const float score = along + across * 2.0f;

			if (score < bestScore) {
				bestScore = score;
				best = node;
			}
		}

		if (ImGuiWindow* target = GetSelectedWindow(best)) {
			ImGui::FocusWindow(target);
		}
	}

	void EditorLayer::DrawConsole() {
		m_Console.Draw(&m_ShowConsole);
	}

	void EditorLayer::DrawAssetBrowser() {
		//m_AssetBrowser.Draw(&m_ShowAssetBrowser);

	}

	void EditorLayer::DrawDockSpace() {
		const ImGuiViewport* viewport = ImGui::GetMainViewport();

		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		constexpr ImGuiWindowFlags hostFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

		ImGui::Begin(s_DockHostWindow, nullptr, hostFlags);

		ImGui::PopStyleVar(3);

		DrawMenuBar();

		m_DockSpaceID = ImGui::GetID(s_DockSpaceID);

		if (ImGui::DockBuilderGetNode(m_DockSpaceID) == nullptr || m_RebuildLayout) {
			m_RebuildLayout = false;
			BuildDefaultLayout(m_DockSpaceID, ImGui::GetContentRegionAvail());
		}

		ImGui::DockSpace(m_DockSpaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

		ImGui::End();
	}

	void EditorLayer::BuildDefaultLayout(const ImGuiID dockSpaceID, const ImVec2 dockSpaceSize) {
		ImGui::DockBuilderRemoveNode(dockSpaceID);
		ImGui::DockBuilderAddNode(dockSpaceID, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockSpaceID, dockSpaceSize);

		ImGuiID bottomNode = 0;
		ImGuiID upperNode = 0;
		ImGui::DockBuilderSplitNode(dockSpaceID, ImGuiDir_Down, 0.34f, &bottomNode, &upperNode);

		ImGuiID inspectorNode = 0;
		ImGuiID viewportNode = 0;
		ImGui::DockBuilderSplitNode(upperNode, ImGuiDir_Right, 0.24f, &inspectorNode, &viewportNode);

		ImGui::DockBuilderDockWindow(s_ViewportPanel, viewportNode);
		ImGui::DockBuilderDockWindow(s_InspectorPanel, inspectorNode);
		ImGui::DockBuilderDockWindow(Cori::ConsolePanel::s_DefaultName, bottomNode);
		//ImGui::DockBuilderDockWindow(AssetBrowserPanel::s_DefaultName, bottomNode);
		ImGui::DockBuilderFinish(dockSpaceID);
	}

	void EditorLayer::DrawMenuBar() {
		if (!ImGui::BeginMenuBar()) {
			return;
		}

		if (BeginMenuMnemonic("File", 0)) {
			ImGui::TextDisabled("Nothing here yet");
			ImGui::EndMenu();
		}

		if (BeginMenuMnemonic("Edit", 0)) {
			ImGui::TextDisabled("Nothing here yet");
			ImGui::EndMenu();
		}

		if (BeginMenuMnemonic("Assets", 0)) {
			//if (MenuItemMnemonic(AssetBrowserPanel::s_DefaultName, 0, m_ShowAssetBrowser)) {
			//	m_ShowAssetBrowser = !m_ShowAssetBrowser;
			//}
			ImGui::EndMenu();
		}

		if (BeginMenuMnemonic("Entity", 1)) {
			if (MenuItemMnemonic("Create Cube", 0)) {
				CreatePlaceholderEntity();
			}
			ImGui::EndMenu();
		}

		if (BeginMenuMnemonic("View", 0)) {
			if (MenuItemMnemonic(Cori::ConsolePanel::s_DefaultName, 0, m_ShowConsole)) {
				m_ShowConsole = !m_ShowConsole;
			}

			if (MenuItemMnemonic(s_InspectorPanel, 0, m_ShowInspector)) {
				m_ShowInspector = !m_ShowInspector;
			}

			//if (MenuItemMnemonic(AssetBrowserPanel::s_DefaultName, 0, m_ShowAssetBrowser)) {
			//	m_ShowAssetBrowser = !m_ShowAssetBrowser;
			//}

			ImGui::Separator();

			if (MenuItemMnemonic("Reset Layout", 0)) {
				m_RebuildLayout = true;
			}
			ImGui::EndMenu();
		}

		if (BeginMenuMnemonic("Settings", 0)) {
			if (MenuItemMnemonic(s_WindowSettingsWindow, 0)) {
				m_ShowWindowSettings = true;
			}
			ImGui::EndMenu();
		}

		if (BeginMenuMnemonic("Help", 0)) {
			ImGui::TextDisabled("Nothing here yet");
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	void EditorLayer::DrawViewport() {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		const bool visible = ImGui::Begin(s_ViewportPanel, nullptr, ImGuiWindowFlags_NoCollapse);
		ImGui::PopStyleVar();

		m_ViewportFocused = ImGui::IsWindowFocused();

		if (visible) {
			const ImVec2 region = ImGui::GetContentRegionAvail();

			auto fbs = ImGui::GetIO().DisplayFramebufferScale;

			const vk::Extent2D panelExtent{
				static_cast<uint32_t>(std::lround(std::max(region.x * fbs.x, 1.0f))),
				static_cast<uint32_t>(std::lround(std::max(region.y * fbs.y, 1.0f)))
			};

			if (panelExtent.width == m_PanelExtent.width && panelExtent.height == m_PanelExtent.height) {
				//if (m_PanelStableFrames < s_ResizeSettleFrames) {
				//	m_PanelStableFrames++;
				//}
			}
			else {
				m_PanelExtent = panelExtent;
				m_PanelStableFrames = 0;
			}

			const auto renderSync = m_RenderSync.lock();
			const std::optional<ImTextureID> prt = renderSync ? renderSync->GetMainPRT() : std::nullopt;

			if (prt) {
				const ImVec2 imageOrigin = ImGui::GetCursorScreenPos();

				ImGui::Image(prt.value(), region);

				bool dragInFlight = false;

				const ImGuiPayload* payload = ImGui::GetDragDropPayload();
				dragInFlight = !(!payload || !payload->IsDataType(AssetDragDropPayload::s_PayloadType) || payload->DataSize != static_cast<int32_t>(sizeof(AssetDragDropPayload)));


				bool imageHovered = ImGui::IsItemHovered(dragInFlight ? ImGuiHoveredFlags_AllowWhenBlockedByActiveItem : ImGuiHoveredFlags_None);
				bool imageClicked = !dragInFlight && ImGui::IsItemClicked(ImGuiMouseButton_Left);

				HandleViewportAssetDrop(imageOrigin, region);

				DrawObjectGizmo(imageOrigin, region);
				DrawViewGizmo(imageOrigin, region);

				UpdateViewportPicking(imageOrigin, region, imageHovered, imageClicked, dragInFlight);

				DrawViewportToolbar(imageOrigin);
				DrawSelectionOverlay(imageOrigin);
			}
			else {
				ImGui::TextUnformatted("Waiting for the scene render target...");
			}
		}

		ImGui::End();
	}

	void EditorLayer::UpdateViewportPicking(const ImVec2 imageOrigin, const ImVec2 region, const bool imageHovered, const bool imageClicked, const bool dragInFlight) {
		auto renderSync = m_RenderSync.lock();

		bool gizmoOwnsMouse = ImGuizmo::IsUsingAny() || ImGuizmo::IsOver() || ViewGizmo::IsUsing() || ViewGizmo::IsOver();

		if (!renderSync || m_CameraCaptureActive || !imageHovered || (gizmoOwnsMouse && !dragInFlight)) {
			m_HoveredEntity = entt::null;
			m_HoverAcceptFromTicket = s_NoPickTicket;
			m_LastHoverPickPos = ImVec2(-1.0f, -1.0f);
			return;
		}

		ImVec2 mouse = ImGui::GetIO().MousePos;

		float u = (mouse.x - imageOrigin.x) / std::max(region.x, 1.0f);
		float v = (mouse.y - imageOrigin.y) / std::max(region.y, 1.0f);

		if (imageClicked) {
			m_ClickPickTicket = renderSync->RequestPick(u, v);
		}

		else if (m_ClickPickTicket == s_NoPickTicket && HasHoverPickInputChanged(mouse)) {
			m_LastHoverPickPos = mouse;
			m_LastHoverPickCameraPosition = m_CameraPosition;
			m_LastHoverPickCameraYaw = m_CameraYaw;
			m_LastHoverPickCameraPitch = m_CameraPitch;

			uint64_t ticket = renderSync->RequestPick(u, v);

			if (m_HoverAcceptFromTicket == s_NoPickTicket) {
				m_HoverAcceptFromTicket = ticket;
			}
		}
	}

	void EditorLayer::HandleViewportAssetDrop(const ImVec2 imageOrigin, const ImVec2 region) {
		m_DropHoverActive = false;

		if (!ImGui::BeginDragDropTarget()) {
			return;
		}

		constexpr ImGuiDragDropFlags flags = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;

		bool delivered = false;
		AssetDragDropPayload* payload = nullptr;

		const ImGuiPayload* impayload = ImGui::AcceptDragDropPayload(AssetDragDropPayload::s_PayloadType, flags);

		if (!(!impayload || impayload->DataSize != static_cast<int32_t>(sizeof(AssetDragDropPayload)))) {
			delivered = impayload->IsDelivery();
			payload = static_cast<AssetDragDropPayload*>(impayload->Data);
		}

		if (payload) {
			bool droppable = payload->Is(Cori::Core::AssetTraits<Cori::Graphics::Material>::TypeHash) || payload->Is(Cori::Core::AssetTraits<Cori::Graphics::Mesh>::TypeHash);

			if (droppable) {
				m_DropHoverActive = true;

				if (delivered) {
					ImVec2 mouse = ImGui::GetIO().MousePos;

					float u = (mouse.x - imageOrigin.x) / std::max(region.x, 1.0f);
					float v = (mouse.y - imageOrigin.y) / std::max(region.y, 1.0f);
					auto renderSync = m_RenderSync.lock();
					if (renderSync) {
						m_PendingDropPayload = *payload;
						m_DropPickTicket = renderSync->RequestPick(u, v);
					}
				}
			}
		}

		ImGui::EndDragDropTarget();
	}

	void EditorLayer::ApplyAssetToEntity(const AssetDragDropPayload& payload, const entt::entity target) {
		if (!payload.IsSet() || target == entt::null || !m_MainScene.IsValid() || !m_MainScene.GetRegistry().valid(target)) {
			return;
		}

		Cori::World::Entity entity{ entt::handle{ m_MainScene.GetRegistry(), target } };

		if (!entity.HasComponents<Cori::World::Components::Entity::Rendering>()) {
			return;
		}

		auto& rendering = entity.GetComponents<Cori::World::Components::Entity::Rendering>();

		if (payload.Is(Cori::Core::AssetTraits<Cori::Graphics::Material>::TypeHash)) {
			rendering.ChangeMaterial(Cori::Core::AssetManager2::Load<Cori::Graphics::Material>(payload.id));
			return;
		}

		if (payload.Is(Cori::Core::AssetTraits<Cori::Graphics::Mesh>::TypeHash)) {
			rendering.ChangeMesh(Cori::Core::AssetManager2::Load<Cori::Graphics::Mesh>(payload.id));
		}
	}

	void EditorLayer::DrawObjectGizmo(const ImVec2 imageOrigin, const ImVec2 region) {
		ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
		ImGuizmo::SetRect(imageOrigin.x, imageOrigin.y, region.x, region.y);
		ImGuizmo::SetOrthographic(false);

		if (!m_GizmoEnabled || m_CameraCaptureActive || m_SelectedEntity == entt::null || !m_MainScene.IsValid() || !m_MainScene.GetRegistry().valid(m_SelectedEntity)) {
			return;
		}

		Cori::World::Entity entity{ entt::handle{ m_MainScene.GetRegistry(), m_SelectedEntity } };

		if (!entity.HasComponents<Cori::World::Components::Entity::Transform>()) {
			return;
		}

		auto& tc = entity.GetComponents<Cori::World::Components::Entity::Transform>();
		auto& rc = entity.GetComponents<Cori::World::Components::Entity::Rendering>();

		auto& camera = m_MainScene.GetActiveCamera();

		glm::mat4 view = camera.GetViewMatrix();

		if (m_GizmoAABBCorrection) {
			auto AABB = Cori::Graphics::VulkanMeshManager::GetAABB3D(rc.GetMesh().GetHandle());
			if (!AABB) {
				return;
			}
			view = glm::translate(view, { AABB.value().bxCenter, AABB.value().byCenter, AABB.value().bzCenter });
		}

		glm::mat4 projection = camera.GetProjectionMatrix();
		projection[1][1] *= -1.0f;

		glm::mat4 world = tc.m_WorldTransform;

		float snap = s_TranslateSnap;

		if (m_GizmoOperation == ImGuizmo::ROTATE) {
			snap = s_RotateSnap;
		}
		else if (m_GizmoOperation == ImGuizmo::SCALE) {
			snap = s_ScaleSnap;
		}

		glm::vec3 snapValues{ snap };

		bool snapping = m_GizmoSnap || ImGui::GetIO().KeyCtrl;

		if (!ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), m_GizmoOperation, ImGuizmo::MODE::WORLD,
			glm::value_ptr(world), nullptr, snapping ? glm::value_ptr(snapValues) : nullptr)) {
			return;
		}

		glm::mat4 parentWorld = tc.m_WorldTransform * glm::inverse(tc.GetLocalTransform());
		glm::mat4 local = glm::inverse(parentWorld) * world;

		glm::vec3 scale{ glm::length(glm::vec3(local[0])), glm::length(glm::vec3(local[1])), glm::length(glm::vec3(local[2]))
		};

		if (scale.x < 0.0001f || scale.y < 0.0001f || scale.z < 0.0001f) {
			return;
		}

		glm::mat3 rotationBasis{ glm::vec3(local[0]) / scale.x, glm::vec3(local[1]) / scale.y, glm::vec3(local[2]) / scale.z };

		glm::quat rotation = glm::normalize(glm::quat_cast(rotationBasis));
		float angleDegrees = glm::degrees(glm::angle(rotation));

		tc.SetLocalPosition(glm::vec3(local[3]));
		tc.SetLocalScale(scale);

		if (std::abs(angleDegrees) < 0.0001f) {
			tc.SetLocalRotation(0.0f, glm::vec3(0.0f, 0.0f, 1.0f));
		}
		else {
			tc.SetLocalRotation(angleDegrees, glm::axis(rotation));
		}

		m_Inspector.InvalidateRotationCache();
	}

	void EditorLayer::DrawViewGizmo(const ImVec2 imageOrigin, const ImVec2 region) {
		if (m_CameraCaptureActive || !m_MainScene.IsValid()) {
			m_OrbitPivot.reset();
			m_ViewSnapActive = false;
			return;
		}

		auto& camera = m_MainScene.GetActiveCamera();

		glm::vec3 forward = camera.GetForward();

		glm::vec3 orbitPivot = m_OrbitPivot.value_or(m_CameraPosition + forward * s_ViewGuizmoOrbitDistance);

		float radius = Cori::Utility::ScaleUIUnit(s_ViewGizmoRadius);
		float margin = Cori::Utility::ScaleUIUnit(s_ViewGizmoMargin);

		auto up = Cori::Graphics::CameraController::GetWorldUp();
		glm::vec2 center = { imageOrigin.x + region.x - margin - radius, imageOrigin.y + margin + radius };
		ViewGizmo::Result result = ViewGizmo::Rotate(ToGizmoBasis(m_CameraPosition), ToGizmoBasis(forward), ToGizmoBasis(up), ToGizmoBasis(orbitPivot), center.x, center.y);

		bool dragging = ViewGizmo::IsUsing() || m_OrbitPivot.has_value();

		if (ViewGizmo::IsUsing()) {
			if (!m_OrbitPivot) {
				m_OrbitPivot = orbitPivot;
			}
		}
		else {
			m_OrbitPivot.reset();
		}

		if (result.modified) {
			glm::vec3 newForward = glm::normalize(FromGizmoBasis(result.forward));
			glm::vec3 newPosition = FromGizmoBasis(result.position);

			float newYaw = std::abs(newForward.z) < 0.9999f ? glm::degrees(std::atan2(newForward.y, newForward.x)) : m_CameraYaw;

			float newPitch = std::clamp(glm::degrees(std::asin(std::clamp(newForward.z, -1.0f, 1.0f))), -89.0f, 89.0f);

			if (dragging) {
				m_ViewSnapActive = false;

				m_CameraPosition = newPosition;
				m_CameraYaw = newYaw;
				m_CameraPitch = newPitch;
			}
			else {
				m_ViewSnapActive = true;
				m_ViewSnapElapsed = 0.0f;
				m_ViewSnapPivot = orbitPivot;
				m_ViewSnapStartDistance = glm::length(m_CameraPosition - orbitPivot);
				m_ViewSnapTargetDistance = glm::length(newPosition - orbitPivot);
				m_ViewSnapStartYaw = m_CameraYaw;
				m_ViewSnapYawDelta = WrapDegrees(newYaw - m_CameraYaw);
				m_ViewSnapStartPitch = m_CameraPitch;
				m_ViewSnapTargetPitch = newPitch;
			}
		}

		bool cameraMoved = result.modified || m_ViewSnapActive;

		if (m_ViewSnapActive) {
			m_ViewSnapElapsed += ImGui::GetIO().DeltaTime;

			float t = std::min(m_ViewSnapElapsed / s_ViewGuizmoSnapDuration, 1.0f);
			float eased = 1.0f - (1.0f - t) * (1.0f - t);

			m_CameraYaw = m_ViewSnapStartYaw + m_ViewSnapYawDelta * eased;
			m_CameraPitch = glm::mix(m_ViewSnapStartPitch, m_ViewSnapTargetPitch, eased);

			camera.SetYawPitch(m_CameraYaw, m_CameraPitch);

			m_CameraPosition = m_ViewSnapPivot - camera.GetForward() * glm::mix(m_ViewSnapStartDistance, m_ViewSnapTargetDistance, eased);

			if (t >= 1.0f) {
				m_ViewSnapActive = false;
			}
		}

		if (!cameraMoved) {
			return;
		}

		camera.SetPosition3D(m_CameraPosition);
		camera.SetYawPitch(m_CameraYaw, m_CameraPitch);

		camera.RecalculateVP();
	}

	void EditorLayer::DrawViewportToolbar(const ImVec2 imageOrigin) {
		const float inset = Cori::Utility::ScaleUIUnit(s_SelectionOverlayInset);

		ImGui::SetCursorScreenPos(ImVec2(imageOrigin.x + inset, imageOrigin.y + inset));

		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, ImGui::GetStyle().FrameRounding);
		ImGui::BeginChild("##viewportToolbar", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

		auto operationButton = [this](const char* label, const ImGuizmo::OPERATION operation, const char* tooltip) {
			bool active = m_GizmoOperation == operation;

			if (active) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
			}

			if (ImGui::Button(label)) {
				m_GizmoOperation = operation;
			}

			if (active) {
				ImGui::PopStyleColor();
			}

			ImGui::SetItemTooltip("%s", tooltip);
			ImGui::SameLine();
		};

		operationButton("T", ImGuizmo::TRANSLATE, "Translate (W)");
		operationButton("R", ImGuizmo::ROTATE, "Rotate (E)");
		operationButton("S", ImGuizmo::SCALE, "Scale (R)");

		ImGui::Checkbox("AABB Correction", &m_GizmoAABBCorrection);

		ImGui::SetItemTooltip("Gizmo space (X)");
		ImGui::SameLine();

		ImGui::EndChild();
		ImGui::PopStyleVar();
	}

	Cori::World::Entity EditorLayer::CreatePlaceholderEntity() {
		auto entity = m_MainScene.CreateEntity(std::format("Entity_{:03}", ++m_CreatedEntityCount));

		entity.AddComponent<Cori::World::Components::Entity::Rendering>(
			Cori::Core::AssetRef<Cori::Graphics::Mesh>(Cori::Graphics::VulkanMeshManager::GetPlaceholder<Cori::Graphics::Mesh>()),
			Cori::Core::AssetManager2::Load<Cori::Graphics::Material>("assets://WhiteMaterial.json"),
			glm::vec4{ 0.0f, 0.0f, 1.0f, 1.0f });

		auto& transform = entity.GetComponents<Cori::World::Components::Entity::Transform>();
		transform.SetLocalPosition(m_CameraPosition + m_MainScene.GetActiveCamera().GetForward() * s_EntitySpawnDistance);

		m_SelectedEntity = entity.GetRawEntity();
		m_Inspector.InvalidateRotationCache();

		CORI_INFO_TAGGED({ Tags::Snowflake::Self, Tags::Snowflake::Viewport }, "Created entity '{}' {} metres in front of the camera.", entity.GetName(), s_EntitySpawnDistance);

		return entity;
	}

	void EditorLayer::DrawInspector() {
		m_Inspector.Draw(&m_ShowInspector, m_MainScene, m_SelectedEntity);
	}

	void EditorLayer::DrawSelectionOverlay(const ImVec2 imageOrigin) {
		if (m_SelectedEntity == entt::null || !m_MainScene.IsValid()) {
			return;
		}

		if (!m_MainScene.GetRegistry().valid(m_SelectedEntity)) {
			m_SelectedEntity = entt::null;
			return;
		}

		const Cori::World::Entity entity{ entt::handle{ m_MainScene.GetRegistry(), m_SelectedEntity } };
		const std::string_view name = entity.GetName();

		ImGui::SetCursorScreenPos(ImVec2(imageOrigin.x + Cori::Utility::ScaleUIUnit(s_SelectionOverlayInset), ImGui::GetCursorScreenPos().y));
		ImGui::Text("Selected: %.*s [%u]", static_cast<int32_t>(name.size()), name.data(), entt::to_integral(m_SelectedEntity));
	}

	bool EditorLayer::HasHoverPickInputChanged(const ImVec2 mouse) const {
		return mouse.x != m_LastHoverPickPos.x
			|| mouse.y != m_LastHoverPickPos.y
			|| m_CameraPosition != m_LastHoverPickCameraPosition
			|| m_CameraYaw != m_LastHoverPickCameraYaw
			|| m_CameraPitch != m_LastHoverPickCameraPitch;
	}

	entt::entity EditorLayer::ResolvePickedEntity(const Cori::Graphics::PickResult& result) {
		if (result.entityID == Cori::Graphics::s_NullEntityID || !m_MainScene.IsValid()) {
			return entt::null;
		}

		const auto picked = static_cast<entt::entity>(result.entityID);

		if (!m_MainScene.GetRegistry().valid(picked)) {
			return entt::null;
		}

		return picked;
	}

	void EditorLayer::PollPickResults() {
		const auto renderSync = m_RenderSync.lock();
		if (!renderSync) {
			return;
		}

		Cori::Graphics::PickResult result;
		while (renderSync->PollPickResult(result)) {
			if (result.ticket == m_ClickPickTicket) {
				m_ClickPickTicket = s_NoPickTicket;
				m_SelectedEntity = ResolvePickedEntity(result);
			}
			else if (result.ticket == m_DropPickTicket) {
				m_DropPickTicket = s_NoPickTicket;

				ApplyAssetToEntity(m_PendingDropPayload, ResolvePickedEntity(result));

				m_PendingDropPayload = {};
			}
			else if (result.ticket >= m_HoverAcceptFromTicket) {
				m_HoveredEntity = ResolvePickedEntity(result);
			}
		}

		if (m_SelectedEntity != entt::null && m_MainScene.IsValid() && !m_MainScene.GetRegistry().valid(m_SelectedEntity)) {
			m_SelectedEntity = entt::null;
		}

		renderSync->ClearHighlights();

		if (m_DropHoverActive && m_HoveredEntity != entt::null) {
			renderSync->AddHighlight(m_HoveredEntity, s_DropTargetOutlineColor);
		}
		else if (m_HoveredEntity != m_SelectedEntity) {
			renderSync->AddHighlight(m_HoveredEntity, s_HoverOutlineColor);
		}

		if (!m_DropHoverActive || m_SelectedEntity != m_HoveredEntity) {
			renderSync->AddHighlight(m_SelectedEntity, s_SelectionOutlineColor);
		}
	}

	void EditorLayer::DrawWindowSettings() {
		if (!m_ShowWindowSettings) {
			return;
		}

		ImGui::SetNextWindowSize({ Cori::Utility::ScaleUIUnit(s_WindowSettingsWidth), 0.0f }, ImGuiCond_FirstUseEver);

		if (ImGui::Begin(s_WindowSettingsWindow, &m_ShowWindowSettings, ImGuiWindowFlags_AlwaysAutoResize)) {
			Cori::ImGuiPresets::ScreenModeAndResolutionDropdowns();
		}

		ImGui::End();
	}

	void EditorLayer::OnUpdate(Cori::Core::GameTimer& gameTimer) {
		FlushViewportResize();
		PollPickResults();
		UpdateCameraCapture();
		UpdateCamera(static_cast<float>(gameTimer.GetDeltaTime()));

		m_MainScene.OnUpdate(gameTimer);
		m_Browser.OnUpdate(gameTimer);
	}

	void EditorLayer::OnTickUpdate([[maybe_unused]] Cori::Core::GameTimer& gameTimer) {
		m_MainScene.OnTickUpdate(gameTimer);
		m_Browser.OnTickUpdate(gameTimer);
	}

	void EditorLayer::FlushViewportResize() {
		//if (m_PanelStableFrames < s_ResizeSettleFrames) {
		//	return;
		//}

		if (m_PanelExtent.width == 0 || m_PanelExtent.height == 0) {
			return;
		}

		if (m_PanelExtent.width == m_PRTExtent.width && m_PanelExtent.height == m_PRTExtent.height) {
			return;
		}

		m_PRTExtent = m_PanelExtent;

		if (const auto renderSync = m_RenderSync.lock()) {
			renderSync->RequestResize(m_PRTExtent);
		}

		if (m_MainScene.IsValid()) {
			m_MainScene.GetActiveCamera().SetAspectRatio(static_cast<float>(m_PRTExtent.width) / static_cast<float>(m_PRTExtent.height));
		}
	}

	void EditorLayer::UpdateCameraCapture() {
		const bool middleDown = Cori::Core::Input::IsMouseKeyDown(Cori::Core::CORI_MOUSEBUTTON_MIDDLE);

		if (middleDown && !m_CameraCaptureActive) {
			if (!m_ViewportFocused) {
				return;
			}

			if (!Cori::Core::Input::SetRelativeMouseMode(true)) {
				return;
			}

			m_CameraCaptureActive = true;

			ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange;
			return;
		}

		if ((!middleDown || !m_ViewportFocused) && m_CameraCaptureActive) {
			Cori::Core::Input::SetRelativeMouseMode(false);
			m_CameraCaptureActive = false;
			ImGui::GetIO().ConfigFlags &= ~(ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange);
		}
	}

	void EditorLayer::UpdateCamera(const float deltaTime) {
		if (m_MainScene.IsValid()) {
			auto& camera = m_MainScene.GetActiveCamera();


			if (m_CameraCaptureActive) {
				const glm::vec2 delta = Cori::Core::Input::GetMouseDelta() * 0.25f;

				m_CameraYaw -= delta.x * s_MouseSensitivity;
				m_CameraPitch -= delta.y * s_MouseSensitivity;
				m_CameraPitch = std::clamp(m_CameraPitch, -89.0f, 89.0f);

				camera.SetYawPitch(m_CameraYaw, m_CameraPitch);

				glm::vec3 direction{ 0.0f };

				if (Cori::Core::Input::IsKeyDown(Cori::Core::CORI_KEY_W)) {
					direction += camera.GetForward();
				}

				if (Cori::Core::Input::IsKeyDown(Cori::Core::CORI_KEY_S)) {
					direction -= camera.GetForward();
				}

				if (Cori::Core::Input::IsKeyDown(Cori::Core::CORI_KEY_D)) {
					direction += camera.GetRight();
				}

				if (Cori::Core::Input::IsKeyDown(Cori::Core::CORI_KEY_A)) {
					direction -= camera.GetRight();
				}

				if (Cori::Core::Input::IsKeyDown(Cori::Core::CORI_KEY_SPACE)) {
					direction += Cori::Graphics::CameraController::GetWorldUp();
				}

				if (Cori::Core::Input::IsKeyDown(Cori::Core::CORI_KEY_C)) {
					direction -= Cori::Graphics::CameraController::GetWorldUp();
				}

				if (glm::length(direction) > 0.0001f) {
					const float speed = Cori::Core::Input::IsKeyDown(Cori::Core::CORI_KEY_LSHIFT) ? s_MoveSpeed * s_SprintMultiplier : s_MoveSpeed;
					m_CameraPosition += glm::normalize(direction) * speed * deltaTime;
				}

				camera.SetPosition3D(m_CameraPosition);
			}

			camera.RecalculateVP();
		}
	}
}

#include "ComponentInspector.hpp"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

namespace {
	constexpr float s_LabelColumnEms{ 7.0f };

	bool BeginFieldTable(const char* id) {
		if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX)) {
			return false;
		}

		ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * s_LabelColumnEms);
		ImGui::TableSetupColumn("##widget", ImGuiTableColumnFlags_WidthStretch);

		return true;
	}

	void FieldRow(const char* label) {
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);

		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);

		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-FLT_MIN);
	}

	std::optional<Snowflake::AssetDragDropPayload> AssetSlotRow(const char* label, const Cori::Core::AssetID current, const uint64_t acceptedTypeHash) {
		ImGui::PushID(label);
		std::optional<Snowflake::AssetDragDropPayload> result = std::nullopt;

		FieldRow(label);

		std::string caption = current != 0 ? Cori::Core::AssetManager2::GetAssetDisplayName(current) : std::string{};

		if (caption.empty()) {
			caption = current != 0 ? "<unnamed>" : "Placeholder";
		}

		caption += "##slot";

		ImGui::Button(caption.c_str(), ImVec2(-FLT_MIN, 0.0f));

		Snowflake::AssetDragDropPayload* inFlight = nullptr;

		const ImGuiPayload* impayload = ImGui::GetDragDropPayload();

		if (!(!impayload || !impayload->IsDataType(Snowflake::AssetDragDropPayload::s_PayloadType) || impayload->DataSize != static_cast<int32_t>(sizeof(Snowflake::AssetDragDropPayload)))) {
			inFlight = static_cast<Snowflake::AssetDragDropPayload*>(impayload->Data);
		}

		if (inFlight && inFlight->Is(acceptedTypeHash) && ImGui::BeginDragDropTarget()) {
			const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(Snowflake::AssetDragDropPayload::s_PayloadType);
			if (payload && payload->DataSize == static_cast<int32_t>(sizeof(Snowflake::AssetDragDropPayload)) && payload->IsDelivery()) {
				result = *static_cast<Snowflake::AssetDragDropPayload*>(payload->Data);
			}

			ImGui::EndDragDropTarget();
		}

		ImGui::PopID();

		return result;
	}

	glm::vec3 EulerFromAngleAxis(const float angleDegrees, const glm::vec3 axis) {
		if (std::abs(angleDegrees) < 0.0001f || glm::length(axis) < 0.0001f) {
			return glm::vec3{ 0.0f };
		}

		return glm::degrees(glm::eulerAngles(glm::angleAxis(glm::radians(angleDegrees), glm::normalize(axis))));
	}

	std::pair<float, glm::vec3> AngleAxisFromEuler(const glm::vec3 eulerDegrees) {
		const glm::quat rotation = glm::normalize(glm::quat(glm::radians(eulerDegrees)));

		const float angleDegrees = glm::degrees(glm::angle(rotation));

		if (std::abs(angleDegrees) < 0.0001f) {
			return { 0.0f, glm::vec3{ 0.0f, 0.0f, 1.0f } };
		}

		return { angleDegrees, glm::axis(rotation) };
	}
}

namespace Snowflake {
	void ComponentInspector::InvalidateRotationCache() {
		m_RotationCacheValid = false;
	}

	void ComponentInspector::Draw(bool* open, Cori::World::SceneHandle scene, const entt::entity selected, const char* name) {
		if (open && !*open) {
			return;
		}

		if (!ImGui::Begin(name, open)) {
			ImGui::End();
			return;
		}

		if (!scene.IsValid() || selected == entt::null || !scene.GetRegistry().valid(selected)) {
			ImGui::TextDisabled("Nothing selected.");
			ImGui::End();
			return;
		}

		const Cori::World::Entity entity{ entt::handle{ scene.GetRegistry(), selected } };

		DrawEntityHeader(entity);
		DrawTransform(entity);
		DrawRendering(entity);

		ImGui::End();
	}

	void ComponentInspector::DrawEntityHeader(Cori::World::Entity entity) {
		const entt::entity raw = entity.GetRawEntity();

		if (m_NameBufferEntity != raw) {
			m_NameBufferEntity = raw;

			const std::string_view current = entity.GetName();
			const uint64_t length = std::min<uint64_t>(current.size(), sizeof(m_NameBuffer) - 1);

			std::memcpy(m_NameBuffer, current.data(), length);
			m_NameBuffer[length] = '\0';
		}

		ImGui::SetNextItemWidth(-FLT_MIN);

		if (ImGui::InputText("##name", m_NameBuffer, sizeof(m_NameBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
			entity.SetName(m_NameBuffer);
		}

		ImGui::TextDisabled("Entity %u", entt::to_integral(raw));
		ImGui::Separator();
	}

	void ComponentInspector::DrawTransform(Cori::World::Entity entity) {
		if (!entity.HasComponents<Cori::World::Components::Entity::Transform>()) {
			return;
		}

		if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
			return;
		}

		auto& transform = entity.GetComponents<Cori::World::Components::Entity::Transform>();

		const entt::entity raw = entity.GetRawEntity();

		if (!m_RotationCacheValid || m_CachedRotationEntity != raw) {
			const auto [angleDegrees, axis] = transform.GetLocalRotation();

			m_CachedRotation = EulerFromAngleAxis(angleDegrees, axis);
			m_CachedRotationEntity = raw;
			m_RotationCacheValid = true;
		}

		if (!BeginFieldTable("##transform")) {
			return;
		}

		glm::vec3 position = transform.GetLocalPosition();

		FieldRow("Position");
		if (ImGui::DragFloat3("##position", glm::value_ptr(position), 0.02f, 0.0f, 0.0f, "%.3f")) {
			transform.SetLocalPosition(position);
		}

		FieldRow("Rotation");
		if (ImGui::DragFloat3("##rotation", glm::value_ptr(m_CachedRotation), 0.5f, 0.0f, 0.0f, "%.1f")) {
			const auto [angleDegrees, axis] = AngleAxisFromEuler(m_CachedRotation);
			transform.SetLocalRotation(angleDegrees, axis);
		}

		glm::vec3 scale = transform.GetLocalScale();

		FieldRow("Scale");
		if (ImGui::DragFloat3("##scale", glm::value_ptr(scale), 0.02f, 0.0f, 0.0f, "%.3f")) {
			transform.SetLocalScale(scale);
		}

		int32_t depthOffset = transform.GetLocalDepthOffset();

		FieldRow("Depth offset");
		if (ImGui::DragInt("##depth", &depthOffset, 1.0f, INT16_MIN, INT16_MAX)) {
			transform.SetLocalDepth(static_cast<int16_t>(depthOffset));
		}

		const glm::vec3 worldPosition = glm::vec3(transform.m_WorldTransform[3]);

		FieldRow("World position");
		ImGui::Text("%.3f, %.3f, %.3f", worldPosition.x, worldPosition.y, worldPosition.z);

		ImGui::EndTable();
	}

	void ComponentInspector::DrawRendering(Cori::World::Entity entity) {
		if (!entity.HasComponents<Cori::World::Components::Entity::Rendering>()) {
			return;
		}

		if (!ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
			return;
		}

		auto& rendering = entity.GetComponents<Cori::World::Components::Entity::Rendering>();

		if (!BeginFieldTable("##rendering")) {
			return;
		}

		auto mesh = rendering.GetMesh();
		auto material = rendering.GetMaterial();

		if (auto dropped = AssetSlotRow("Mesh", mesh.IsInitialized() && mesh.GetAssetID().has_value() ? mesh.GetAssetID().value() : 0, Cori::Core::AssetTraits<Cori::Graphics::Mesh>::TypeHash)) {
			rendering.ChangeMesh(Cori::Core::AssetManager2::Load<Cori::Graphics::Mesh>(dropped->id));
		}

		if (auto dropped = AssetSlotRow("Material", material.IsInitialized() && material.GetAssetID().has_value() ? material.GetAssetID().value() : 0, Cori::Core::AssetTraits<Cori::Graphics::Material>::TypeHash)) {
			rendering.ChangeMaterial(Cori::Core::AssetManager2::Load<Cori::Graphics::Material>(dropped->id));
		}

		glm::vec4 uvOffsets = rendering.GetUVOffsets();

		FieldRow("UV offsets");
		if (ImGui::DragFloat4("##uvOffsets", glm::value_ptr(uvOffsets), 0.01f, 0.0f, 0.0f, "%.3f")) {
			rendering.ChangeUVOffsets(uvOffsets);
		}

		ImGui::EndTable();
	}
}

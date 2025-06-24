#pragma once

#include <box2cpp/box2cpp.h>
#include <box2d/math_functions.h>
#include <imgui.h>
#include <imgui_internal.h>

namespace Cori::Physics
{
	class DebugImguiRenderer
	{
	public:
		b2DebugDraw callbacks{};

		// Basic settings:
		b2Vec2 camera_pos{};
		float camera_scale = 32;
		b2Rot camera_rot = b2Rot{ .c = 1, .s = 0 };
		bool y_axis_is_upwards = true;
		bool enable_mouse_drag = false;
		ImGuiMouseButton mouse_drag_button = ImGuiMouseButton_Left;

		// Render settings:
		float line_thickness = 1;
		float shape_alpha = 1;
		float shape_fill_alpha_factor = 0.5f;
		float transform_axis_length = 16;
		ImVec4 transform_color_x = ImVec4(1, 0, 0, 1);
		ImVec4 transform_color_y = ImVec4(0, 1, 0, 1);
		ImVec4 text_bg_color = ImVec4(0, 0, 0, 0.5f);
		ImVec2 text_background_padding = ImVec2(2, 2);

		DebugImguiRenderer()
		{
			callbacks.context = this;

			callbacks.drawShapes = true;
			callbacks.drawJoints = true;
			callbacks.drawJointExtras = false;
			callbacks.drawBounds = false;
			callbacks.drawMass = true;
			callbacks.drawBodyNames = true;
			callbacks.drawContacts = false;
			callbacks.drawGraphColors = false;
			callbacks.drawContactNormals = false;
			callbacks.drawContactImpulses = false;
			callbacks.drawContactFeatures = false;
			callbacks.drawFrictionImpulses = false;
			callbacks.drawIslands = false;

			callbacks.DrawPolygonFcn = [](const b2Vec2* vertices, int vertexCount, b2HexColor color, void* context)
				{
					auto& self = *static_cast<DebugImguiRenderer*>(context);
					self.DrawPolygon(vertices, vertexCount, color, 0, nullptr);
				};

			callbacks.DrawSolidPolygonFcn = [](b2Transform xf, const b2Vec2* vertices, int vertexCount, float radius, b2HexColor color, void* context)
				{
					auto& self = *static_cast<DebugImguiRenderer*>(context);
					self.DrawPolygonFilled(vertices, vertexCount, color, radius, &xf);
				};

			callbacks.DrawCircleFcn = [](b2Vec2 center, float radius, b2HexColor color, void* context)
				{
					auto& self = *static_cast<DebugImguiRenderer*>(context);
					self.DrawList().AddCircle(self.Box2dToImguiPoint(center), self.Box2dToImguiLength(radius), self.ShapeColorToImguiColor(color, false), 0, self.line_thickness);
				};

			callbacks.DrawSolidCircleFcn = [](b2Transform xf, float radius, b2HexColor color, void* context)
				{
					auto& self = *static_cast<DebugImguiRenderer*>(context);
					self.DrawList().AddCircleFilled(self.Box2dToImguiPoint(xf.p), self.Box2dToImguiLength(radius), self.ShapeColorToImguiColor(color, true));
					self.DrawList().AddCircle(self.Box2dToImguiPoint(xf.p), self.Box2dToImguiLength(radius), self.ShapeColorToImguiColor(color, false), 0, self.line_thickness);

					self.DrawLine(xf.p, b2Vec2(xf.p.x + xf.q.c * radius, xf.p.y + xf.q.s * radius), color);
				};

			callbacks.DrawSolidCapsuleFcn = [](b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor color, void* context)
				{
					auto& self = *static_cast<DebugImguiRenderer*>(context); 
					self.DrawCapsuleFilled(p1, p2, radius, color);
				};

			callbacks.DrawSegmentFcn = [](b2Vec2 p1, b2Vec2 p2, b2HexColor color, void* context)
				{
					auto& self = *static_cast<DebugImguiRenderer*>(context);
					self.DrawLine(p1, p2, color);
				};

			callbacks.DrawTransformFcn = [](b2Transform xf, void* context)
				{
					auto& self = *static_cast<DebugImguiRenderer*>(context);
					ImVec2 p = self.Box2dToImguiPoint(xf.p);
					ImVec2 d = self.Box2dToImguiDir(b2Vec2(xf.q.c, xf.q.s));
					float dlen = std::sqrt(d.x * d.x + d.y * d.y);
					d.x = d.x / dlen * self.transform_axis_length;
					d.y = d.y / dlen * self.transform_axis_length;
					ImVec2 d2 = self.Box2dToImguiDir(b2Vec2(-xf.q.s, xf.q.c));
					d2.x = d2.x / dlen * self.transform_axis_length;
					d2.y = d2.y / dlen * self.transform_axis_length;
					self.DrawList().AddLine(ImVec2(p.x - 0.5f, p.y - 0.5f), ImVec2(p.x + d2.x - 0.5f, p.y + d2.y - 0.5f), ImGui::ColorConvertFloat4ToU32(self.transform_color_y), self.line_thickness);
					self.DrawList().AddLine(ImVec2(p.x - 0.5f, p.y - 0.5f), ImVec2(p.x + d.x - 0.5f, p.y + d.y - 0.5f), ImGui::ColorConvertFloat4ToU32(self.transform_color_x), self.line_thickness);
				};

			callbacks.DrawPointFcn = [](b2Vec2 p, float size, b2HexColor color, void* context)
				{
					auto& self = *static_cast<DebugImguiRenderer*>(context);
					self.DrawList().AddCircleFilled(self.Box2dToImguiPoint(p), size * self.line_thickness, self.ShapeColorToImguiColor(color, true));
				};

			callbacks.DrawStringFcn = [](b2Vec2 p, const char* s, b2HexColor color, void* context)
				{
					auto& self = *static_cast<DebugImguiRenderer*>(context);
					ImVec2 text_pos = self.Box2dToImguiPoint(p);
					text_pos.x = std::round(text_pos.x);
					text_pos.y = std::round(text_pos.y);
					if (*s == ' ') {
						float step = ImGui::CalcTextSize(" ").x;
						while (*s == ' ') { text_pos.x += step; s++; }
					}
					ImVec2 text_size = ImGui::CalcTextSize(s);
					self.DrawList().AddRectFilled(
						ImVec2(text_pos.x - self.text_background_padding.x, text_pos.y - self.text_background_padding.y),
						ImVec2(text_pos.x + self.text_background_padding.x + text_size.x, text_pos.y + self.text_background_padding.y + text_size.y),
						ImGui::ColorConvertFloat4ToU32(self.text_bg_color),
						(self.text_background_padding.x + self.text_background_padding.y) / 2
					);
					self.DrawList().AddText(text_pos, self.ShapeColorToImguiColor(color, false), s);
			};
		}

		DebugImguiRenderer(const DebugImguiRenderer& other) : callbacks(other.callbacks) { callbacks.context = this; }
		DebugImguiRenderer& operator=(const DebugImguiRenderer& other) { callbacks = other.callbacks; callbacks.context = this; return *this; }

		void ViewportCalc(const glm::vec2 cameraBounds, const int pixelsPerMeter) {

			const ImVec2 viewportSize = ImGui::GetMainViewport()->WorkSize;
			camera_scale = viewportSize.y / (cameraBounds.y / pixelsPerMeter);
			
			camera_pos.x = viewportSize.x / (camera_scale * 2);
			camera_pos.y = viewportSize.y / (camera_scale * 2);

		}

		void DrawShapes(WorldRef world)
		{
			callbacks.useDrawingBounds = false;
			struct Guard { DebugImguiRenderer& self; ~Guard() { self.callbacks.useDrawingBounds = false; } };
			Guard guard{ *this };

			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImVec2 p_min = viewport->WorkPos;
			ImVec2 p_max = { p_min.x + viewport->WorkSize.x, p_min.y + viewport->WorkSize.y };

			b2Vec2 world_corners[4];
			world_corners[0] = ImguiToBox2dPoint(p_min);
			world_corners[1] = ImguiToBox2dPoint(ImVec2(p_max.x, p_min.y));
			world_corners[2] = ImguiToBox2dPoint(p_max);
			world_corners[3] = ImguiToBox2dPoint(ImVec2(p_min.x, p_max.y));

			b2Vec2 lower = world_corners[0], upper = world_corners[0];
			for (int i = 1; i < 4; ++i) {
				lower = b2Min(lower, world_corners[i]);
				upper = b2Max(upper, world_corners[i]);
			}

			callbacks.drawingBounds = { lower, upper };
			world.Draw(callbacks);
		}

		void DrawModeToggles()
		{
			if (ImGui::Begin("Box2D Debug Toggles"))
			{
				ImGui::Checkbox("Shapes", &callbacks.drawShapes);
				ImGui::Checkbox("Joints", &callbacks.drawJoints);
				ImGui::Checkbox("Joint extras", &callbacks.drawJointExtras);
				ImGui::Checkbox("Bounds", &callbacks.drawBounds);
				ImGui::Checkbox("Mass", &callbacks.drawMass);
				ImGui::Checkbox("Body names", &callbacks.drawBodyNames);
				ImGui::Checkbox("Contacts", &callbacks.drawContacts);
				ImGui::Checkbox("Graph colors", &callbacks.drawGraphColors);
				ImGui::Checkbox("Contact normals", &callbacks.drawContactNormals);
				ImGui::Checkbox("Contact impulses", &callbacks.drawContactImpulses);
				ImGui::Checkbox("Contact features", &callbacks.drawContactFeatures);
				ImGui::Checkbox("Friction impulses", &callbacks.drawFrictionImpulses);
				ImGui::Checkbox("Islands", &callbacks.drawIslands);
				ImGui::Separator();
				ImGui::Checkbox("Mouse drag", &enable_mouse_drag);
				ImGui::SliderFloat("Scale", &camera_scale, 0.1f, 128.f);
				ImGui::SliderFloat("cam offset x", &camera_pos.x, -256.0f, 256.f);
				ImGui::SliderFloat("cam offset y", &camera_pos.y, -256.0f, 256.f);


			}
			ImGui::End();
		}

		void HandleMouseDrag(WorldRef world, const float mouseForce = 1000.0f)
		{
			if (!enable_mouse_drag) {
				if (mouse_joint) {
					mouse_joint = {};
					mouse_joint_body = {};
				}
				return;
			}

			b2Vec2 point = ImguiToBox2dPoint(ImGui::GetMousePos());

			//CORI_INFO("Pos: {}, {}", point.x, point.y);


			if (mouse_joint) {
				if (ImGui::IsMouseDown(mouse_drag_button)) {
					mouse_joint.SetTarget(point);
					mouse_joint.GetBodyB().SetAwake(true);
				}
				else {
					mouse_joint = {};
					mouse_joint_body = {};
				}
			}
			else {
				if (ImGui::IsMouseClicked(mouse_drag_button)) {
					ShapeRef target;
					world.Overlap(b2ShapeProxy{ .points = {point}, .count = 1, .radius = 0 }, b2DefaultQueryFilter(),
						[&](ShapeRef shape) {
							if (shape.GetBody().GetType() == b2_dynamicBody) {
								target = shape;
								return false;
							}
							return true;
						}
					);
					if (target) {
						mouse_joint_body = world.CreateBody(OwningHandle, Body::Params{});
						MouseJoint::Params joint_params{};
						joint_params.bodyIdA = mouse_joint_body;
						joint_params.bodyIdB = target.GetBody();
						joint_params.target = point;
						joint_params.maxForce = mouseForce * target.GetBody().GetMass();
						mouse_joint = world.CreateJoint(DestroyWithParent, joint_params);
						target.GetBody().SetAwake(true);
					}
				}
			}
		}

		// Coordinate conversions:
		ImVec2 Box2dToImguiDir(b2Vec2 dir) const
		{
			float sign = y_axis_is_upwards ? -1 : 1;
			ImVec2 ret(dir.x * camera_rot.c + dir.y * camera_rot.s, (-dir.x * camera_rot.s + dir.y * camera_rot.c) * sign);
			ret.x *= camera_scale;
			ret.y *= camera_scale;
			return ret;
		}

		ImVec2 Box2dToImguiPoint(b2Vec2 point) const
		{
			point.x -= camera_pos.x;
			point.y -= camera_pos.y;
			ImVec2 ret = Box2dToImguiDir(point);

			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ret.x += viewport->WorkPos.x + viewport->WorkSize.x * 0.5f;
			ret.y += viewport->WorkPos.y + viewport->WorkSize.y * 0.5f;
			return ret;
		}
		 
		float Box2dToImguiLength(float len) const {
			return len * camera_scale;
		}

		b2Vec2 ImguiToBox2dPoint(ImVec2 point) const
		{
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			point.x -= (viewport->WorkPos.x + viewport->WorkSize.x * 0.5f);
			point.y -= (viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);

			point.x /= camera_scale;
			point.y /= camera_scale;
			float sign = y_axis_is_upwards ? -1 : 1;
			b2Vec2 ret(point.x * camera_rot.c - point.y * camera_rot.s * sign, point.x * camera_rot.s + point.y * camera_rot.c * sign);
			ret.x += camera_pos.x;
			ret.y += camera_pos.y;
			return ret;
		}

		ImDrawList& DrawList() const {
			return *ImGui::GetBackgroundDrawList();
		}

		ImU32 ShapeColorToImguiColor(const b2HexColor& color, bool fill) const {
			return IM_COL32(color >> 16, (color >> 8) & 0xff, color & 0xff, std::clamp(0xff * shape_alpha * (fill ? shape_fill_alpha_factor : 1.0f), 0.0f, 255.0f));
		}

		ImU32 FmtColorToImguiColor(fmt::color color) const {
			uint32_t color_value = static_cast<uint32_t>(color);
			return IM_COL32((color_value >> 16) & 0xff, (color_value >> 8) & 0xff, color_value & 0xff, 0xff);

		}

		b2Vec2 GetPolygonVertex(const b2Vec2* data, int size, int i, const b2Transform* xf) const {
			b2Vec2 ret;
			if (y_axis_is_upwards) ret = data[size - 1 - i];
			else ret = data[i];
			if (xf) ret = b2TransformPoint(*xf, ret);
			return ret;
		}

		void DrawLine(b2Vec2 p1, b2Vec2 p2, b2HexColor color) const {
			ImVec2 a = Box2dToImguiPoint(p1); a.x -= 0.5f; a.y -= 0.5f;
			ImVec2 b = Box2dToImguiPoint(p2); b.x -= 0.5f; b.y -= 0.5f;
			DrawList().AddLine(a, b, ShapeColorToImguiColor(color, false), line_thickness);
		}

		void PushPolygonPoints(const b2Vec2* vertices, int vertexCount, float radius, const b2Transform* xf) const {
			if (radius <= 0) {
				for (int i = 0; i < vertexCount; i++) DrawList().PathLineTo(Box2dToImguiPoint(GetPolygonVertex(vertices, vertexCount, i, xf)));
			}
			else {
				ImVec2 prev_point; float prev_angle = 0;
				for (int i = 0; i < vertexCount + 2; i++) {
					ImVec2 point = Box2dToImguiPoint(GetPolygonVertex(vertices, vertexCount, i % vertexCount, xf));
					if (i != 0) {
						float angle = std::atan2(point.y - prev_point.y, point.x - prev_point.x);
						if (i != 1) {
							if (angle < prev_angle) angle += B2_PI * 2;
							DrawList().PathArcTo(prev_point, Box2dToImguiLength(radius), prev_angle - B2_PI / 2, angle - B2_PI / 2);
						}
						prev_angle = angle;
					}
					prev_point = point;
				}
			}
		}
		void DrawPolygon(const b2Vec2* vertices, int vertexCount, b2HexColor color, float radius, const b2Transform* xf) const {
			PushPolygonPoints(vertices, vertexCount, radius, xf);
			DrawList().PathStroke(ShapeColorToImguiColor(color, false), ImDrawFlags_Closed, line_thickness);
		}
		void DrawPolygonFilled(const b2Vec2* vertices, int vertexCount, b2HexColor color, float radius, const b2Transform* xf) const {
			PushPolygonPoints(vertices, vertexCount, radius, xf);
			DrawList().PathFillConvex(ShapeColorToImguiColor(color, true));
			DrawPolygon(vertices, vertexCount, color, radius, xf);
		}
		void PushCapsulePoints(b2Vec2 p1, b2Vec2 p2, float radius) const {
			b2Vec2 points[] = { p1, p2 };
			PushPolygonPoints(points, 2, radius, nullptr);
		}
		void DrawCapsule(b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor color) const {
			PushCapsulePoints(p1, p2, radius);
			DrawList().PathStroke(ShapeColorToImguiColor(color, false), ImDrawFlags_Closed, line_thickness);
			DrawLine(p1, p2, color);
		}
		void DrawCapsuleFilled(b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor color) const {
			PushCapsulePoints(p1, p2, radius);
			DrawList().PathFillConvex(ShapeColorToImguiColor(color, true));
			DrawCapsule(p1, p2, radius, color);
		}

		void DrawCircle(b2Vec2 center, float radius, b2HexColor color) {
			DrawList().AddCircle(Box2dToImguiPoint(center), Box2dToImguiLength(radius), ShapeColorToImguiColor(color, false), 0, line_thickness);
		}
		void DrawCircleFilled(b2Transform xf, float radius, b2HexColor color) {
			DrawList().AddCircleFilled(Box2dToImguiPoint(xf.p), Box2dToImguiLength(radius), ShapeColorToImguiColor(color, true));
			DrawList().AddCircle(Box2dToImguiPoint(xf.p), Box2dToImguiLength(radius), ShapeColorToImguiColor(color, false), 0, line_thickness);

			DrawLine(xf.p, b2Vec2(xf.p.x + xf.q.c * radius, xf.p.y + xf.q.s * radius), color);
		}
		void DrawPoint(b2Vec2 p, float size, b2HexColor color) {
			DrawList().AddCircleFilled(Box2dToImguiPoint(p), size * line_thickness, ShapeColorToImguiColor(color, true));
		}


		void DrawText(b2Vec2 p, fmt::color color, std::string text) {
			DrawList().AddText(Box2dToImguiPoint(p), FmtColorToImguiColor(color), text.c_str());
			
		}



		MouseJointRef mouse_joint;
		Body mouse_joint_body;
	};
}
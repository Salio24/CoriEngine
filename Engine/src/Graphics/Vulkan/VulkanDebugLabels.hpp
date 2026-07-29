#pragma once

namespace Cori {
	namespace Graphics {
		namespace DebugLabelColors {
			inline constexpr std::array Frame     { 0.188f, 0.282f, 0.470f, 1.0f };
			inline constexpr std::array Pass      { 0.282f, 0.470f, 0.690f, 1.0f };
			inline constexpr std::array Transfer  { 0.063f, 0.533f, 0.282f, 1.0f };
			inline constexpr std::array Barrier   { 0.596f, 0.376f, 0.094f, 1.0f };
			inline constexpr std::array Sync      { 0.345f, 0.345f, 0.345f, 1.0f };
			inline constexpr std::array Composite { 0.627f, 0.317f, 0.658f, 1.0f };
			inline constexpr std::array Scene     { 0.850f, 0.549f, 0.196f, 1.0f };
			inline constexpr std::array Upload    { 0.196f, 0.698f, 0.647f, 1.0f };
		}

		class VulkanDebugLabels {
		public:
			static void Enable() {
				s_Enabled = true;
			}

			static void Begin(const vk::CommandBuffer cmb, const char* name, const std::array<float, 4>& color) {
				if (!s_Enabled) {
					return;
				}

				const vk::DebugUtilsLabelEXT label{
					.pLabelName = name,
					.color = color
				};

				cmb.beginDebugUtilsLabelEXT(label);
			}

			static void End(const vk::CommandBuffer cmb) {
				if (!s_Enabled) {
					return;
				}

				cmb.endDebugUtilsLabelEXT();
			}

			static void Insert(const vk::CommandBuffer cmb, const char* name, const std::array<float, 4>& color) {
				if (!s_Enabled) {
					return;
				}

				const vk::DebugUtilsLabelEXT label{
					.pLabelName = name,
					.color = color
				};

				cmb.insertDebugUtilsLabelEXT(label);
			}

			static void BeginQueue(const vk::Queue queue, const char* name, const std::array<float, 4>& color) {
				if (!s_Enabled) {
					return;
				}

				const vk::DebugUtilsLabelEXT label{
					.pLabelName = name,
					.color = color
				};

				queue.beginDebugUtilsLabelEXT(label);
			}

			static void EndQueue(const vk::Queue queue) {
				if (!s_Enabled) {
					return;
				}

				queue.endDebugUtilsLabelEXT();
			}

			static void InsertQueue(const vk::Queue queue, const char* name, const std::array<float, 4>& color) {
				if (!s_Enabled) {
					return;
				}

				const vk::DebugUtilsLabelEXT label{
					.pLabelName = name,
					.color = color
				};

				queue.insertDebugUtilsLabelEXT(label);
			}

			template<typename... Args>
			[[nodiscard]] static const char* Format(const fmt::format_string<Args...> fmt, Args&&... args) {
				if (!s_Enabled) {
					return "";
				}

				thread_local std::array<char, 192> buffer;
				const auto result = fmt::format_to_n(buffer.data(), buffer.size() - 1, fmt, std::forward<Args>(args)...);
				*result.out = '\0';

				return buffer.data();
			}

		private:
			static inline bool s_Enabled{ false };
		};

		class VulkanDebugLabelScope {
		public:
			VulkanDebugLabelScope(const vk::CommandBuffer cmb, const char* name, const std::array<float, 4>& color) : m_CommandBuffer(cmb) {
				VulkanDebugLabels::Begin(m_CommandBuffer, name, color);
			}

			VulkanDebugLabelScope(const VulkanDebugLabelScope&) = delete;
			VulkanDebugLabelScope& operator=(const VulkanDebugLabelScope&) = delete;
			VulkanDebugLabelScope(VulkanDebugLabelScope&&) = delete;
			VulkanDebugLabelScope& operator=(VulkanDebugLabelScope&&) = delete;

			~VulkanDebugLabelScope() {
				VulkanDebugLabels::End(m_CommandBuffer);
			}

		private:
			vk::CommandBuffer m_CommandBuffer;
		};

		class VulkanQueueDebugLabelScope {
		public:
			VulkanQueueDebugLabelScope(const vk::Queue queue, const char* name, const std::array<float, 4>& color) : m_Queue(queue) {
				VulkanDebugLabels::BeginQueue(m_Queue, name, color);
			}

			VulkanQueueDebugLabelScope(const VulkanQueueDebugLabelScope&) = delete;
			VulkanQueueDebugLabelScope& operator=(const VulkanQueueDebugLabelScope&) = delete;
			VulkanQueueDebugLabelScope(VulkanQueueDebugLabelScope&&) = delete;
			VulkanQueueDebugLabelScope& operator=(VulkanQueueDebugLabelScope&&) = delete;

			~VulkanQueueDebugLabelScope() {
				VulkanDebugLabels::EndQueue(m_Queue);
			}

		private:
			vk::Queue m_Queue;
		};
	}
}

#ifdef DEBUG_BUILD

#define CORI_VK_LABEL_CONCAT_IMPL(a, b) a##b
#define CORI_VK_LABEL_CONCAT(a, b) CORI_VK_LABEL_CONCAT_IMPL(a, b)
#define CORI_VK_LABEL_VAR CORI_VK_LABEL_CONCAT(___cori_vk_label_, __LINE__)

#define CORI_VK_LABEL(cmb, name, color) const ::Cori::Graphics::VulkanDebugLabelScope CORI_VK_LABEL_VAR(cmb, name, color)
#define CORI_VK_LABEL_F(cmb, color, ...) const ::Cori::Graphics::VulkanDebugLabelScope CORI_VK_LABEL_VAR(cmb, ::Cori::Graphics::VulkanDebugLabels::Format(__VA_ARGS__), color)
#define CORI_VK_LABEL_BEGIN(cmb, name, color) ::Cori::Graphics::VulkanDebugLabels::Begin(cmb, name, color)
#define CORI_VK_LABEL_BEGIN_F(cmb, color, ...) ::Cori::Graphics::VulkanDebugLabels::Begin(cmb, ::Cori::Graphics::VulkanDebugLabels::Format(__VA_ARGS__), color)
#define CORI_VK_LABEL_END(cmb) ::Cori::Graphics::VulkanDebugLabels::End(cmb)
#define CORI_VK_LABEL_INSERT(cmb, name, color) ::Cori::Graphics::VulkanDebugLabels::Insert(cmb, name, color)
#define CORI_VK_LABEL_INSERT_F(cmb, color, ...) ::Cori::Graphics::VulkanDebugLabels::Insert(cmb, ::Cori::Graphics::VulkanDebugLabels::Format(__VA_ARGS__), color)

#define CORI_VK_QUEUE_LABEL(queue, name, color) const ::Cori::Graphics::VulkanQueueDebugLabelScope CORI_VK_LABEL_VAR(queue, name, color)
#define CORI_VK_QUEUE_LABEL_BEGIN(queue, name, color) ::Cori::Graphics::VulkanDebugLabels::BeginQueue(queue, name, color)
#define CORI_VK_QUEUE_LABEL_END(queue) ::Cori::Graphics::VulkanDebugLabels::EndQueue(queue)
#define CORI_VK_QUEUE_LABEL_INSERT(queue, name, color) ::Cori::Graphics::VulkanDebugLabels::InsertQueue(queue, name, color)
#define CORI_VK_QUEUE_LABEL_INSERT_F(queue, color, ...) ::Cori::Graphics::VulkanDebugLabels::InsertQueue(queue, ::Cori::Graphics::VulkanDebugLabels::Format(__VA_ARGS__), color)

#else

#define CORI_VK_LABEL(cmb, name, color)
#define CORI_VK_LABEL_F(cmb, color, ...)
#define CORI_VK_LABEL_BEGIN(cmb, name, color)
#define CORI_VK_LABEL_BEGIN_F(cmb, color, ...)
#define CORI_VK_LABEL_END(cmb)
#define CORI_VK_LABEL_INSERT(cmb, name, color)
#define CORI_VK_LABEL_INSERT_F(cmb, color, ...)

#define CORI_VK_QUEUE_LABEL(queue, name, color)
#define CORI_VK_QUEUE_LABEL_BEGIN(queue, name, color)
#define CORI_VK_QUEUE_LABEL_END(queue)
#define CORI_VK_QUEUE_LABEL_INSERT(queue, name, color)
#define CORI_VK_QUEUE_LABEL_INSERT_F(queue, color, ...)

#endif

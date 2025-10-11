#pragma once 
#include "EventSystem/Event.hpp"
#include "Window.hpp"
#include "Layer.hpp"
#include "LayerStack.hpp"
#include "ImGuiLayer.hpp"
#include "WorldSystem/SceneManager.hpp"
#include "Time.hpp"
#include "Core/Threading/MainThreadComandQueue.hpp"
#include "Core/Threading/ThreadPool.hpp"

/**
 * @brief Global engine namespace.
 */
namespace Cori {
	/**
	 * @brief Core systems of the engine are here.
	 */
	namespace Core {
		/**
		 * @brief Main Application object, there can only be one Application object. Basically a root of the program.
		 */
		class Application {
		public:
			explicit Application(const char* windowName);
			virtual ~Application();

			/**
			 * @brief Run loop, internal.
			 */
			void Run();

			/**
			 * @brief Pushes a Layer to the LayerStack.
			 * @param layer Rawptr to the created Layer.
			 * @details LayerStack manages the lifetime of the Layer. Duplicate Layer names are forbidden and will result in a CoriError.
			 * @note All operations with the LayerStack are processed at the end of the frame.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			[[nodiscard]] static std::expected<void, CoriError<>> PushLayer(Layer* layer);

			/**
			 * @brief Pushes an overlay Layer to the LayerStack.
			 * @param overlay Rawptr to the created overlay Layer.
			 * @details LayerStack manages the lifetime of the overlay Layer.
			 * Overlay Layers are always on the top of the LayerStack and always above regular Layers, thus are updated before regular Layers.
			 * Duplicate Layer names are forbidden and will result in a CoriError.
			 * @note All operations with the LayerStack are processed at the end of the frame.
			 * @return Expected object with void on success or CoriError<> on failure.
			 */
			[[nodiscard]] static std::expected<void, CoriError<>> PushOverlay(Layer* overlay);


			/**
			 * @brief Pops the Layer from the LayerStack.
			 * @details The Layer to be popped is the last that was pushed to the LayerStack. If there is nothing to pop, nothing will happen.
			 * @note All operations with the LayerStack are processed at the end of the frame.
			 */
			static void PopLayer();

			/**
			 * @brief Pops the overlay Layer from the LayerStack.
			 * @details The overlay Layer to be popped is the last that was pushed to the LayerStack. If there is nothing to pop, nothing will happen.
			 * @note All operations with the LayerStack are processed at the end of the frame.
			 */
			static void PopOverlay();

			/**
			 * @brief Setts the background color of the rendering canvas.
			 * @param color Normalized RGBA color.
			 */
			static void SetBackgroundColor(const glm::vec4& color);

			/**
			 * @brief Getter for the main Window.
			 * @return Non const reference to the main Window.
			 */
			static Window& GetWindow() { return *s_Instance->m_Window; }

			/**
			 * @brief Getter for the GameTimer.
			 * @return Non const reference to the GameTimer.
			 */
			static GameTimer& GetGameTimer() { return s_Instance->m_GameTimer; }

			/**
			 * @brief Emits the event and propagates it thought the LayerStack.
			 * @param event Event reference to emit.
			 */
			static void EmitEvent(Event& event);

			/**
			 * @brief Submits a task to be executed on the main thread.
			 * @details Submitted tasks will be processed in the very beginning of the next frame.
			 * \n It is safe to call this function from any thread.
			 * @tparam F Auto deduced callable type.
			 * @tparam Args Auto deduced callable argument types.
			 * @param f Task callable, no specific signature required.
			 * @param args Arguments that will be passed to the callable task upon execution.
			 * @return Future that will hold the result of invoke result of the passed callable.
			 */
			template <class F, class... Args>
			static std::future<std::invoke_result_t<F, Args...>> SubmitMainTask(F&& f, Args&&... args) {
				return s_Instance->m_CommandQueue.Submit(std::forward<F>(f), std::forward<Args>(args)...);
			}

			/**
			 * @brief Submits a task to be executed on the worker thread.
			 * @details It is safe to call this function from any thread.
			 * @tparam F Auto deduced callable type.
			 * @tparam Args Auto deduced callable argument types.
			 * @param f Task callable, no specific signature required.
			 * @param args Arguments that will be passed to the callable task upon execution.
			 * @return Future that will hold the result of invoke result of the passed callable.
			 */
			template <class F, class... Args>
			static std::future<std::invoke_result_t<F, Args...>> SubmitWorkerTask(F&& f, Args&&... args) {
				return s_Instance->m_WorkerPool.Submit(std::forward<F>(f), std::forward<Args>(args)...);
			}

			/**
			 * @brief Returns a number of available worker threads.
			 * @return Worker threads count.
			 */
			static uint16_t GetWorkerCount();

		private:
			void OnEvent(Event& event);

			void TickrateUpdate(GameTimer& gameTimer);

			bool OnWindowClose();

			bool m_RenderImGui{ true };

			std::unique_ptr<Window> m_Window;

			Internal::ImGuiLayer* m_ImGuiLayer;

			LayerStack m_LayerStack;

			Threading::MainThreadQueue m_CommandQueue;

			Threading::ThreadPool m_WorkerPool;

			GameTimer m_GameTimer;

			bool m_Running{ true };

			static Application* s_Instance;

			glm::vec4 m_BackgroundColor{ 0.5f, 0.5f, 0.0f, 1.0f };
		};

		Application* CreateApplication();
	}
}
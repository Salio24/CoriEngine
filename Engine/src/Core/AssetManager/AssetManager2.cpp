#include "AssetManager2.hpp"

#include "Core/Application.hpp"

namespace Cori {
	namespace Core {
		std::unique_ptr<AssetManager2> AssetManager2::s_Instance{ nullptr };

		void AssetManager2::Init() {
			new AssetManager2();
		}

		void AssetManager2::Shutdown() {
			s_Instance.reset();
		}

		AssetManager2& AssetManager2::Get() {
			CORI_CORE_ASSERT(s_Instance, "Calling AssetManager::Get but it was already destroyed or not yet created.");
			return *s_Instance;
		}

		void AssetManager2::OnUpdate(GameTimer& timer) {
			static float timer_ = 0.0;
			timer_ += timer.GetDeltaTime();

			if (timer_ > 1.0f) {
				timer_ = 0.0f;
				Application::SubmitWorkerTask([] { ScanAndReload(); });

				for (AssetDirID i = 0; i < Get().m_NextAssetDir; i++) {
					Application::SubmitWorkerTask([i] {
						ScanDirectory(i);
					});
				}
			}
		}
	}
}

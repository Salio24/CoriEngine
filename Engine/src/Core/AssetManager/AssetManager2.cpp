#include "AssetManager2.hpp"

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
	}
}
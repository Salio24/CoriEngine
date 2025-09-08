#include "AssetManager.hpp"

namespace Cori {
	AssetManager::Cache* AssetManager::s_Cache = nullptr;

	void AssetManager::Init() {
		s_Cache = new Cache();
	}

	void AssetManager::Shutdown() {
		delete s_Cache;
	}
}

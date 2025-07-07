#include "PrimitivePool.hpp"
#include "SceneSystem/Components.hpp"
#include "SceneSystem/Scene.hpp"

namespace Cori {
	namespace Graphics {

		PrimitivePool::PrimitivePool() {
			m_PrimitivePool.reserve(4096);
		}


		PrimitivePool::~PrimitivePool() {

		}
	}
}

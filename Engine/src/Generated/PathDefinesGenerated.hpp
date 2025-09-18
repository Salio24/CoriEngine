#pragma once

namespace Cori {
	namespace FileSystem {
		namespace Internal {
			struct PathDefines {
                static std::filesystem::path& GetEngineDataRoot() {
                    static std::filesystem::path EngineDataRoot = "/home/salio/CLionProjects/VoidScape/CoriEngine/Engine/enginedata";
                    return EngineDataRoot;
                }
			};
		}
	}
}

// Azahar for Frame: the environment switches that drive tests and diagnostics (AZAHARFRAME_TEST_*,
// AZAHARFRAME_HOMEBREW_AUTO/ROM/ROMDIR/AUTO_QUIT, AZAHARFRAME_DEBUG_STALL, AZAHARFRAME_XR_NO_GPU_WAIT)
// are read through these functions.  Without AZAHARFRAME_TEST_HOOKS (CMake option AZAHARFRAME_TEST_HOOKS,
// OFF in a release package) they never find anything, so a release cannot be steered by them.

#pragma once

#include <cstdlib>

namespace Common {

/// getenv() for a test or diagnostic switch; nullptr in a build without test hooks.
inline const char* TestHookEnv([[maybe_unused]] const char* name) {
#ifdef AZAHARFRAME_TEST_HOOKS
    return std::getenv(name);
#else
    return nullptr;
#endif
}

} // namespace Common

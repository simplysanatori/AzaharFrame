// Azahar for Frame: Qt flavours of Common::TestHookEnv (see common/test_hooks.h).

#pragma once

#include <QString>
#include "common/test_hooks.h"

inline QString FrameTestEnv(const char* name) {
    const char* v = Common::TestHookEnv(name);
    return v ? QString::fromLocal8Bit(v) : QString();
}

inline int FrameTestEnvInt(const char* name) {
    const char* v = Common::TestHookEnv(name);
    return v ? static_cast<int>(std::strtol(v, nullptr, 10)) : 0;
}

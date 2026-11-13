// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiu: frame_qt_tests builds the VR group (configure_frame_vr.cpp) and
// configuration_shared.cpp, whose "Use global configuration (%1)" item takes its translation
// context from ConfigurePerGame. The dialog itself is never built here: only its meta object is
// needed, and QDialog's stands in for it.

#include "citra_qt/configuration/configure_per_game.h"

const QMetaObject ConfigurePerGame::staticMetaObject = QDialog::staticMetaObject;

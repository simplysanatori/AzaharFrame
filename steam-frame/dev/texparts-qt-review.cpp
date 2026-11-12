// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame texparts: renders the Flat window's "Customize Texture Pack"
// dialog (src/citra_qt/frame_texture_parts.cpp, the real one) on a pack folder
// to a PNG, without a display, for design reviews. Not part of the build or a
// release.
//
//   g++ -std=c++20 -fPIC -DAZAHARFRAME_TEXPARTS_REVIEW -Isrc -Iexternals/json \
//       steam-frame/dev/texparts-qt-review.cpp
//       src/citra_qt/frame_texture_parts.cpp \ src/core/custom_tex/parts.cpp
//       $(pkg-config --cflags --libs Qt6Widgets) -o /tmp/texparts-qt
//   QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=2 /tmp/texparts-qt PACK_DIR
//   OUT.png
//
// The pack's own .azaharframe-parts.json decides what is on; the dialog shows
// the game as running
// ("Restart the Game to Apply").

#include <QApplication>
#include <QPainter>
#include <QPixmap>
#include <QStyleFactory>

#include "citra_qt/frame_texture_parts.h"

namespace {

/// A dark palette close to the Flat window's (Qt's own look, the system's dark
/// colours).
void DarkPalette(QApplication &app) {
  app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
  QPalette p;
  p.setColor(QPalette::Window, QColor(0x2a, 0x2e, 0x32));
  p.setColor(QPalette::WindowText, QColor(0xfc, 0xfc, 0xfc));
  p.setColor(QPalette::Base, QColor(0x20, 0x23, 0x26));
  p.setColor(QPalette::AlternateBase, QColor(0x27, 0x2b, 0x2f));
  p.setColor(QPalette::Text, QColor(0xfc, 0xfc, 0xfc));
  p.setColor(QPalette::Button, QColor(0x31, 0x36, 0x3b));
  p.setColor(QPalette::ButtonText, QColor(0xfc, 0xfc, 0xfc));
  p.setColor(QPalette::Highlight, QColor(0x1d, 0x54, 0x6c));
  p.setColor(QPalette::HighlightedText, QColor(0xfc, 0xfc, 0xfc));
  p.setColor(QPalette::Disabled, QPalette::ButtonText,
             QColor(0x6e, 0x74, 0x7a));
  p.setColor(QPalette::Disabled, QPalette::Text, QColor(0x6e, 0x74, 0x7a));
  p.setColor(QPalette::Disabled, QPalette::WindowText,
             QColor(0x6e, 0x74, 0x7a));
  app.setPalette(p);
  // Fusion's indicators vanish on a dark palette offscreen; outline them for
  // the picture.
  app.setStyleSheet(
      QStringLiteral("QRadioButton::indicator { width: 12px; height: 12px; "
                     "border: 2px solid #a1a9b1;"
                     " border-radius: 8px; }"
                     "QRadioButton::indicator:checked { border-color: #3daee9; "
                     "background: qradialgradient("
                     "cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5, stop:0 "
                     "#3daee9, stop:0.55 #3daee9,"
                     " stop:0.6 transparent); }"
                     "QTreeView::indicator { width: 12px; height: 12px; "
                     "border: 2px solid #a1a9b1;"
                     " border-radius: 3px; }"
                     "QTreeView::indicator:checked { border-color: #3daee9; "
                     "background: #3daee9; }"
                     "QTreeView::indicator:indeterminate { border-color: "
                     "#3daee9; background: qlineargradient("
                     "x1:0, y1:0, x2:0, y2:1, stop:0 transparent, stop:0.4 "
                     "transparent, stop:0.41 #3daee9,"
                     " stop:0.6 #3daee9, stop:0.61 transparent); }"));
}

} // namespace

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  if (argc < 3) {
    return 2;
  }
  DarkPalette(app);
  FrameTexturePartsDialog dlg(nullptr, argv[1], true);
  dlg.show();
  app.processEvents();
  // The dialog with a title bar drawn round it (offscreen windows have none).
  const QPixmap body = dlg.grab();
  const qreal dpr = body.devicePixelRatio();
  const int bar = 32;
  QPixmap framed(body.width(), body.height() + static_cast<int>(bar * dpr));
  framed.setDevicePixelRatio(dpr);
  framed.fill(QColor(0x31, 0x36, 0x3b));
  {
    QPainter pt(&framed);
    pt.setPen(QColor(0xfc, 0xfc, 0xfc));
    pt.drawText(QRect(0, 0, static_cast<int>(body.width() / dpr), bar),
                Qt::AlignCenter, dlg.windowTitle());
    pt.drawPixmap(0, bar, body);
  }
  return framed.save(QString::fromLocal8Bit(argv[2])) ? 0 : 1;
}

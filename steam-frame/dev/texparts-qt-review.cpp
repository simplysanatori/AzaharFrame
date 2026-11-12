// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame texparts: a sketch of the Flat window's "Customize Texture Pack" dialog (Qt widgets,
// fake data from DeathWrench's MM3DHD 0.0.42b) and the game list menu entry that opens it. Design
// review only, not part of the build or a release. Renders PNGs without a display:
//
//   g++ -std=c++20 -fPIC steam-frame/dev/texparts-qt-mock.cpp $(pkg-config --cflags --libs Qt6Widgets) -o /tmp/texparts-qt-mock
//   QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=2 /tmp/texparts-qt-mock THUMBS_DIR OUT_DIR
//
// THUMBS_DIR/<key>.png are the part previews (missing: no icon).

#include <QApplication>
#include <QButtonGroup>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QStyleFactory>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

QString thumbs_dir;

QIcon Thumb(const QString& key) {
    QPixmap src(thumbs_dir + "/" + key + ".png");
    if (src.isNull()) {
        return {};
    }
    const int side = std::min(src.width(), src.height());
    const QPixmap sq = src.copy((src.width() - side) / 2, (src.height() - side) / 2, side, side);
    return QIcon(sq.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}

/// A dark palette close to the Flat window's (Qt's own look, the system's dark colours).
void DarkPalette(QApplication& app) {
    app.setStyle(QStyleFactory::create("Fusion"));
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
    p.setColor(QPalette::PlaceholderText, QColor(0xa1, 0xa9, 0xb1));
    p.setColor(QPalette::Link, QColor(0x3d, 0xae, 0xe9));
    p.setColor(QPalette::ToolTipBase, QColor(0x31, 0x36, 0x3b));
    p.setColor(QPalette::ToolTipText, QColor(0xfc, 0xfc, 0xfc));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x6e, 0x74, 0x7a));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(0x6e, 0x74, 0x7a));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x6e, 0x74, 0x7a));
    app.setPalette(p);
    // Fusion's indicators vanish on a dark palette; outline them (a sketch, not the real theme).
    app.setStyleSheet(
        "QRadioButton::indicator { width: 12px; height: 12px; border: 2px solid #a1a9b1;"
        " border-radius: 8px; }"
        "QRadioButton::indicator:checked { border-color: #3daee9; background: qradialgradient("
        "cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5, stop:0 #3daee9, stop:0.55 #3daee9,"
        " stop:0.6 transparent); }"
        "QTreeView::indicator { width: 12px; height: 12px; border: 2px solid #a1a9b1;"
        " border-radius: 3px; }"
        "QTreeView::indicator:checked { border-color: #3daee9; background: #3daee9; }"
        "QTreeView::indicator:indeterminate { border-color: #3daee9; background: qlineargradient("
        "x1:0, y1:0, x2:0, y2:1, stop:0 transparent, stop:0.4 transparent, stop:0.41 #3daee9,"
        " stop:0.6 #3daee9, stop:0.61 transparent); }");
}

enum Col { kPart, kTextures, kSize, kOverlaps };

QTreeWidgetItem* Part(QTreeWidgetItem* parent, QTreeWidget* tree, const QString& name,
                      const QString& n, const QString& size, const QString& thumb, bool on,
                      const QString& overlaps = {}) {
    auto* it = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);
    it->setText(kPart, name);
    it->setText(kTextures, n);
    it->setText(kSize, size);
    it->setText(kOverlaps, overlaps);
    it->setTextAlignment(kTextures, Qt::AlignRight | Qt::AlignVCenter);
    it->setTextAlignment(kSize, Qt::AlignRight | Qt::AlignVCenter);
    it->setIcon(kPart, Thumb(thumb));
    it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
    it->setCheckState(kPart, on ? Qt::Checked : Qt::Unchecked);
    return it;
}

/// A group of variants: its name, then a radio button per variant (and Off).
void Variants(QTreeWidget* tree, const QString& group,
              const std::vector<std::tuple<QString, QString, QString, QString>>& rows, int chosen) {
    auto* head = new QTreeWidgetItem(tree);
    head->setText(kPart, group);
    QFont f = head->font(kPart);
    f.setBold(true);
    head->setFont(kPart, f);
    head->setFlags(Qt::ItemIsEnabled);
    auto* buttons = new QButtonGroup(tree);
    for (int i = -1; i < static_cast<int>(rows.size()); i++) {
        auto* it = new QTreeWidgetItem(head);
        auto* rb = new QRadioButton(i < 0 ? QStringLiteral("Off") : std::get<0>(rows[i]));
        if (i >= 0) {
            rb->setIcon(Thumb(std::get<3>(rows[i])));
            it->setText(kTextures, std::get<1>(rows[i]));
            it->setText(kSize, std::get<2>(rows[i]));
            it->setTextAlignment(kTextures, Qt::AlignRight | Qt::AlignVCenter);
            it->setTextAlignment(kSize, Qt::AlignRight | Qt::AlignVCenter);
        }
        rb->setIconSize({28, 28});
        rb->setChecked(i == chosen);
        rb->setAutoFillBackground(false);
        buttons->addButton(rb);
        tree->setItemWidget(it, kPart, rb);
    }
    head->setExpanded(true);
}

QDialog* MakeDialog() {
    auto* dlg = new QDialog;
    dlg->setWindowTitle("Customize Texture Pack");
    dlg->resize(760, 700);
    auto* v = new QVBoxLayout(dlg);

    auto* name = new QLabel("MM3D HD");
    QFont nf = name->font();
    nf.setPointSizeF(nf.pointSizeF() * 1.4);
    name->setFont(nf);
    v->addWidget(name);
    auto* by = new QLabel(
        "Created by <a href=\"https://github.com/DeathWrench/MM3DHD\">DeathWrench</a> · "
        "0.0.42b · 209 Textures On · 73 MB");
    v->addWidget(by);

    auto* row = new QHBoxLayout;
    auto* tree = new QTreeWidget;
    tree->setColumnCount(4);
    tree->setHeaderLabels({"Part", "Textures", "Size", "Overlaps"});
    tree->setIconSize({28, 28});
    tree->setAlternatingRowColors(true);
    tree->setRootIsDecorated(true);
    tree->setUniformRowHeights(true);
    Part(nullptr, tree, "Main", "80", "45 MB", "mm-main", true);
    Variants(tree, "Fonts",
             {{"Original N64", "1", "178 KB", "mm-font-n64"},
              {"Original Pizza", "1", "117 KB", "mm-font-pizza"},
              {"Original WW (resembles 3DS)", "1", "123 KB", "mm-font-ww"}},
             0);
    Variants(tree, "Link's Tunic",
             {{"N64 Inspired", "3", "770 KB", "mm-tunic-n64"},
              {"Nerrel Style", "3", "810 KB", "mm-tunic-nerrel"}},
             1);
    auto* link = Part(nullptr, tree, "Link: Nerrel Style", "10", "252 KB", "mm-link-nerrel", true,
                      "Other Link Textures");
    auto* ai = new QTreeWidgetItem(tree);
    ai->setText(kPart, "AI Upscaled or WIP");
    ai->setText(kTextures, "119");
    ai->setText(kSize, "31 MB");
    ai->setTextAlignment(kTextures, Qt::AlignRight | Qt::AlignVCenter);
    ai->setTextAlignment(kSize, Qt::AlignRight | Qt::AlignVCenter);
    ai->setFlags(ai->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
    ai->setCheckState(kPart, Qt::PartiallyChecked);
    Part(ai, tree, "Map", "37", "3.4 MB", "mm-ai-map", true);
    Part(ai, tree, "Other Link Textures", "78", "23 MB", "mm-ai-link", true, "Link: Nerrel Style");
    Part(ai, tree, "wip", "4", "4.4 MB", "mm-ai-wip", false);
    ai->setExpanded(true);
    tree->setCurrentItem(link);
    tree->header()->setStretchLastSection(false);
    tree->header()->setSectionResizeMode(kPart, QHeaderView::Stretch);
    for (int c : {kTextures, kSize, kOverlaps}) {
        tree->header()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    }
    row->addWidget(tree, 1);

    auto* side = new QVBoxLayout;
    for (const char* label : {"Move Up", "Move Down"}) {
        side->addWidget(new QPushButton(label));
    }
    side->addSpacing(16);
    for (const char* label : {"All On", "All Off"}) {
        side->addWidget(new QPushButton(label));
    }
    side->addStretch(1);
    row->addLayout(side);
    v->addLayout(row, 1);

    v->addWidget(new QLabel("Where Parts Overlap, the Lower One Wins"));
    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    v->addWidget(box);
    return dlg;
}

/// The game list's menu with the new entry under the texture pack one.
QMenu* MakeMenu() {
    auto* m = new QMenu;
    m->addAction("Favorite");
    m->addMenu("Open");
    m->addAction("Dump RomFS");
    m->addMenu("Disk Shader Cache");
    m->addAction("Optimize Texture Pack for Frame...");
    auto* customize = m->addAction("Customize Texture Pack...");
    m->addMenu("Uninstall");
    m->addAction("Remove Play Time Data");
    m->addMenu("Create Shortcut");
    m->addSeparator();
    m->addAction("Properties");
    m->setActiveAction(customize);
    return m;
}

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    if (argc < 3) {
        return 2;
    }
    thumbs_dir = QString::fromLocal8Bit(argv[1]);
    const QString out = QString::fromLocal8Bit(argv[2]);
    DarkPalette(app);

    QDialog* dlg = MakeDialog();
    dlg->show();
    app.processEvents();
    // The dialog with a title bar drawn round it (offscreen windows have none).
    const QPixmap body = dlg->grab();
    const qreal dpr = body.devicePixelRatio();
    const int bar = 32;
    QPixmap framed(body.width(), body.height() + static_cast<int>(bar * dpr));
    framed.setDevicePixelRatio(dpr);
    framed.fill(QColor(0x31, 0x36, 0x3b));
    {
        QPainter pt(&framed);
        pt.setPen(QColor(0xfc, 0xfc, 0xfc));
        pt.drawText(QRect(0, 0, static_cast<int>(body.width() / dpr), bar), Qt::AlignCenter,
                    dlg->windowTitle());
        pt.drawPixmap(0, bar, body);
    }
    bool ok = framed.save(out + "/d1-flat-dialog-mm3dhd.png");

    QMenu* menu = MakeMenu();
    menu->show();
    app.processEvents();
    ok &= menu->grab().save(out + "/d2-flat-game-list-menu.png");
    return ok ? 0 : 1;
}

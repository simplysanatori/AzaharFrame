// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame texparts: see frame_texture_parts.h.

#include "citra_qt/frame_texture_parts.h"

#include <algorithm>
#include <filesystem>
#include <QApplication>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#ifndef AZAHARFRAME_TEXPARTS_REVIEW // the review harness (steam-frame/dev) has no emulator
#include <memory>
#include <fmt/format.h>
#include "citra_qt/frame_texture_layers.h" // AzaharFrame texlayers
#include "common/file_util.h"
#include "core/custom_tex/layers.h"
#endif

namespace P = CustomTex::Parts;

namespace {

constexpr int kRoleGroup = Qt::UserRole;    ///< the group's index in the layout
constexpr int kRolePart = Qt::UserRole + 1; ///< the part's key ("" the group's row)

QString Q(const std::string& s) {
    return QString::fromStdString(s);
}

QIcon Preview(const std::string& path) {
    if (path.empty()) {
        return {};
    }
    const QPixmap src(Q(path));
    if (src.isNull()) {
        return {};
    }
    const int side = std::min(src.width(), src.height());
    return QIcon(src.copy((src.width() - side) / 2, (src.height() - side) / 2, side, side)
                     .scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}

/// pack.json's name, author and version (a pack.json with comments, which Azahar allows, shows
/// the folder's name).
struct PackInfo {
    std::string name, author, version;
};
PackInfo ReadPackInfo(const std::string& pack) {
    PackInfo info;
    QFile file(Q(pack + "/pack.json"));
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonObject j = QJsonDocument::fromJson(file.readAll()).object();
        info.name = j.value(QStringLiteral("name")).toString().toStdString();
        info.author = j.value(QStringLiteral("author")).toString().toStdString();
        info.version = j.value(QStringLiteral("version")).toString().toStdString();
    }
    return info;
}

void SetNumbers(QTreeWidgetItem* it, u64 textures, u64 bytes) {
    it->setText(1, QString::number(textures));
    it->setText(2, Q(P::SizeText(bytes)));
    it->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
    it->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
}

} // namespace

FrameTexturePartsDialog::FrameTexturePartsDialog(QWidget* parent, const std::string& pack_,
                                                 bool running)
    : QDialog(parent), pack(pack_) {
    setWindowTitle(QStringLiteral("Customize Texture Pack"));
    resize(760, 640);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    state = P::ReadState(pack);
    layout = P::Scan(pack, state.hints);
    QApplication::restoreOverrideCursor();

    auto* v = new QVBoxLayout(this);
    top = new QWidget; // AzaharFrame texlayers: the game's packs (AddTop)
    auto* top_layout = new QVBoxLayout(top);
    top_layout->setContentsMargins(0, 0, 0, 0);
    top->hide();
    v->addWidget(top);
    title = new QLabel;
    QFont tf = title->font();
    tf.setPointSizeF(tf.pointSizeF() * 1.4);
    title->setFont(tf);
    v->addWidget(title);
    header = new QLabel;
    v->addWidget(header);

    auto* row = new QHBoxLayout;
    tree = new QTreeWidget;
    tree->setColumnCount(4);
    tree->setHeaderLabels({QStringLiteral("Part"), QStringLiteral("Textures"),
                           QStringLiteral("Size"), QStringLiteral("Conflicts")});
    tree->setIconSize({28, 28});
    tree->setAlternatingRowColors(true);
    tree->setUniformRowHeights(true);
    tree->header()->setStretchLastSection(false);
    tree->header()->setSectionResizeMode(kPart, QHeaderView::Stretch);
    for (const int c : {kTextures, kSize, kConflicts}) {
        tree->header()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    }
    row->addWidget(tree, 1);
    auto* side = new QVBoxLayout;
    move_up = new QPushButton(QStringLiteral("Move Up"));
    move_down = new QPushButton(QStringLiteral("Move Down"));
    all_on = new QPushButton(QStringLiteral("All On"));
    all_off = new QPushButton(QStringLiteral("All Off"));
    side->addWidget(move_up);
    side->addWidget(move_down);
    side->addSpacing(16);
    side->addWidget(all_on);
    side->addWidget(all_off);
    side->addStretch(1);
    row->addLayout(side);
    v->addLayout(row, 1);

    note = new QLabel(QStringLiteral("Texture Conflicts: Parts Higher in the List Take Priority"));
    v->addWidget(note);
    if (running) {
        v->addWidget(new QLabel(QStringLiteral("Restart the Game to Apply")));
    }
    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    v->addWidget(box);

    connect(box, &QDialogButtonBox::accepted, this, [this] {
        if (Save()) {
            accept();
        } else {
            QMessageBox::warning(this, windowTitle(),
                                 QStringLiteral("The Choices Could Not Be Saved"));
        }
    });
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(tree, &QTreeWidget::itemChanged, this,
            [this](QTreeWidgetItem* it, int col) { ItemChanged(it, col); });
    connect(tree, &QTreeWidget::itemSelectionChanged, this, [this] { Selected(); });
    connect(move_up, &QPushButton::clicked, this, [this] { MoveSelected(-1); });
    connect(move_down, &QPushButton::clicked, this, [this] { MoveSelected(1); });
    connect(all_on, &QPushButton::clicked, this, [this] {
        P::SetAll(state, layout, true);
        Changed();
    });
    connect(all_off, &QPushButton::clicked, this, [this] {
        P::SetAll(state, layout, false);
        Changed();
    });
    ShowPack(pack, &state);
}

void FrameTexturePartsDialog::ShowPack(const std::string& pack_, const P::State* known) {
    if (pack_ != pack) {
        if (state.set) {
            other_states[pack] = state; // kept for Save
        }
        pack = pack_;
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const auto kept = other_states.find(pack);
        state = known ? *known : kept != other_states.end() ? kept->second : P::ReadState(pack);
        layout = P::Scan(pack, state.hints);
        QApplication::restoreOverrideCursor();
    } else if (known) {
        state = *known;
    }
    const PackInfo info = ReadPackInfo(pack);
    std::string name = info.name;
    if (name.empty()) {
        // the folder's name; for load/textures/<TID>, the pack it points to
        std::error_code ec;
        name = std::filesystem::weakly_canonical(pack, ec).filename().string();
    }
    title->setText(Q(name));
    header->setProperty("author", Q(info.author));
    header->setProperty("version", Q(info.version));
    selected_key.clear();
    if (const auto order = P::Order(layout, state); !order.empty()) {
        selected_key = layout.groups[order[0]].key; // the first row starts selected
    }
    Rebuild();
}

void FrameTexturePartsDialog::SetPackState(const std::string& pack_, const P::State& state_) {
    if (pack_ == pack) {
        state = state_;
        Rebuild();
    } else {
        other_states[pack_] = state_;
    }
}

void FrameTexturePartsDialog::AddTop(QWidget* widget) {
    top->layout()->addWidget(widget);
    top->show();
    resize(width(), 820);
}

void FrameTexturePartsDialog::Changed() {
    if (on_change) {
        on_change(pack, state);
    }
    Rebuild();
}

bool FrameTexturePartsDialog::HasParts() const {
    return layout.PartCount() >= 2;
}

bool FrameTexturePartsDialog::Save() {
    bool ok = true;
    for (const auto& [other, other_state] : other_states) { // AzaharFrame texlayers
        if (other != pack && other_state.set) {
            ok = P::WriteState(other, other_state) && ok;
        }
    }
    if (state.set) { // else nothing changed
        ok = P::WriteState(pack, state) && ok;
    }
    if (ok && on_save) {
        ok = on_save();
    }
    return ok;
}

void FrameTexturePartsDialog::Rebuild() {
    building = true;
    tree->clear();
    delete radio_groups; // the last list's
    radio_groups = new QObject(this);
    const auto [on_textures, on_bytes] = P::TotalsOn(layout, state);
    {
        const QString author = header->property("author").toString();
        QStringList rest;
        if (!header->property("version").toString().isEmpty()) {
            rest << header->property("version").toString();
        }
        rest << Q(P::CountText(on_textures, "Texture On", "Textures On"))
             << Q(P::SizeText(on_bytes));
        header->setText((author.isEmpty() ? QString()
                                          : QStringLiteral("Created by ") + author.toHtmlEscaped() +
                                                QStringLiteral(" · ")) +
                        rest.join(QStringLiteral(" · ")));
    }
    note->setVisible(P::AnyConflicts(layout));
    all_on->setEnabled(std::any_of(layout.groups.begin(), layout.groups.end(),
                                   [](const P::Group& g) { return g.kind != P::Kind::Choice; }));
    all_off->setEnabled(all_on->isEnabled());
    QTreeWidgetItem* select = nullptr;
    for (const std::size_t gi : P::Order(layout, state)) {
        const P::Group& g = layout.groups[gi];
        auto* it = new QTreeWidgetItem(tree);
        it->setData(kPart, kRoleGroup, static_cast<int>(gi));
        it->setText(kPart, Q(g.name));
        if (g.key == selected_key) {
            select = it;
        }
        switch (g.kind) {
        case P::Kind::Single: {
            const P::Part& p = g.parts[0];
            it->setData(kPart, kRolePart, Q(p.key));
            it->setIcon(kPart, Preview(p.preview));
            SetNumbers(it, p.textures, p.bytes);
            it->setText(kConflicts, Q(P::ConflictNames(layout, p)));
            it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
            it->setCheckState(kPart, P::IsOn(layout, state, g, p) ? Qt::Checked : Qt::Unchecked);
            break;
        }
        case P::Kind::Folder: {
            u64 n = 0, bytes = 0;
            QStringList conflicts;
            for (const P::Part& p : g.parts) {
                n += p.textures;
                bytes += p.bytes;
                auto* child = new QTreeWidgetItem(it);
                child->setData(kPart, kRoleGroup, static_cast<int>(gi));
                child->setData(kPart, kRolePart, Q(p.key));
                child->setText(kPart, Q(p.name));
                child->setIcon(kPart, Preview(p.preview));
                SetNumbers(child, p.textures, p.bytes);
                child->setText(kConflicts, Q(P::ConflictNames(layout, p)));
                child->setFlags(child->flags() | Qt::ItemIsUserCheckable);
                child->setCheckState(kPart,
                                     P::IsOn(layout, state, g, p) ? Qt::Checked : Qt::Unchecked);
            }
            SetNumbers(it, n, bytes);
            it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
            const int value = P::GroupValue(layout, state, g);
            it->setCheckState(kPart, value == 2   ? Qt::Checked
                                     : value == 1 ? Qt::PartiallyChecked
                                                  : Qt::Unchecked);
            it->setExpanded(true);
            break;
        }
        case P::Kind::Choice: {
            QFont f = it->font(kPart);
            f.setBold(true);
            it->setFont(kPart, f);
            auto* buttons = new QButtonGroup(radio_groups);
            const std::string chosen = P::Chosen(layout, state, g);
            for (int i = -1; i < static_cast<int>(g.parts.size()); i++) {
                auto* child = new QTreeWidgetItem(it);
                child->setData(kPart, kRoleGroup, static_cast<int>(gi));
                const P::Part* p = i < 0 ? nullptr : &g.parts[static_cast<std::size_t>(i)];
                auto* rb = new QRadioButton(p ? Q(p->name) : QStringLiteral("Off"));
                if (p) {
                    child->setData(kPart, kRolePart, Q(p->key));
                    rb->setIcon(Preview(p->preview));
                    SetNumbers(child, p->textures, p->bytes);
                    child->setText(kConflicts, Q(P::ConflictNames(layout, *p)));
                }
                rb->setIconSize({28, 28});
                rb->setChecked(p ? p->key == chosen : chosen.empty());
                buttons->addButton(rb);
                tree->setItemWidget(child, kPart, rb);
                const std::string key = p ? p->key : std::string{};
                connect(rb, &QRadioButton::toggled, this, [this, gi, key](bool on) {
                    if (!on || building) {
                        return;
                    }
                    P::Choose(state, layout, layout.groups[gi], key);
                    // after the click: the button's own signal is still running
                    QMetaObject::invokeMethod(this, [this] { Changed(); }, Qt::QueuedConnection);
                });
            }
            it->setExpanded(true);
            break;
        }
        }
    }
    if (select) {
        tree->setCurrentItem(select);
    }
    building = false;
    Selected();
}

void FrameTexturePartsDialog::ItemChanged(QTreeWidgetItem* it, int column) {
    if (building || column != kPart) {
        return;
    }
    const int gi = it->data(kPart, kRoleGroup).toInt();
    if (gi < 0 || gi >= static_cast<int>(layout.groups.size())) {
        return;
    }
    const P::Group& g = layout.groups[static_cast<std::size_t>(gi)];
    const bool on = it->checkState(kPart) != Qt::Unchecked;
    const std::string key = it->data(kPart, kRolePart).toString().toStdString();
    if (g.kind == P::Kind::Folder && key.empty()) {
        // the folder's checkbox: a click on a mixed one turns every part on
        P::SetGroup(state, layout, g, P::GroupValue(layout, state, g) != 2);
    } else if (!key.empty()) {
        P::SetPart(state, layout, key, on);
    }
    QMetaObject::invokeMethod(this, [this] { Changed(); }, Qt::QueuedConnection);
}

int FrameTexturePartsDialog::SelectedGroup() const {
    const QTreeWidgetItem* it = tree->currentItem();
    if (!it) {
        return -1;
    }
    const int gi = it->data(kPart, kRoleGroup).toInt();
    return gi >= 0 && gi < static_cast<int>(layout.groups.size()) ? gi : -1;
}

void FrameTexturePartsDialog::Selected() {
    const int gi = SelectedGroup();
    bool up = false, down = false;
    if (gi >= 0) {
        const P::Group& g = layout.groups[static_cast<std::size_t>(gi)];
        if (!building) {
            selected_key = g.key;
        }
        if (P::CanMove(layout, g)) {
            const auto order = P::Order(layout, state);
            const auto at = std::find(order.begin(), order.end(), static_cast<std::size_t>(gi));
            up = at != order.begin();
            down = at != order.end() && at + 1 != order.end();
        }
    }
    move_up->setEnabled(up);
    move_down->setEnabled(down);
}

void FrameTexturePartsDialog::MoveSelected(int delta) {
    const int gi = SelectedGroup();
    if (gi < 0) {
        return;
    }
    selected_key = layout.groups[static_cast<std::size_t>(gi)].key;
    P::Move(state, layout, selected_key, delta);
    Changed();
}

#ifndef AZAHARFRAME_TEXPARTS_REVIEW
void FrameTexturePartsDialog::Open(QWidget* parent, u64 title_id, bool running) {
    const QString title = QStringLiteral("Customize Texture Pack");
    // AzaharFrame texlayers: every pack of the game, on or off, and their order
    const CustomTex::Paths paths = CustomTex::DefaultPaths();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto game = std::make_shared<CustomTex::Layers::Game>(CustomTex::Layers::Game::Load(paths, title_id));
    QApplication::restoreOverrideCursor();
    if (game->packs.empty()) {
        QMessageBox::information(parent, title, QStringLiteral("No Texture Pack for This Game"));
        return;
    }
    const std::vector<std::string> on = CustomTex::Layers::Packs(game->list);
    const std::string first = on.empty() ? game->packs[0].id : on[0];
    FrameTexturePartsDialog dialog(parent, first, running);
    auto* packs = new FrameTextureLayers(&dialog, game);
    dialog.AddTop(packs);
    dialog.on_change = [packs](const std::string& pack, const P::State& state) {
        packs->PackChanged(pack, state);
    };
    dialog.on_save = [game, paths, title_id] {
        return game->Save(paths, title_id).ok;
    };
    dialog.exec();
}
#endif

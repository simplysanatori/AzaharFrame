// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame texlayers: see frame_texture_layers.h.

#include "citra_qt/frame_texture_layers.h"

#include <algorithm>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include "citra_qt/frame_texture_parts.h"

namespace L = CustomTex::Layers;

namespace {

constexpr int kRoleId = Qt::UserRole;

QString Q(const std::string& s) {
    return QString::fromStdString(s);
}

QLabel* Subheader(const QString& text) {
    auto* label = new QLabel(text);
    QFont f = label->font();
    f.setBold(true);
    label->setFont(f);
    return label;
}

/// Tall enough for `rows` rows (at most), no taller.
void FitRows(QListWidget* list, int rows) {
    const int row = std::max(list->sizeHintForRow(0), list->fontMetrics().height() + 6);
    list->setFixedHeight(row * std::clamp(rows, 1, 6) + 2 * list->frameWidth() + 2);
}

} // namespace

FrameTextureLayers::FrameTextureLayers(FrameTexturePartsDialog* dialog_,
                                       std::shared_ptr<L::Game> game_)
    : QWidget(dialog_), dialog(dialog_), game(std::move(game_)) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 8);
    v->addWidget(Subheader(QStringLiteral("Texture Packs")));
    packs = new QListWidget;
    v->addWidget(packs);

    order_box = new QWidget;
    auto* ov = new QVBoxLayout(order_box);
    ov->setContentsMargins(0, 8, 0, 0);
    ov->addWidget(Subheader(QStringLiteral("Enabled Textures")));
    auto* row = new QHBoxLayout;
    order = new QListWidget;
    row->addWidget(order, 1);
    auto* side = new QVBoxLayout;
    move_up = new QPushButton(QStringLiteral("Move Up"));
    move_down = new QPushButton(QStringLiteral("Move Down"));
    side->addWidget(move_up);
    side->addWidget(move_down);
    side->addStretch(1);
    row->addLayout(side);
    ov->addLayout(row);
    ov->addWidget(new QLabel(QStringLiteral("Texture Conflicts: Higher in the List Takes Priority")));
    v->addWidget(order_box);

    connect(packs, &QListWidget::itemChanged, this,
            [this](QListWidgetItem* it) { PackItemChanged(it); });
    connect(packs, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* it) {
        if (building || !it) {
            return;
        }
        const std::string id = it->data(kRoleId).toString().toStdString();
        const auto d = game->data.find(id);
        dialog->ShowPack(id, d != game->data.end() ? &d->second.state : nullptr);
    });
    connect(order, &QListWidget::itemChanged, this,
            [this](QListWidgetItem* it) { LayerItemChanged(it); });
    connect(order, &QListWidget::itemSelectionChanged, this, [this] { Selected(); });
    connect(move_up, &QPushButton::clicked, this, [this] { MoveSelected(-1); });
    connect(move_down, &QPushButton::clicked, this, [this] { MoveSelected(1); });
    Rebuild();
}

void FrameTextureLayers::Rebuild() {
    building = true;
    const std::string shown = dialog->ShownPack();
    packs->clear();
    for (const CustomTex::Pack& p : game->packs) {
        auto* it = new QListWidgetItem(Q(p.name), packs);
        it->setData(kRoleId, Q(p.id));
        it->setToolTip(Q(p.path));
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(game->IsOn(p.id) ? Qt::Checked : Qt::Unchecked);
        if (p.id == shown) {
            packs->setCurrentItem(it);
        }
    }
    FitRows(packs, packs->count());

    const int selected = order->currentRow();
    order->clear();
    const bool layered = L::Packs(game->list).size() >= 2;
    order_box->setVisible(layered);
    if (layered) {
        for (const L::Row& r : game->Rows()) {
            auto* it = new QListWidgetItem(Q(r.name + "  ·  " + r.detail), order);
            if (r.value >= 0) {
                it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
                it->setCheckState(r.value == 2   ? Qt::Checked
                                  : r.value == 1 ? Qt::PartiallyChecked
                                                 : Qt::Unchecked);
            }
        }
        FitRows(order, order->count());
        if (selected >= 0 && selected < order->count()) {
            order->setCurrentRow(selected);
        }
    }
    building = false;
    Selected();
}

void FrameTextureLayers::PackItemChanged(QListWidgetItem* it) {
    if (building) {
        return;
    }
    const std::string id = it->data(kRoleId).toString().toStdString();
    const bool on = it->checkState() == Qt::Checked;
    QMetaObject::invokeMethod(
        this,
        [this, id, on] {
            game->SetOn(id, on);
            Rebuild();
        },
        Qt::QueuedConnection);
}

void FrameTextureLayers::LayerItemChanged(QListWidgetItem* it) {
    if (building) {
        return;
    }
    const int row = order->row(it);
    QMetaObject::invokeMethod(
        this,
        [this, row] {
            if (row < 0 || row >= static_cast<int>(game->list.entries.size())) {
                return;
            }
            const std::string pack = game->list.entries[static_cast<std::size_t>(row)].pack;
            if (game->Toggle(static_cast<std::size_t>(row))) {
                PushState(pack);
            }
            Rebuild();
        },
        Qt::QueuedConnection);
}

void FrameTextureLayers::MoveSelected(int delta) {
    const int row = order->currentRow();
    if (row < 0 || !game->Move(static_cast<std::size_t>(row), delta)) {
        return;
    }
    PushState(game->list.entries[static_cast<std::size_t>(row + delta)].pack);
    building = true;
    order->setCurrentRow(row + delta);
    building = false;
    Rebuild();
}

void FrameTextureLayers::Selected() {
    const int row = order->currentRow();
    const int n = order->count();
    move_up->setEnabled(row > 0 && row < n);
    move_down->setEnabled(row >= 0 && row + 1 < n);
}

void FrameTextureLayers::PushState(const std::string& pack) {
    if (const auto d = game->data.find(pack); d != game->data.end()) {
        dialog->SetPackState(pack, d->second.state);
    }
}

void FrameTextureLayers::PackChanged(const std::string& pack,
                                     const CustomTex::Parts::State& state) {
    game->PackChanged(pack, state);
    game->dirty.insert(pack);
    Rebuild();
}

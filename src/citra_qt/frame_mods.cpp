// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "citra_qt/frame_mods.h"

#include <algorithm>
#include <optional>
#include <thread>
#include <utility>

#include <QApplication>
#include <QPointer>
#include <QTimer>
#include <fmt/format.h>

#include "common/logging/log.h"
#include "core/mods/library.h"
#include "frame_xr/frame_xr.h"

namespace Menu = FrameXr::Menu;

namespace {

/// `work` on a thread of its own, then `done` with its result on the GUI thread (if `self` is
/// still there).
template <typename Work, typename Done>
void OffThread(FrameMods* self, Work work, Done done) {
    QPointer<FrameMods> guard(self);
    std::thread([guard, work = std::move(work), done = std::move(done)] {
        auto result = work();
        QMetaObject::invokeMethod(
            qApp,
            [guard, done, result = std::move(result)] {
                if (guard) {
                    done(result);
                }
            },
            Qt::QueuedConnection);
    }).detach();
}

/// What an install is doing, as the row's supporting line says it.
std::string PhaseText(Mods::State::Phase phase) {
    switch (phase) {
    case Mods::State::Phase::Resolving:
        return "Connecting";
    case Mods::State::Phase::Downloading:
        return "Downloading";
    case Mods::State::Phase::Extracting:
        return "Unpacking";
    case Mods::State::Phase::Installing:
        return "Installing";
    default:
        return {};
    }
}

Menu::ModRow ToRow(const Mods::ModInfo& info) {
    Menu::ModRow r;
    r.id = info.id;
    r.name = info.name;
    r.verified = info.verified;
    r.managed = info.managed;
    r.active = info.state == Mods::ModState::Active;
    if (info.managed) {
        r.detail = Menu::JoinDetail({info.author, info.version});
    } else if (info.files.size() > 1) {
        r.detail = fmt::format("{} Files", info.files.size()); // your own: what is there
    }
    return r;
}

std::string PackDetail(const CustomTex::Pack& p) {
    return Menu::JoinDetail({p.author, p.version, CustomTex::FormatName(p.format),
                             p.size_bytes ? Menu::SizeText(*p.size_bytes) : std::string{}});
}

} // namespace

FrameMods::FrameMods(QObject* parent) : QObject(parent) {
    progress = new QTimer(this);
    connect(progress, &QTimer::timeout, this, [this] { PollProgress(); });
}

FrameMods::~FrameMods() = default;

void FrameMods::Open(u64 title_id_, int run_) {
    if (title_id_ != title_id || run_ != run) {
        // Another game, or the same one started again: what waited for a restart has applied.
        title_id = title_id_;
        run = run_;
        mods = {};
        textures = {};
        packs.clear();
        failed.clear();
        pending = {};
        PublishMods();
        PublishTextures();
    }
    if (title_id == 0) {
        return;
    }
    RefreshLoaded();
    RefreshPacks();
    if (mods.catalog == Menu::CatalogState::Ready) {
        RefreshAvailable();
    }
}

void FrameMods::Command(const FrameXr::MenuRequest& r) {
    using C = FrameXr::MenuCommand;
    if (title_id == 0) {
        return;
    }
    switch (r.command) {
    case C::SelectTexturePack:
        SelectPack(r.text);
        return;
    case C::LoadModCatalog:
        if (mods.catalog == Menu::CatalogState::Idle) {
            mods.catalog = Menu::CatalogState::Loading;
            PublishMods();
        }
        RefreshAvailable();
        return;
    case C::InstallMod:
        Install(r.text, false);
        return;
    case C::SetModActive:
        if (r.value != 0) {
            Install(r.text, false); // from the library: no download
        } else {
            Deactivate(r.text);
        }
        return;
    case C::RemoveMod: {
        const auto it = std::find_if(mods.loaded.begin(), mods.loaded.end(),
                                     [&r](const Menu::ModRow& m) { return m.id == r.text; });
        if (it == mods.loaded.end() || mods.busy) {
            return;
        }
        if (it->managed) {
            Remove(r.text); // downloaded again from its author with Install Mods
            return;
        }
        // Your own files: only you have them, so ask.
        const Mods::Library& lib = Mods::Default();
        std::string files;
        for (const Mods::ModInfo& info : lib.ListLoaded(title_id)) {
            if (info.id == r.text) {
                for (std::size_t i = 0; i < info.files.size() && i < 3; i++) {
                    files += (files.empty() ? "" : ", ") + info.files[i];
                }
                if (info.files.size() > 3) {
                    files += fmt::format(" and {} More", info.files.size() - 3);
                }
            }
        }
        pending = {Pending::Kind::Remove, r.text};
        mods.dialog = {Menu::ModsDialogKind::Remove, it->name, files};
        PublishMods();
        return;
    }
    case C::ModsDialog: {
        const Pending p = std::exchange(pending, {});
        const Menu::ModsDialogKind kind = mods.dialog.kind;
        mods.dialog = {};
        PublishMods();
        if (r.value == 0 || kind == Menu::ModsDialogKind::Blocked) {
            return;
        }
        if (p.kind == Pending::Kind::Install) {
            Install(p.id, true);
        } else if (p.kind == Pending::Kind::Remove) {
            Remove(p.id);
        }
        return;
    }
    default:
        return;
    }
}

void FrameMods::TextureSettingChanged() {
    textures.needs_restart = true;
    PublishTextures();
}

// ---- Mods ----

void FrameMods::RefreshLoaded() {
    const u64 tid = title_id;
    OffThread(
        this, [tid] { return Mods::Default().ListLoaded(tid); },
        [this, tid](const std::vector<Mods::ModInfo>& list) {
            if (tid != title_id) {
                return;
            }
            mods.loaded.clear();
            for (const Mods::ModInfo& info : list) {
                mods.loaded.push_back(ToRow(info));
            }
            PublishMods();
        });
}

void FrameMods::RefreshAvailable() {
    const u64 tid = title_id;
    OffThread(
        this,
        [tid] {
            Mods::Library& lib = Mods::Default();
            lib.RefreshCatalogOnce(); // one request a run; the built-in catalog without it
            return lib.ListAvailable(tid);
        },
        [this, tid](const std::vector<Mods::AvailableMod>& list) {
            if (tid != title_id) {
                return;
            }
            mods.available.clear();
            for (const Mods::AvailableMod& a : list) {
                Menu::ModOffer o;
                o.id = a.id;
                o.name = a.name;
                o.author = a.author;
                o.verified = a.verified;
                if (a.id == installing) {
                    o.state = Menu::OfferState::Installing;
                    o.note = PhaseText(Mods::Default().GetState().phase);
                } else if (a.active) {
                    o.state = Menu::OfferState::Installed;
                } else if (!a.installable) {
                    o.state = Menu::OfferState::Unavailable;
                    o.note = "Unsupported Archive";
                } else if (a.id == failed) {
                    o.note = "Install Failed";
                    o.error = true;
                }
                mods.available.push_back(std::move(o));
            }
            mods.catalog = Menu::CatalogState::Ready;
            PublishMods();
        });
}

Menu::ModOffer* FrameMods::Offer(const std::string& id) {
    for (Menu::ModOffer& o : mods.available) {
        if (o.id == id) {
            return &o;
        }
    }
    return nullptr;
}

void FrameMods::Install(const std::string& id, bool replace) {
    if (mods.busy || id.empty()) {
        return;
    }
    mods.busy = true;
    installing = id;
    failed.clear();
    if (Menu::ModOffer* o = Offer(id)) {
        o->state = Menu::OfferState::Installing;
        o->note = PhaseText(Mods::State::Phase::Resolving);
        o->error = false;
    }
    PublishMods();
    progress->start(250);
    const u64 tid = title_id;
    LOG_INFO(Frontend, "xr: mods: install {}{}", id, replace ? " (replace)" : "");
    OffThread(
        this, [tid, id, replace] { return Mods::Default().Install(tid, id, replace); },
        [this, tid, id](const Mods::InstallOutcome& out) {
            progress->stop();
            installing.clear();
            mods.busy = false;
            if (tid != title_id) {
                return;
            }
            std::string name = id;
            if (const Menu::ModOffer* o = Offer(id)) {
                name = o->name;
            } else {
                for (const Menu::ModRow& r : mods.loaded) {
                    if (r.id == id) {
                        name = r.name;
                    }
                }
            }
            switch (out.result) {
            case Mods::InstallResult::Ok:
                mods.needs_restart = true;
                break;
            case Mods::InstallResult::AlreadyInstalled:
                break;
            case Mods::InstallResult::Conflict:
                // Never replaced silently: ask, or say what has to go first.
                LOG_INFO(Frontend, "xr: mods: {} conflicts with {} ({})", id, out.conflict_id,
                         out.conflict_path);
                if (out.conflict_unmanaged) {
                    mods.dialog = {Menu::ModsDialogKind::Blocked, name, out.conflict_name};
                } else {
                    pending = {Pending::Kind::Install, id};
                    mods.dialog = {Menu::ModsDialogKind::Replace, name, out.conflict_name};
                }
                break;
            case Mods::InstallResult::NotAvailable:
            case Mods::InstallResult::Error:
                LOG_WARNING(Frontend, "xr: mods: install {} failed: {}", id, out.error);
                failed = id;
                break;
            }
            RefreshLoaded();
            RefreshAvailable();
            PublishMods();
        });
}

void FrameMods::PollProgress() {
    Menu::ModOffer* o = Offer(installing);
    if (!o) {
        return;
    }
    const std::string text = PhaseText(Mods::Default().GetState().phase);
    if (!text.empty() && text != o->note) {
        o->note = text;
        PublishMods();
    }
}

void FrameMods::Deactivate(const std::string& id) {
    if (mods.busy) {
        return;
    }
    mods.busy = true;
    PublishMods();
    const u64 tid = title_id;
    OffThread(
        this, [tid, id] { return Mods::Default().Deactivate(tid, id); },
        [this, tid, id](const Mods::RemoveOutcome& out) {
            mods.busy = false;
            if (tid != title_id) {
                return;
            }
            if (out.ok) {
                mods.needs_restart = true;
            } else {
                LOG_WARNING(Frontend, "xr: mods: turning off {} failed: {}", id, out.error);
            }
            RefreshLoaded();
            if (mods.catalog == Menu::CatalogState::Ready) {
                RefreshAvailable();
            }
            PublishMods();
        });
}

void FrameMods::Remove(const std::string& id) {
    if (mods.busy) {
        return;
    }
    mods.busy = true;
    PublishMods();
    const u64 tid = title_id;
    LOG_INFO(Frontend, "xr: mods: remove {}", id);
    OffThread(
        this, [tid, id] { return Mods::Default().Remove(tid, id); },
        [this, tid, id](const Mods::RemoveOutcome& out) {
            mods.busy = false;
            if (tid != title_id) {
                return;
            }
            if (out.ok) {
                mods.needs_restart = true;
            } else {
                LOG_WARNING(Frontend, "xr: mods: remove {} failed: {}", id, out.error);
            }
            RefreshLoaded();
            if (mods.catalog == Menu::CatalogState::Ready) {
                RefreshAvailable();
            }
            PublishMods();
        });
}

void FrameMods::PublishMods() {
    FrameXr::SetInGameMenuModsList(mods);
}

// ---- Textures ----

void FrameMods::RefreshPacks() {
    const u64 tid = title_id;
    OffThread(
        this, [tid] { return CustomTex::ListPacks(CustomTex::DefaultPaths(), tid); },
        [this, tid](const std::vector<CustomTex::Pack>& list) {
            if (tid != title_id) {
                return;
            }
            packs = list;
            textures.packs.clear();
            for (const CustomTex::Pack& p : packs) {
                textures.packs.push_back({p.id, p.name, PackDetail(p), p.active});
            }
            PublishTextures();
            ComputeSizes();
        });
}

void FrameMods::ComputeSizes() {
    std::vector<std::string> paths;
    for (const CustomTex::Pack& p : packs) {
        if (!p.size_bytes) {
            paths.push_back(p.path);
        }
    }
    if (paths.empty() || sizing) {
        return;
    }
    sizing = true;
    // Seconds for a pack of many GB: the list shows first, the sizes when they are known.
    OffThread(
        this,
        [paths] {
            for (const std::string& path : paths) {
                CustomTex::ComputeSize(path);
            }
            return true;
        },
        [this](bool) {
            sizing = false;
            for (std::size_t i = 0; i < packs.size() && i < textures.packs.size(); i++) {
                packs[i].size_bytes = CustomTex::CachedSize(packs[i].path);
                textures.packs[i].detail = PackDetail(packs[i]);
            }
            PublishTextures();
        });
}

void FrameMods::SelectPack(const std::string& id) {
    if (textures.busy) {
        return;
    }
    std::optional<CustomTex::Pack> pack;
    if (!id.empty()) {
        for (const CustomTex::Pack& p : packs) {
            if (p.id == id) {
                pack = p;
            }
        }
        if (!pack) {
            return;
        }
    }
    textures.busy = true;
    for (Menu::TexturePackItem& p : textures.packs) {
        p.active = p.id == id; // at once; the list is read again after
    }
    PublishTextures();
    const u64 tid = title_id;
    LOG_INFO(Frontend, "xr: texture packs: {}", pack ? pack->name : "Off");
    OffThread(
        this,
        [tid, pack] {
            const CustomTex::Paths paths = CustomTex::DefaultPaths();
            return pack ? CustomTex::Activate(paths, tid, *pack) : CustomTex::Deactivate(paths, tid);
        },
        [this, tid](const CustomTex::Result& out) {
            textures.busy = false;
            if (tid != title_id) {
                return;
            }
            if (out.ok) {
                textures.needs_restart = true;
            } else {
                LOG_WARNING(Frontend, "xr: texture packs: switch failed: {}", out.error);
                FrameXr::ShowToastText("Texture Pack Not Changed");
            }
            RefreshPacks();
        });
}

void FrameMods::PublishTextures() {
    FrameXr::SetInGameMenuTextures(textures);
}

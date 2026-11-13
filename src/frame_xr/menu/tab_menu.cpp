// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "frame_xr/menu/tab_menu.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "frame_xr/menu/ds_drawing.h"
#include "frame_xr/menu/md3_fonts.h"

namespace FrameXr::Menu {

namespace Sym = ::FrameXr::Menu::Icon;

namespace {

// Layout (dp). The rail on the left, the pane (the tab's content) to its right.
constexpr float kRailW = 104.0f;
constexpr float kRailItemH = 72.0f; // AzaharFrame ui3: 80 until the seventh tab (Homebrew)
constexpr float kPaneMargin = 12.0f;
constexpr float kPaneRadius = 24.0f;
constexpr float kPad = 24.0f;      ///< inside the pane
constexpr float kControlH = 48.0f; ///< segmented buttons, menu buttons, the rows' buttons
constexpr float kLabelGap = 24.0f; ///< between a row's label and its control
constexpr float kStatRowH = 44.0f;
constexpr int kSlots = 10;

struct TabInfo {
    const char* label;
    char32_t icon;
};
constexpr std::array<TabInfo, static_cast<int>(Tab::Count)> kTabs{{
    {"Main", Sym::kHome},
    {"Position", Sym::kOpenWith},
    {"Stats", Sym::kMonitoring},
    {"Controls", Sym::kStadiaController},
    {"Mods", Sym::kExtension}, // AzaharFrame cheatstab
    {"Homebrew", Sym::kDeployedCode}, // AzaharFrame ui3
    {"Settings", Sym::kSettings},
}};

// AzaharFrame menuux: Button, a full-width tonal button (Switch to Flat at the end of Settings).
// AzaharFrame autosave: Header, a group's subheader ("Saves") as tall as a row (the list scrolls
// in whole rows).
enum class RowKind { Switch, Segmented, Slider, MenuButton, Offset, Link, Button, Header };

struct Row {
    Id id;
    RowKind kind;
    Lock lock = Lock::None;
    bool on = false;   ///< Switch
    int selected = 0;  ///< Segmented, MenuButton
    float t = 0.0f;    ///< Slider
    int stops = 0;     ///< Slider
    std::string value; ///< Slider, Offset; a MenuButton's value when it is none of its options
    bool Enabled() const {
        return lock == Lock::None;
    }
};

std::string Format(const char* f, double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), f, v);
    return buf;
}

std::string SliderText(Id id, float value) {
    if (id == Id::GameFov) {
        return Format("%.0f\u00b0", value);
    }
    if (id == Id::Depth3d) {
        return Format("%.0f%%", value);
    }
    if (id == Id::ReticleSpeed) { // AzaharFrame reticle: tenths of 1x, "2.0x"
        return Format("%.1fx", value / 10.0f);
    }
    if (id == Id::AutosaveEvery) { // AzaharFrame autosave: the stop's minutes
        const int i = std::clamp(static_cast<int>(std::lround(value)), 0,
                                 static_cast<int>(kAutosaveEvery.size()) - 1);
        return std::to_string(kAutosaveEvery[static_cast<std::size_t>(i)]) + " Min";
    }
    return Format("%.0f", value);
}

std::vector<Row> RowsFor(Tab tab, const MenuModel& m,
                         SettingsSection section = SettingsSection::General) {
    const auto lock = [&m](Id id) { return m.locked[static_cast<std::size_t>(id)]; };
    const auto row = [&](Id id, RowKind kind, int selected = 0, bool on = false) {
        Row r{id, kind, lock(id)};
        r.selected = selected;
        r.on = on;
        return r;
    };
    std::vector<Row> rows;
    if (tab == Tab::Position) {
        rows.push_back(row(Id::HideBottomScreen, RowKind::Switch, 0, m.hide_bottom_screen));
        rows.push_back(row(Id::ScreenLayout, RowKind::Segmented, m.screen_layout));
        rows.push_back(row(Id::MoveScreens, RowKind::Segmented, m.move_screens)); // AzaharFrame menu
        rows.push_back(row(Id::ScreenShape, RowKind::MenuButton, m.screen_shape));
        rows.push_back(row(Id::ScreenCurve, RowKind::MenuButton, m.screen_curve));
        Row offset = row(Id::ImmersiveOffset, RowKind::Offset);
        offset.value = m.immersive_offset;
        offset.on = m.immersive_offset_set;
        rows.push_back(offset);
    } else if (tab == Tab::Settings) {
        // Wider View and on: the Full (6DoF) modes only.
        const auto full = [&](Row r) {
            if (r.lock == Lock::None && m.immersive_mode < 2) {
                r.lock = Lock::FullOnly;
            }
            return r;
        };
        // AzaharFrame autosave: in three sections (owner decision, round 2). General: the
        // settings that are the same in every game, and the list's end.
        if (section == SettingsSection::General) {
            rows.push_back(row(Id::Background, RowKind::Segmented, m.background));
            rows.push_back(row(Id::WindowScale, RowKind::Segmented, m.window_scale));
            rows.push_back(row(Id::FaceButtons, RowKind::Segmented, m.face_buttons));
            rows.push_back(row(Id::MotionControls, RowKind::Segmented, m.motion_controls));
            rows.push_back(row(Id::ShowControllers, RowKind::Switch, 0, m.show_controllers));
            rows.push_back(row(Id::FastForwardSpeed, RowKind::Segmented, m.fast_forward_speed));
            rows.push_back(row(Id::FastForwardAudio, RowKind::Segmented, m.fast_forward_audio)); // ffaudio
            // AzaharFrame autosave: the Saves group (the game's values in Per-Game); Every and
            // Keep greyed while Autosave is off.
            rows.push_back(row(Id::SavesHeader, RowKind::Header));
            rows.push_back(row(Id::Autosave, RowKind::Switch, 0, m.autosave));
            const auto saves = [&](Id id, float t, int stops, int value) {
                Row r = row(id, RowKind::Slider);
                if (r.lock == Lock::None && !m.autosave) {
                    r.lock = Lock::AutosaveOff;
                }
                r.t = t;
                r.stops = stops;
                r.value = SliderText(id, static_cast<float>(value));
                rows.push_back(r);
            };
            const int every = std::clamp(m.autosave_every, 0, 4);
            const int keep = std::clamp(m.autosave_keep, 1, 10);
            saves(Id::AutosaveEvery, static_cast<float>(every) / 4.0f, 4, every);
            saves(Id::AutosaveKeep, static_cast<float>(keep - 1) / 9.0f, 9, keep);
            rows.push_back(row(Id::MoreSettings, RowKind::Link));
            rows.push_back(row(Id::SwitchToFlat, RowKind::Button)); // AzaharFrame menuux
            rows.push_back(row(Id::HelpHeader, RowKind::Header)); // AzaharFrame crashreport
            rows.push_back(row(Id::ReportProblem, RowKind::Link));
            rows.push_back(row(Id::AboutApp, RowKind::Link));
            return rows;
        }
        if (section == SettingsSection::Graphics) {
            // AzaharFrame depth3d
            Row depth = row(Id::Depth3d, RowKind::Slider);
            const SliderRange dr = SliderFor(Id::Depth3d);
            depth.t = (static_cast<float>(m.depth_3d) - dr.min) / (dr.max - dr.min);
            depth.value = SliderText(Id::Depth3d, static_cast<float>(m.depth_3d));
            rows.push_back(depth);
            // AzaharFrame menuux: the renderer's settings people change most, for VR.
            Row res = row(Id::InternalResolution, RowKind::MenuButton, m.internal_resolution - 1);
            if (m.internal_resolution < 1 || m.internal_resolution > kMaxResolution) {
                res.value = m.internal_resolution < 1 ? "Auto"
                                                      : std::to_string(m.internal_resolution) + "x";
                res.selected = -1;
            }
            rows.push_back(res);
            // Internal Resolution is its ceiling and 1x its floor: at 1x it has nothing to do.
            Row dynamic = row(Id::DynamicResolution, RowKind::Switch, 0, m.dynamic_resolution);
            if (dynamic.lock == Lock::None && m.internal_resolution == 1) {
                dynamic.lock = Lock::AtLowest;
            }
            rows.push_back(dynamic);
            rows.push_back(row(Id::TextureFilter, RowKind::Segmented, m.texture_filter));
            rows.push_back(row(Id::AsyncShaders, RowKind::Switch, 0, m.async_shaders));
            rows.push_back(row(Id::AccurateImageCopies, RowKind::Segmented, // accxfer, xfergpu
                               m.accurate_image_copies));
            return rows;
        }
        rows.push_back(row(Id::ImmersiveMode, RowKind::MenuButton, m.immersive_mode));
        rows.push_back(full(row(Id::WiderView, RowKind::Segmented, m.wider_view)));
        Row fov = full(row(Id::GameFov, RowKind::Slider));
        const SliderRange fr = SliderFor(Id::GameFov);
        fov.t = (static_cast<float>(m.game_fov) - fr.min) / (fr.max - fr.min);
        fov.value = SliderText(Id::GameFov, static_cast<float>(m.game_fov));
        rows.push_back(fov);
        rows.push_back(full(row(Id::MatchCamera, RowKind::Switch, 0, m.match_camera))); // depth
        rows.push_back(full(row(Id::NarrowScreen, RowKind::Segmented, m.narrow_screen)));
        rows.push_back(full(row(Id::DepthFactor, RowKind::Segmented, m.depth_factor)));
    }
    return rows;
}

} // namespace

// AzaharFrame autosave
SettingsSection SectionOf(Id id) {
    switch (id) {
    case Id::Depth3d:
    case Id::InternalResolution:
    case Id::DynamicResolution:
    case Id::TextureFilter:
    case Id::AccurateImageCopies: // AzaharFrame accxfer
    case Id::AsyncShaders:
        return SettingsSection::Graphics;
    case Id::ImmersiveMode:
    case Id::WiderView:
    case Id::GameFov:
    case Id::MatchCamera:
    case Id::NarrowScreen:
    case Id::DepthFactor:
        return SettingsSection::Immersive;
    default:
        return SettingsSection::General;
    }
}

bool PerGameSetting(Id id) {
    switch (id) {
    case Id::HideBottomScreen:
    case Id::ScreenShape:
    case Id::ImmersiveMode:
    case Id::WiderView:
    case Id::GameFov:
    case Id::MatchCamera: // AzaharFrame depth
    case Id::NarrowScreen:
    case Id::DepthFactor:
    case Id::Depth3d: // AzaharFrame depth3d
    case Id::InternalResolution: // AzaharFrame menuux: Azahar's own per-game settings
    case Id::TextureFilter:
    case Id::AccurateImageCopies: // AzaharFrame accxfer
    case Id::AsyncShaders:
    case Id::FastForwardAudio: // AzaharFrame ffaudio
    case Id::Autosave: // AzaharFrame autosave: the Saves rows (General's only per-game ones)
    case Id::AutosaveEvery:
    case Id::AutosaveKeep:
    case Id::TouchpadTouch: // AzaharFrame reticle: the Controls tab's rows
    case Id::BottomScreenReticle:
    case Id::ReticleSpeed:
    case Id::ReticleAtEdge:
        return true;
    default:
        return false;
    }
}

// AzaharFrame reticle
const std::vector<Id>& ControlsScopeRows() {
    static const std::vector<Id> kRows{Id::TouchpadTouch, Id::BottomScreenReticle,
                                       Id::ReticleSpeed, Id::ReticleAtEdge};
    return kRows;
}

// AzaharFrame useglobal; reticle: any tab with the scope control
std::vector<Id> GameValueRows(const MenuModel& model, Tab tab, SettingsSection section) {
    std::vector<Id> ids;
    const auto add = [&](Id id) {
        if (PerGameSetting(id) && model.game_value[static_cast<std::size_t>(id)]) {
            ids.push_back(id);
        }
    };
    if (tab == Tab::Controls) {
        for (const Id id : ControlsScopeRows()) {
            add(id);
        }
    } else if (tab == Tab::Settings || tab == Tab::Position) {
        for (const Row& r : RowsFor(tab, model, section)) {
            add(r.id);
        }
    }
    return ids;
}

std::string GameValueCount(std::size_t n) {
    if (n == 0) {
        return "No Per-Game Values Set";
    }
    return std::to_string(n) + (n == 1 ? " Per-Game Value Set" : " Per-Game Values Set");
}

const std::vector<std::string>& OptionsFor(Id id) {
    static const std::vector<std::string> kNone;
    static const std::vector<std::string> kShape{"3DS (5:3)",       "Widescreen (16:9)",
                                                 "Ultrawide (21:9)", "Standard (4:3)",
                                                 "3:2",             "16:10"};
    static const std::vector<std::string> kCurve{"Off", "15°", "30°", "45°",
                                                "60°", "75°", "90°"};
    static const std::vector<std::string> kLayout{"Stacked", "Side by Side"};
    static const std::vector<std::string> kMove{"Off", "Secondary", "Both"};
    static const std::vector<std::string> kTouchpad{"Tap for Input", "Click for Input"}; // menuux
    static const std::vector<std::string> kReticle{"Off", "Left Controller", "Right Controller",
                                                   "Headset"}; // AzaharFrame input2: Headset
    static const std::vector<std::string> kAtEdge{"Follow", "Snap to Middle"};
    static const std::vector<std::string> kMouseMode{"Bottom Screen", "Pointer"}; // padsui
    static const std::vector<std::string> kStatsAt{"In Front", "On Wrist"}; // statspos
    static const std::vector<std::string> kPage{"In Game", "Azahar's Window"};
    static const std::vector<std::string> kModsSegments{"Cheats", "Mods", "Textures"}; // cheatstab
    static const std::vector<std::string> kScope{"Global", "Per-Game"};
    static const std::vector<std::string> kSections{"General", "Graphics", "Immersive"}; // autosave
    static const std::vector<std::string> kBackground{"Black", "Passthrough"};
    static const std::vector<std::string> kFace{"By Position (Nintendo)", "By Label (Xbox)"};
    static const std::vector<std::string> kMotion{"Off", "Right Controller", "Left Controller", "Headset"};
    static const std::vector<std::string> kScale{"100%", "150%", "200%"};
    static const std::vector<std::string> kMode{"Off", "Wide (3DoF)",
                                                "Full (6DoF): Ocarina of Time, Majora's Mask",
                                                "Full (6DoF): Other Games"};
    static const std::vector<std::string> kWider{"Off", "1.1x", "1.2x", "1.3x", "1.4x"};
    static const std::vector<std::string> kNarrow{"Off", "10%", "20%", "30%"};
    static const std::vector<std::string> kDepth{"1x", "10x", "100x"};
    // AzaharFrame menuux
    static const std::vector<std::string> kResolution{
        "1x (400x240)",   "2x (800x480)",   "3x (1200x720)",  "4x (1600x960)",
        "5x (2000x1200)", "6x (2400x1440)", "7x (2800x1680)", "8x (3200x1920)"};
    static const std::vector<std::string> kTexFilter{"None", "Bicubic", "ScaleForce", "xBRZ",
                                                     "MMPX"};
    static const std::vector<std::string> kFastForward{"150%", "200%", "300%", "400%"};
    static const std::vector<std::string> kFastForwardAudio{"Sped Up", "Normal Pitch", "Normal Speed", "Muted"};
    static const std::vector<std::string> kImageCopies{"Off", "Automatic", "On"}; // xfergpu
    static_assert(kMaxResolution == 8 && kTextureFilters.size() == 5 &&
                  kFastForwardSpeeds.size() == 4);
    switch (id) {
    case Id::InternalResolution:
        return kResolution;
    case Id::TextureFilter:
        return kTexFilter;
    case Id::FastForwardSpeed:
        return kFastForward;
    case Id::FastForwardAudio:
        return kFastForwardAudio;
    case Id::AccurateImageCopies: // AzaharFrame xfergpu
        return kImageCopies;
    case Id::ScreenShape:
        return kShape;
    case Id::ScreenCurve:
        return kCurve;
    case Id::ScreenLayout:
        return kLayout;
    case Id::MoveScreens:
        return kMove;
    case Id::TouchpadTouch:
        return kTouchpad;
    case Id::BottomScreenReticle: // AzaharFrame mouse
        return kReticle;
    case Id::ReticleAtEdge:
        return kAtEdge;
    case Id::MouseMode:
        return kMouseMode;
    case Id::StatsPosition:
        return kStatsAt;
    case Id::ControlsPage:
        return kPage;
    case Id::ModsSegment:
        return kModsSegments;
    case Id::Scope:
        return kScope;
    case Id::SettingsSection: // AzaharFrame autosave
        return kSections;
    case Id::Background:
        return kBackground;
    case Id::FaceButtons:
        return kFace;
    case Id::MotionControls:
        return kMotion;
    case Id::WindowScale:
        return kScale;
    case Id::ImmersiveMode:
        return kMode;
    case Id::WiderView:
        return kWider;
    case Id::NarrowScreen:
        return kNarrow;
    case Id::DepthFactor:
        return kDepth;
    default:
        return kNone;
    }
}

std::string LabelFor(Id id) {
    switch (id) {
    case Id::Continue:
        return "Continue";
    case Id::QuickSave:
        return "Quick Save";
    case Id::QuickLoad:
        return "Quick Load";
    case Id::SaveState:
        return "Save State";
    case Id::LoadState:
        return "Load State";
    case Id::Screenshot:
        return "Screenshot";
    case Id::Restart:
        return "Restart";
    case Id::AzaharWindow:
        return "Azahar's Window";
    case Id::SwitchToFlat:
        return "Switch to Flat";
    case Id::CloseGame:
        return "Close Game";
    case Id::ResetPosition:
        return "Reset Position";
    case Id::SwapScreens:
        return "Swap Screens";
    case Id::HideBottomScreen:
        return "Hide Bottom Screen";
    case Id::ScreenShape:
        return "Screen Shape";
    case Id::ScreenCurve:
        return "Screen Curve";
    case Id::ScreenLayout:
        return "Screen Layout";
    case Id::MoveScreens:
        return "Detach Screens";
    case Id::TouchpadTouch: // AzaharFrame menuux
        return "Touchpad";
    case Id::BottomScreenReticle: // AzaharFrame mouse
        return "Bottom Screen Reticle";
    case Id::ReticleAtEdge:
        return "At Edge";
    case Id::ReticleSpeed: // AzaharFrame reticle
        return "Reticle Speed";
    case Id::ScopeMore:
        return "Use Global Values for All";
    case Id::MouseMode: // AzaharFrame padsui
        return "Mouse Mode";
    case Id::RecentreAtEdge:
        return "Recentre at Edge";
    case Id::UseDevice:
        return "Use This Device";
    case Id::CursorSpeed:
        return "Cursor Speed";
    case Id::EditControls:
        return "Edit Controls";
    case Id::RemapCancel:
        return "Cancel";
    case Id::RemapReset:
        return "Reset to Default";
    case Id::RemapDone:
        return "Done";
    case Id::ImmersiveOffset:
        return "Immersive Offset";
    case Id::ImmersiveOffsetReset:
        return "Reset";
    case Id::ShowStats:
        return "Show Stats";
    case Id::StatsPosition:
        return "Stats Position";
    case Id::Background:
        return "Background";
    case Id::FaceButtons:
        return "Face Buttons";
    case Id::MotionControls:
        return "Motion Controls";
    case Id::ShowControllers:
        return "Show Controllers";
    case Id::WindowScale:
        return "Window Scale";
    case Id::Depth3d:
        return "3D Depth";
    case Id::InternalResolution: // AzaharFrame menuux
        return "Internal Resolution";
    case Id::DynamicResolution:
        return "Dynamic Resolution";
    case Id::TextureFilter:
        return "Texture Filter";
    case Id::AsyncShaders:
        return "Async Shader Compilation";
    case Id::AccurateImageCopies: // AzaharFrame accxfer
        return "Accurate Image Copies";
    case Id::FastForwardSpeed:
        return "Fast Forward Speed";
    case Id::FastForwardAudio:
        return "Fast Forward Audio";
    case Id::ImmersiveMode:
        return "Immersive Mode";
    case Id::WiderView:
        return "Wider View";
    case Id::GameFov:
        return "Immersive Game FOV";
    case Id::MatchCamera: // AzaharFrame depth
        return "Match Game Camera";
    case Id::NarrowScreen:
        return "Narrow Screen";
    case Id::DepthFactor:
        return "Depth Factor Multiplier";
    case Id::MoreSettings:
        return "More Settings";
    case Id::SavesHeader: // AzaharFrame autosave
        return "Saves";
    case Id::HelpHeader: // AzaharFrame crashreport
        return "Help";
    case Id::ReportProblem:
        return "Report a Problem";
    case Id::AboutApp:
        return "About Azahar for Frame";
    case Id::UseGlobal: // AzaharFrame useglobal: the chip
        return "This Game";
    case Id::Autosave:
    case Id::AutosaveCard:
        return "Autosave";
    case Id::AutosaveEvery:
        return "Every";
    case Id::AutosaveKeep:
        return "Keep";
    case Id::AutosaveEntry:
        return "Autosave";
    case Id::AutosaveSettings:
        return "Settings";
    case Id::UndoLoad:
        return "Undo Load";
    case Id::DownloadCheats: // AzaharFrame cheatstab
        return "Download";
    case Id::CheatsWindow:
        return "Azahar's Window";
    case Id::InstallMods: // AzaharFrame texui
        return "Install Mods";
    case Id::ModRemove:
        return "Remove";
    case Id::ModInstall:
        return "Install";
    case Id::CustomTextures:
        return "Custom Textures";
    case Id::TexturePreload:
        return "Preload";
    case Id::RestartGame:
        return "Restart";
    case Id::TurnBackOn: // AzaharFrame compat
        return "Turn Back On";
    case Id::DownloadPacks:
        return "Download Packs";
    case Id::PackDownload:
        return "Download";
    case Id::PackCancel:
    case Id::ModCancel: // AzaharFrame integ3
        return "Cancel";
    case Id::PackDelete:
        return "Delete";
    case Id::PackCustomize: // AzaharFrame texparts
        return "Customize";
    case Id::PartsAllOn:
        return "All On";
    case Id::PartsAllOff:
        return "All Off";
    case Id::PartUp:
    case Id::LayerUp: // AzaharFrame texlayers
        return "Move Up";
    case Id::PartDown:
    case Id::LayerDown:
        return "Move Down";
    case Id::InstallFromZip: // AzaharFrame zipinstall
        return "Install from Zip";
    case Id::HomebrewInstall: // AzaharFrame emerald
        return "Install";
    case Id::HomebrewUpdate:
        return "Update";
    case Id::HomebrewPlay:
        return "Play";
    case Id::HomebrewReinstall: // AzaharFrame ui3
        return "Reinstall";
    case Id::HomebrewCheck:
        return "Check for Updates";
    case Id::OptimizePack: // AzaharFrame ui3
        return "Optimize for Frame";
    case Id::OptimizeStop:
        return "Stop";
    case Id::UseOriginal:
        return "Use Original";
    case Id::RemoveOriginal: // AzaharFrame ui3
        return "Remove Original";
    case Id::OptimizeNudge:
        return "Optimize";
    case Id::AddCheat: // AzaharFrame ui2
        return "Add Cheat";
    case Id::CheatsOtherVersion: // AzaharFrame scroll
        return "For Another Version";
    case Id::InstallCheatFile:
        return "Install from File";
    default:
        return {};
    }
}

std::string ExplainOption(Id id, int index) {
    if (id == Id::AccurateImageCopies) { // AzaharFrame xfergpu: the owner's words
        switch (index) {
        case 0:
            return "No effect on performance, but graphical glitches may occur.";
        case 1:
            return "Fixes glitches only where a game needs it. Little to no effect on performance.";
        case 2:
            return "Makes every image copy exact. Substantially lowers performance.";
        default:
            break;
        }
    }
    return "";
}

bool HasOptionTips(Id id) {
    return id == Id::AccurateImageCopies;
}

std::string ExplainFor(Id id) {
    switch (id) {
    case Id::UseGlobal: // AzaharFrame useglobal: the chip's action
        return "Use Global Value";
    case Id::ReticleSpeed: // AzaharFrame reticle
        return "How far the reticle moves when you turn your head or the controller.";
    case Id::HideBottomScreen:
        return "Shows only the top screen.";
    case Id::ScreenShape:
        return "The top screen's shape, for games or mods that change it.";
    case Id::ScreenCurve:
        return "Bends the Primary Screen so its sides come toward you.";
    case Id::ScreenLayout:
        return "The Secondary Screen below the Primary Screen, or to its right.";
    case Id::ImmersiveOffset:
        return "Moves the immersive screen to fit your lenses, in every game.";
    case Id::ShowStats:
        return "Shows the stats while you play.";
    case Id::StatsPosition:
        return "Where the stats show: in front of you, or on your left wrist.";
    case Id::Background:
        return "What shows behind the screens.";
    case Id::FaceButtons:
        return "How A, B, X and Y map to the 3DS.";
    case Id::MotionControls:
        return "The controller or headset that acts as the 3DS gyroscope.";
    case Id::ShowControllers:
        return "Shows the controllers in VR.";
    case Id::WindowScale:
        return "Makes text and buttons in Azahar's window bigger.";
    case Id::Depth3d:
        return "How deep the 3D looks in this game.";
    case Id::InternalResolution: // AzaharFrame menuux
        return "How sharp the game is drawn. Higher needs more of the GPU.";
    case Id::DynamicResolution:
        return "Lowers the resolution a step while the GPU cannot keep up.";
    case Id::TextureFilter:
        return "Smooths the game's textures. Some filters cost frames.";
    case Id::AsyncShaders:
        return "Fewer stutters while new effects load. From the next start.";
    case Id::AccurateImageCopies: // AzaharFrame accxfer
        return "Fixes flickering, shimmering or glow glitches in some games.";
    case Id::FastForwardSpeed:
        return "How fast View + Right Trigger runs the game.";
    case Id::FastForwardAudio:
        return "How the sound plays while fast forwarding. Normal Pitch keeps voices and music at their usual pitch. Normal Speed plays at real speed and skips ahead to keep up.";
    case Id::ImmersiveMode:
        return "Draws a wider view of the game around you.";
    case Id::WiderView:
        return "Shows more of the game around the edges. Some games leave them empty.";
    case Id::GameFov:
        return "The game camera's vertical field of view.";
    case Id::MatchCamera:
        return "Reads the game's camera angle and shape while it runs.";
    case Id::NarrowScreen:
        return "Narrows the screen so the side HUD moves into view.";
    case Id::DepthFactor:
        return "How far the game camera moves with your head.";
    case Id::Autosave: // AzaharFrame autosave
        return "Saves on its own as you play, when this menu opens and when the game closes.";
    case Id::AutosaveEvery:
        return "Minutes of play between autosaves.";
    case Id::AutosaveKeep:
        return "How many autosaves the game keeps. The oldest go.";
    default:
        return {};
    }
}

std::string ExplainLock(Lock lock) {
    switch (lock) {
    case Lock::GameValue:
        return "This game has its own value: choose Per-Game to change it.";
    case Lock::FullOnly:
        return "Only in the Full immersive modes.";
    case Lock::Unavailable:
        return "Not available on this headset.";
    case Lock::AtLowest: // AzaharFrame menuux
        return "Internal Resolution is already 1x, the lowest.";
    case Lock::InPassthrough: // AzaharFrame vis
        return "Off while the Background is Passthrough.";
    case Lock::AutosaveOff: // AzaharFrame autosave
        return "Turn on Autosave to change it.";
    case Lock::NoGameValues: // AzaharFrame reticle
        return "Nothing on this tab is set for this game.";
    case Lock::NoGame:
        return "Per-Game values need a running game.";
    default:
        return {};
    }
}

// AzaharFrame autosave
void DrawAutosavedLabel(Kit& kit, const Rect& r) {
    const float h = r.H();
    kit.Fill(r, h * 0.5f, 0x1C1C22, 0.92f);
    const float icon = h * 0.42f, size = h * 0.34f;
    const std::string text = "Autosaved";
    const float tw = kit.TextWidthSized(size, true, text);
    const float gap = h * 0.12f;
    const float x = r.Cx() - (icon + gap + tw) * 0.5f;
    kit.Icon(x + icon * 0.5f, r.Cy(), icon, Sym::kSave, false, Tok::kOnSurfaceVariant);
    kit.TextSized(x + icon + gap, r.Cy() + kit.CapHeightSized(size, true) * 0.5f, size, true,
                  0xF0F0F5, text);
}

// AzaharFrame menuux
std::string TouchpadSupporting(int value) {
    return value == 1 ? "Press the pad to touch the bottom screen. Hold it down to drag."
                      : "Your finger on the pad touches the bottom screen.";
}

SliderRange SliderFor(Id id) {
    if (id == Id::GameFov) {
        return {30.0f, 110.0f, 1.0f};
    }
    if (id == Id::Depth3d) {
        return {0.0f, 100.0f, 5.0f};
    }
    if (id == Id::CursorSpeed) { // AzaharFrame padsui: five steps, Normal in the middle
        return {0.0f, 4.0f, 1.0f};
    }
    if (id == Id::AutosaveEvery) { // AzaharFrame autosave: kAutosaveEvery's stops
        return {0.0f, 4.0f, 1.0f};
    }
    if (id == Id::AutosaveKeep) {
        return {1.0f, 10.0f, 1.0f};
    }
    if (id == Id::ReticleSpeed) { // AzaharFrame reticle: 0.5x to 4.0x in tenths
        return {5.0f, 40.0f, 1.0f};
    }
    return {};
}

float MenuLayout::SliderAt(Id id, float x) const {
    for (const SliderTrack& s : sliders) {
        if (s.id == id) {
            // As Kit::Slider places the handle (4 dp wide, centred on the value).
            return std::clamp((x - s.x0 - 2.0f) / std::max(1.0f, s.x1 - s.x0 - 4.0f), 0.0f, 1.0f);
        }
    }
    return -1.0f;
}

State TabMenu::StateOf(const MenuView& view, Target t, bool enabled) const {
    if (!enabled) {
        return State::Disabled;
    }
    if (view.pressed == t) {
        return State::Pressed;
    }
    if (view.hover == t) {
        return State::Hover;
    }
    return State::Rest;
}

void TabMenu::AddHit(const Rect& r, Target t, const Rect& clip) {
    const Rect c{std::max(r.x0, clip.x0), std::max(r.y0, clip.y0), std::min(r.x1, clip.x1),
                 std::min(r.y1, clip.y1)};
    if (c.x1 > c.x0 && c.y1 > c.y0) {
        layout.hits.push_back({c, t});
    }
}

Target MenuLayout::HitTest(float x, float y) const {
    for (auto it = hits.rbegin(); it != hits.rend(); ++it) {
        if (it->r.Contains(x, y)) {
            return it->t;
        }
    }
    return {};
}

void TabMenu::Draw(Kit& kit, const MenuModel& model, const MenuView& view) {
    layout = {};
    anchors.clear();
    const Rect panel{0, 0, kPanelW, kPanelH};
    kit.Fill(panel, kPanelRadius, Tok::kSurfaceContainer);
    DrawRail(kit, view);
    const Rect pane{kRailW, kPaneMargin, kPanelW - kPaneMargin, kPanelH - kPaneMargin};
    kit.Fill(pane, kPaneRadius, Tok::kSurface);
    switch (view.tab) {
    case Tab::Main:
        if (view.page == Page::Tabs) {
            DrawMain(kit, model, view, pane);
        } else {
            DrawSlots(kit, model, view, pane);
        }
        break;
    case Tab::Stats:
        DrawStats(kit, model, view, pane);
        break;
    case Tab::Controls:
        DrawControls(kit, model, view, pane);
        break;
    case Tab::Mods: // AzaharFrame cheatstab
        DrawMods(kit, model, view, pane);
        break;
    case Tab::Homebrew: // AzaharFrame ui3
        DrawHomebrew(kit, model, view, pane);
        break;
    default:
        DrawRows(kit, model, view, pane);
        break;
    }
    if (model.notice.kind != ModsDialogKind::None) { // AzaharFrame savestate: over everything
        DrawModsDialog(kit, model.notice, view);
        return;
    }
    DrawTooltip(kit, view, pane);
}

void TabMenu::DrawRail(Kit& kit, const MenuView& view) {
    const float total = kRailItemH * static_cast<float>(kTabs.size());
    float y = (kPanelH - total) * 0.5f;
    const Rect all{0, 0, kPanelW, kPanelH};
    for (int i = 0; i < static_cast<int>(kTabs.size()); i++) {
        const Rect r{0.0f, y, kRailW, y + kRailItemH};
        const Target t{Id::TabButton, i};
        const bool active = static_cast<int>(view.tab) == i;
        kit.RailItem(r, kTabs[static_cast<std::size_t>(i)].icon, kTabs[static_cast<std::size_t>(i)].label,
                     active, StateOf(view, t));
        AddHit(r, t, all);
        y += kRailItemH;
    }
}

void TabMenu::DrawMain(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane) {
    const Rect c{pane.x0 + kPad, pane.y0 + kPad, pane.x1 - kPad, pane.y1 - kPad};
    float y = c.y0;
    if (!model.game_title.empty()) {
        kit.Text(c.x0, y + 28.0f, Type::Headline, Tok::kOnSurface,
                 kit.Fit(Type::Headline, model.game_title, c.W()));
        y += 52.0f;
    }
    const Rect cont{c.x0, y, c.x1, y + 64.0f};
    kit.Button(cont, ButtonStyle::Filled, Sym::kPlayArrow, LabelFor(Id::Continue),
               StateOf(view, {Id::Continue}), Type::Title);
    AddHit(cont, {Id::Continue}, pane);
    y = cont.y1 + 20.0f;

    // AzaharFrame menuux: eight tiles, four to a row, and Switch to Flat under them as wide as
    // Continue (the same button ends the Settings tab).
    struct TileDef {
        Id id;
        char32_t icon;
        ButtonStyle style;
    };
    const std::array<TileDef, 8> tiles{{
        {Id::QuickSave, Sym::kSave, ButtonStyle::Tonal},
        {Id::QuickLoad, Sym::kHistory, ButtonStyle::Tonal},
        {Id::SaveState, Sym::kSaveAs, ButtonStyle::Tonal},
        {Id::LoadState, Sym::kFolderOpen, ButtonStyle::Tonal},
        {Id::Screenshot, Sym::kPhotoCamera, ButtonStyle::Tonal},
        {Id::Restart, Sym::kRestartAlt, ButtonStyle::Tonal},
        {Id::AzaharWindow, Sym::kOpenInNew, ButtonStyle::Tonal},
        {Id::CloseGame, Sym::kClose, ButtonStyle::Danger},
    }};
    const float gap = 12.0f;
    const Rect flat{c.x0, c.y1 - 56.0f, c.x1, c.y1};
    kit.Button(flat, ButtonStyle::Tonal, Sym::kTv, LabelFor(Id::SwitchToFlat),
               StateOf(view, {Id::SwitchToFlat}), Type::Title);
    AddHit(flat, {Id::SwitchToFlat}, pane);
    constexpr int kColumns = 4;
    const float tw = (c.W() - gap * (kColumns - 1)) / kColumns;
    const float th = (flat.y0 - gap - y - gap) / 2.0f;
    for (std::size_t i = 0; i < tiles.size(); i++) {
        const float x = c.x0 + static_cast<float>(i % kColumns) * (tw + gap);
        const float ty = y + static_cast<float>(i / kColumns) * (th + gap);
        const Rect r{x, ty, x + tw, ty + th};
        const bool enabled = tiles[i].id != Id::QuickLoad || model.quick_load_available;
        kit.Tile(r, tiles[i].style, tiles[i].icon, LabelFor(tiles[i].id),
                 StateOf(view, {tiles[i].id}, enabled));
        if (enabled) {
            AddHit(r, {tiles[i].id}, pane);
        }
    }
}

// The Main tab's Save State and Load State: the slots of the game, two columns of five.
void TabMenu::DrawSlots(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane) {
    const Rect c{pane.x0 + kPad, pane.y0 + kPad, pane.x1 - kPad, pane.y1 - kPad};
    const bool saving = view.page == Page::SaveSlots;
    const Rect back{c.x0 - 4.0f, c.y0, c.x0 + 44.0f, c.y0 + 48.0f};
    kit.StateLayer(back, 24.0f, Tok::kOnSurfaceVariant, StateOf(view, {Id::SlotsBack}));
    kit.Icon(back.Cx(), back.Cy(), 24.0f, Sym::kArrowBack, false, Tok::kOnSurfaceVariant);
    AddHit(back, {Id::SlotsBack}, pane);
    kit.TextMid(back.x1 + 12.0f, back.Cy(), Type::Headline, Tok::kOnSurface,
                LabelFor(saving ? Id::SaveState : Id::LoadState));
    float top = back.y1 + 16.0f;
    const float gap = 12.0f;
    float bottom = c.y1;
    Rect card{};
    if (!saving) {
        // AzaharFrame autosave: the Autosave card over the slots (owner's Option B), and after an
        // autosave was loaded the Undo Load snackbar under them.
        card = {c.x0, top, c.x1, top + kAutosaveCardH};
        DrawAutosaveCard(kit, model, view, card, pane);
        top = card.y1 + gap;
        if (!model.undo_load.empty()) {
            bottom = DrawActionSnackbar(kit, view, pane, model.undo_load, Id::UndoLoad, c.y1);
        }
    }
    // With the snackbar too the rows keep 48 dp: 10 dp between them then.
    const float row_gap = !saving && !model.undo_load.empty() ? 10.0f : gap;
    const float cw = (c.W() - gap) * 0.5f;
    const float rh = (bottom - top - row_gap * 4.0f) / 5.0f;
    for (int i = 0; i < kSlots; i++) {
        const int slot = i + 1;
        std::string time;
        bool other = false; // AzaharFrame savestate: from another build: dimmed, still tappable
        for (const SaveSlot& s : model.slots) {
            if (s.slot == slot) {
                time = s.time;
                other = s.other;
            }
        }
        const bool empty = time.empty();
        const bool enabled = saving || !empty;
        const float x = c.x0 + static_cast<float>(i / 5) * (cw + gap);
        const float y = top + static_cast<float>(i % 5) * (rh + row_gap);
        const Rect r{x, y, x + cw, y + rh};
        const Target t{Id::Slot, slot};
        const State st = StateOf(view, t, enabled);
        // AzaharFrame savestate: another build's state is dimmed less than an empty slot: it
        // still loads (after Load Anyway's question)
        const float op = !enabled ? kDisabledContent : other ? 0.62f : 1.0f;
        kit.Fill(r, 16.0f, Tok::kSurfaceContainerHigh, enabled ? 1.0f : 0.5f);
        kit.StateLayer(r, 16.0f, Tok::kOnSurface, st);
        kit.TextMid(r.x0 + 20.0f, r.Cy(), Type::Label, Tok::kOnSurface,
                    "Slot " + std::to_string(slot), 0, op);
        kit.TextMid(r.x1 - 20.0f, r.Cy(), Type::Body, Tok::kOnSurfaceVariant,
                    empty ? std::string("Empty") : time, 2, op);
        if (enabled) {
            AddHit(r, t, pane);
        }
    }
    if (!saving && view.open_menu == Id::AutosaveCard) {
        DrawAutosaveList(kit, model, view, card);
    }
}

// AzaharFrame autosave: an autosave's picture (an icon when it has none), 5:3.
static void AutosavePictureIn(Kit& kit, const Rect& r, const AutosaveCard* a, uint32_t icon_rgb) {
    if (a && a->picture && a->picture->w > 0 &&
        a->picture->rgba.size() >= static_cast<std::size_t>(a->picture->w * a->picture->h * 4)) {
        kit.Image(a->picture->rgba.data(), a->picture->w, a->picture->h, r);
        return;
    }
    kit.Fill(r, 8.0f, Tok::kSurfaceContainerHighest);
    kit.Icon(r.Cx(), r.Cy(), 28.0f, Sym::kHistory, false, icon_rgb);
}

// AzaharFrame autosave: Load State's card. On: the newest autosave ("Autosave · 3 Minutes Ago",
// its time at the right); Keep 1 loads it, more opens the list (a drop-down arrow). Off: "Autosave:
// Off" and a Settings button to the Saves rows.
void TabMenu::DrawAutosaveCard(Kit& kit, const MenuModel& model, const MenuView& view,
                               const Rect& r, const Rect& pane) {
    const uint32_t on_rgb = Tok::kOnSecondaryContainer;
    const bool open = view.open_menu == Id::AutosaveCard;
    const AutosaveCard* newest = model.autosaves.empty() ? nullptr : &model.autosaves.front();
    const bool tappable = model.autosave && newest != nullptr;
    kit.Fill(r, 16.0f, Tok::kSecondaryContainer, model.autosave && !newest ? 0.5f : 1.0f);
    if (tappable) {
        kit.StateLayer(r, 16.0f, on_rgb, open ? State::Pressed : StateOf(view, {Id::AutosaveCard}));
        AddHit(r, {Id::AutosaveCard}, pane);
    }
    const Rect pic{r.x0 + 12.0f, r.y0 + 8.0f, r.x0 + 12.0f + (r.H() - 16.0f) * 5.0f / 3.0f,
                   r.y1 - 8.0f};
    if (!model.autosave) {
        kit.Icon(pic.Cx(), pic.Cy(), 32.0f, Sym::kHistory, false, on_rgb);
        kit.TextMid(pic.x1 + 16.0f, r.Cy(), Type::Label, on_rgb, "Autosave: Off");
        const std::string& label = LabelFor(Id::AutosaveSettings);
        const float bw = std::max(64.0f, kit.TextWidth(Type::Label, label) + 24.0f);
        const Rect b{r.x1 - 12.0f - bw, r.Cy() - 24.0f, r.x1 - 12.0f, r.Cy() + 24.0f};
        kit.Button(b, ButtonStyle::Text, 0, label, StateOf(view, {Id::AutosaveSettings}));
        AddHit(b, {Id::AutosaveSettings}, pane);
        return;
    }
    if (!newest) {
        // On, and none made yet (the first comes after a minute of play).
        kit.Icon(pic.Cx(), pic.Cy(), 32.0f, Sym::kHistory, false, on_rgb, kDisabledContent);
        kit.TextMid(pic.x1 + 16.0f, r.Cy(), Type::Label, on_rgb, "Autosave · None Yet", 0,
                    kDisabledContent);
        return;
    }
    AutosavePictureIn(kit, pic, newest, on_rgb);
    const float op = newest->other ? 0.62f : 1.0f;
    float right = r.x1 - 20.0f;
    if (model.autosaves.size() > 1) {
        kit.Icon(r.x1 - 28.0f, r.Cy(), 24.0f, Sym::kArrowDropDown, false, on_rgb);
        right = r.x1 - 52.0f;
    }
    const float time_w = kit.TextWidth(Type::Body, newest->time);
    kit.TextMid(right, r.Cy(), Type::Body, on_rgb, newest->time, 2, 0.8f * op);
    kit.TextMid(pic.x1 + 16.0f, r.Cy(), Type::Label, on_rgb,
                kit.Fit(Type::Label, "Autosave \u00b7 " + newest->ago,
                        right - time_w - 16.0f - pic.x1 - 16.0f),
                0, op);
}

// AzaharFrame autosave: the card's list, over the slots: picture, how long ago, the time. Up to
// six in one column at the card's right; more in two columns as wide as the card.
void TabMenu::DrawAutosaveList(Kit& kit, const MenuModel& model, const MenuView& view,
                               const Rect& card) {
    const int n = static_cast<int>(model.autosaves.size());
    if (n == 0) {
        return;
    }
    const int columns = n > 6 ? 2 : 1;
    const int per_column = (n + columns - 1) / columns;
    const float item = kAutosaveItemH, pad = 8.0f;
    const float w = columns == 1 ? std::min(card.W(), 420.0f) : card.W();
    const Rect box{card.x1 - w, card.y1 + 4.0f, card.x1,
                   card.y1 + 4.0f + pad * 2.0f + item * static_cast<float>(per_column)};
    kit.Fill(box, 16.0f, Tok::kSurfaceContainerHigh);
    kit.Stroke(box, 16.0f, 1.0f, Tok::kOutlineVariant);
    const Rect everything{0, 0, kPanelW, kPanelH};
    AddHit(box, {Id::None, kMenuAreaIndex}, everything); // the list's own area eats clicks
    const float cw = (box.W() - 8.0f) / static_cast<float>(columns);
    for (int i = 0; i < n; i++) {
        const AutosaveCard& a = model.autosaves[static_cast<std::size_t>(i)];
        const float x = box.x0 + 4.0f + cw * static_cast<float>(i / per_column);
        const float y = box.y0 + pad + item * static_cast<float>(i % per_column);
        const Rect ir{x, y, x + cw, y + item};
        const Target t{Id::AutosaveEntry, i};
        kit.StateLayer(ir, 12.0f, Tok::kOnSurface, StateOf(view, t));
        const float op = a.other ? 0.62f : 1.0f;
        const float ph = ir.H() - 12.0f;
        const Rect pic{ir.x0 + 12.0f, ir.Cy() - ph * 0.5f, ir.x0 + 12.0f + ph * 5.0f / 3.0f,
                       ir.Cy() + ph * 0.5f};
        AutosavePictureIn(kit, pic, &a, Tok::kOnSurfaceVariant);
        const float clock_w = kit.TextWidth(Type::Body, a.clock);
        kit.TextMid(ir.x1 - 16.0f, ir.Cy(), Type::Body, Tok::kOnSurfaceVariant, a.clock, 2, op);
        kit.TextMid(pic.x1 + 16.0f, ir.Cy(), Type::Label, Tok::kOnSurface,
                    kit.Fit(Type::Label, a.ago, ir.x1 - 16.0f - clock_w - 12.0f - pic.x1 - 16.0f),
                    0, op);
        AddHit(ir, t, everything);
    }
}

void TabMenu::DrawStats(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane) {
    const Rect c{pane.x0 + kPad, pane.y0 + kPad, pane.x1 - kPad, pane.y1 - kPad};
    // Show Stats: a switch row (the whole row toggles it).
    const Lock lock = model.locked[static_cast<std::size_t>(Id::ShowStats)];
    const bool enabled = lock == Lock::None;
    const Rect row{c.x0 - 12.0f, c.y0 - 8.0f, c.x1 + 12.0f, c.y0 - 8.0f + kRowH - 8.0f};
    const State st = StateOf(view, {Id::ShowStats}, enabled);
    kit.StateLayer(row, 16.0f, Tok::kOnSurface, st);
    kit.TextMid(c.x0, row.Cy(), Type::Body, Tok::kOnSurface, LabelFor(Id::ShowStats), 0,
                enabled ? 1.0f : kDisabledContent);
    kit.Switch({c.x0, row.y0, c.x1, row.y1}, model.show_stats, st);
    AddHit(row, {Id::ShowStats, enabled ? -1 : kLabelIndex}, pane);
    anchors.push_back({Id::ShowStats, c.x0, row.y0 + 6.0f, lock});
    // AzaharFrame statspos: Stats Position under it, a label and its two choices.
    const Rect at_row{row.x0, row.y1, row.x1, row.y1 + kRowH - 8.0f};
    {
        const Lock at_lock = model.locked[static_cast<std::size_t>(Id::StatsPosition)];
        const bool at_enabled = at_lock == Lock::None;
        AddHit(at_row, {Id::StatsPosition, kLabelIndex}, pane);
        anchors.push_back({Id::StatsPosition, c.x0, at_row.y0 + 6.0f, at_lock});
        kit.TextMid(c.x0, at_row.Cy(), Type::Body, Tok::kOnSurface, LabelFor(Id::StatsPosition), 0,
                    at_enabled ? 1.0f : kDisabledContent);
        const auto& opts = OptionsFor(Id::StatsPosition);
        const float w = kit.SegmentedWidth(opts, c.W() * 0.6f);
        const Rect seg{c.x1 - w, at_row.Cy() - kControlH * 0.5f, c.x1,
                       at_row.Cy() + kControlH * 0.5f};
        const int hover = view.hover.id == Id::StatsPosition ? view.hover.index : -1;
        const int pressed = view.pressed.id == Id::StatsPosition ? view.pressed.index : -1;
        kit.Segmented(seg, opts, model.stats_position, hover, pressed, !at_enabled);
        if (at_enabled) {
            const auto e = kit.SegmentEdges(seg, opts);
            for (int i = 0; i < static_cast<int>(opts.size()); i++) {
                AddHit({e[static_cast<std::size_t>(i)], seg.y0, e[static_cast<std::size_t>(i + 1)],
                        seg.y1},
                       {Id::StatsPosition, i}, pane);
            }
        }
    }
    kit.Line(c.x0, at_row.y1 + 8.0f, c.x1, at_row.y1 + 8.0f, 1.0f, Tok::kOutlineVariant);

    const float col_gap = 40.0f;
    const float cw = (c.W() - col_gap) * 0.5f;
    const auto group = [&](float x, float& y, const char* title,
                           const std::vector<std::pair<std::string, std::string>>& rows) {
        if (rows.empty()) {
            return;
        }
        kit.TextMid(x, y + 16.0f, Type::Label, Tok::kPrimary, title);
        y += 36.0f;
        for (const auto& [label, value] : rows) {
            const float cy = y + kStatRowH * 0.5f;
            kit.TextMid(x, cy, Type::Body, Tok::kOnSurfaceVariant, label);
            kit.TextMid(x + cw, cy, Type::Value, Tok::kOnSurface, value, 2);
            y += kStatRowH;
        }
        y += 12.0f;
    };
    float yl = at_row.y1 + 24.0f, yr = yl;
    if (model.stats_game.empty() && model.stats_vr.empty()) {
        kit.TextMid(c.x0, yl + 16.0f, Type::Body, Tok::kOnSurfaceVariant, "No Stats Yet");
    }
    group(c.x0, yl, "Game", model.stats_game);
    group(c.x0 + cw + col_gap, yr, "VR App", model.stats_vr);
    group(c.x0 + cw + col_gap, yr, "System", model.stats_system);
    if (!model.version.empty()) {
        const float cy = c.y1 - kStatRowH * 0.5f;
        kit.TextMid(c.x0, cy, Type::Body, Tok::kOnSurfaceVariant, "Version");
        kit.TextMid(c.x0 + cw, cy, Type::Value, Tok::kOnSurface, model.version, 2);
    }
}

// AzaharFrame padsui: Edit Controls. The title, Touchpad Touch for a device with a pointer, and
// Done on top; in the middle our drawing of a New 3DS XL with a chip for every 3DS input saying
// what drives it on the selected device, in that device's own names (sticks too: two pushes,
// up then right, give one); at the bottom the devices as Material filter chips and Reset to Default, or,
// while a chip listens, the snackbar "Press a Button…" with Cancel.
namespace {

constexpr float kChipH = 40.0f;
constexpr float kChipRadius = 8.0f;
constexpr float kChipText = 16.0f;
constexpr float kGlyph = 24.0f;
constexpr float kChipPadL = 8.0f;  ///< before the glyph
constexpr float kChipGap = 8.0f;   ///< glyph to text, and between chips
constexpr float kChipPadR = 16.0f; ///< after the text
constexpr float kLeader = 24.0f;   ///< the columns' inner edges to the drawing
/// The longest chip the columns must hold whole (a gamepad's stick clicks are as long).
constexpr const char* kWidestChip = "Right Stick Flipped";

// The chips round the drawing: each column top to bottom, Home, Select and Start under it (nine
// chips in the right column would not keep 48 dp targets).
// (Ordered so no two leader lines cross: ZL over L, its button being further in; B over Y, so
// Y's leader passes under B.)
constexpr std::array<int, 7> kLeftChips{DsButton::ZL,   DsButton::L,     kDsCirclePad, DsButton::Up,
                                        DsButton::Left, DsButton::Right, DsButton::Down};
constexpr std::array<int, 7> kRightChips{DsButton::ZR, DsButton::R, kDsCStick, DsButton::X,
                                         DsButton::A,  DsButton::B, DsButton::Y};
// The mouse (no sticks): six chips a column (Y over B there: with the shorter columns Y's leader
// would cross B's button); Touch Screen in the row under the drawing, its
// leader up into the bottom screen; Open Menu (the VR menu's, not the 3DS's) in the settings row.
constexpr std::array<int, 6> kMouseLeftChips{DsButton::ZL,   DsButton::L,     DsButton::Up,
                                             DsButton::Left, DsButton::Right, DsButton::Down};
constexpr std::array<int, 6> kMouseRightChips{DsButton::ZR, DsButton::R, DsButton::X,
                                              DsButton::A,  DsButton::Y, DsButton::B};


bool IsStick(int target) {
    return Remap::IsStick(target);
}

} // namespace

void TabMenu::DrawEditControls(Kit& kit, const MenuModel& model, const MenuView& view,
                               const Rect& pane) {
    static const EditDevice kNoDevice{};
    const EditDevice* sel = model.devices.Selected();
    const EditDevice& dev = sel ? *sel : kNoDevice;
    const bool buttons = HasButtons(dev);
    const bool listening = view.remap.listening >= 0;
    const Rect c{pane.x0 + kPad, pane.y0 + kPad, pane.x1 - kPad, pane.y1 - kPad};

    // ---- Top: title, Touchpad Touch, Done ----
    const float bar_h = kControlH;
    // Material button padding: 16 dp before an icon, 24 after the label (a text button: 12, 16).
    const float done_w = 16.0f + 20.0f + 8.0f + kit.TextWidth(Type::Label, LabelFor(Id::RemapDone)) + 24.0f;
    const Rect done{c.x1 - done_w, c.y0, c.x1, c.y0 + bar_h};
    kit.Button(done, ButtonStyle::Filled, Sym::kCheck, LabelFor(Id::RemapDone),
               StateOf(view, {Id::RemapDone}));
    AddHit(done, {Id::RemapDone}, pane);
    // AzaharFrame padsui: Reset to Default beside Done (the device row has Use This Device).
    float bar_x1 = done.x0 - 8.0f;
    if (buttons) {
        const float reset_w =
            12.0f + 20.0f + 8.0f + kit.TextWidth(Type::Label, LabelFor(Id::RemapReset)) + 16.0f;
        const Rect reset{bar_x1 - reset_w, c.y0, bar_x1, c.y0 + bar_h};
        kit.Button(reset, ButtonStyle::Text, Sym::kRestartAlt, LabelFor(Id::RemapReset),
                   StateOf(view, {Id::RemapReset}));
        AddHit(reset, {Id::RemapReset}, pane);
        bar_x1 = reset.x0 - 8.0f;
    }
    // The device's own setting at the start of the bar, else the title.
    if (!HasPointer(dev)) {
        kit.TextMid(c.x0, c.y0 + bar_h * 0.5f, Type::Title, Tok::kOnSurface, LabelFor(Id::EditControls));
    } else {
        // A touchpad: Touchpad Touch (global). The mouse: its Mouse Mode (AzaharFrame padsui).
        const bool mouse = dev.kind == DeviceKind::Mouse;
        const Id bar_id = mouse ? Id::MouseMode : Id::TouchpadTouch;
        const auto& opts = OptionsFor(bar_id);
        std::string label = LabelFor(bar_id);
        // AzaharFrame menuux: the choices whole; the label only when it fits beside them
        // (Touchpad's "Tap for Input" and "Click for Input" say what they are).
        const float want = kit.SegmentedWidth(opts);
        const float all = bar_x1 - 16.0f - c.x0;
        if (kit.TextWidth(Type::Body, label) + 12.0f + want > all) {
            label.clear();
        }
        const float lw = label.empty() ? -12.0f : kit.TextWidth(Type::Body, label);
        const float room = all - lw - 12.0f;
        const float tw = kit.SegmentedWidth(opts, room);
        const Rect ts{c.x0 + lw + 12.0f, c.y0, c.x0 + lw + 12.0f + tw, c.y0 + bar_h};
        const Lock lock = model.locked[static_cast<std::size_t>(bar_id)];
        const bool enabled = lock == Lock::None;
        kit.TextMid(c.x0, ts.Cy(), Type::Body, Tok::kOnSurface, label, 0,
                    enabled ? 1.0f : kDisabledContent);
        const int th = view.hover.id == bar_id ? view.hover.index : -1;
        const int tp = view.pressed.id == bar_id ? view.pressed.index : -1;
        kit.Segmented(ts, opts, mouse ? dev.mouse_mode : model.touchpad_touch, th, tp, !enabled);
        if (enabled) {
            const auto e = kit.SegmentEdges(ts, opts);
            for (int i = 0; i < static_cast<int>(opts.size()); i++) {
                AddHit({e[static_cast<std::size_t>(i)], ts.y0, e[static_cast<std::size_t>(i + 1)], ts.y1},
                       {bar_id, i}, pane);
            }
        }
    }

    // ---- Middle: the 3DS, the chips round it ----
    const Rect slot{c.x0, c.y1 - kControlH, c.x1, c.y1};
    // AzaharFrame padsui: the mouse (no sticks, six chips a column) has a settings row over the
    // device row: Open Menu, Recentre at Edge, Cursor Speed. Its drawing is a little smaller.
    const bool mouse_page = dev.kind == DeviceKind::Mouse;
    const Rect settings{c.x0, slot.y0 - kChipGap - kChipH, c.x1, slot.y0 - kChipGap};
    const Rect area{c.x0, c.y0 + bar_h + 8.0f, c.x1, (mouse_page ? settings.y0 : slot.y0) - 8.0f};
    const Rect art{c.x0, area.y0, c.x1, area.y1 - kChipH - kChipGap};
    // The columns keep room for the widest chip; the drawing takes what is left.
    const float col_w = kChipPadL + kGlyph + kChipGap +
                        kit.TextWidthSized(kChipText, true, kWidestChip) + kChipPadR + 1.0f;
    const DsDrawing ds = PlaceDs({c.x0 + col_w + kLeader, art.y0, c.x1 - col_w - kLeader, art.y1});
    const int lit = listening ? view.remap.listening : -1;
    DrawDs(kit, ds, lit);

    const bool by_position = model.face_buttons == 0;
    struct Chip {
        int target;
        Rect r;
        DsPoint from; ///< the leader's end on the chip
    };
    std::vector<Chip> chips;
    const auto text_of = [&](int t) {
        if (Remap::IsExtra(t)) { // AzaharFrame padsui: the mouse's Touch Screen, Open Menu
            const std::string name =
                SourceName(dev.names, dev.extras[static_cast<std::size_t>(t - DsButton::Touch)]);
            // Chips of their own, out of the columns: they say what they are.
            return std::string(t == DsButton::OpenMenu ? "Open Menu: " : "Touch Screen: ") +
                   (name.empty() ? std::string("None") : name);
        }
        if (IsStick(t)) {
            return StickRouteName(dev.sticks[t == kDsCirclePad ? 0 : 1]);
        }
        return SourceName(dev.names, dev.mapping[static_cast<std::size_t>(t)], by_position);
    };
    const auto chip_w = [&](int t, float max_w) {
        std::string text = text_of(t);
        if (text.empty()) {
            text = "None";
        }
        const float w = kChipPadL + kGlyph + kChipGap + kit.TextWidthSized(kChipText, true, text) + kChipPadR;
        return std::min(w, max_w);
    };
    if (buttons) {
        const auto column = [&](const auto& list, bool left) {
            const float x_in = left ? ds.box.x0 - kLeader : ds.box.x1 + kLeader;
            const float max_w = left ? x_in - c.x0 : c.x1 - x_in;
            const int n = static_cast<int>(list.size());
            const float pitch = n > 1 ? (art.H() - kChipH) / static_cast<float>(n - 1) : 0.0f;
            for (int i = 0; i < n; i++) {
                const int t = list[static_cast<std::size_t>(i)];
                const float w = chip_w(t, max_w);
                const float y = art.y0 + pitch * static_cast<float>(i);
                const Rect r = left ? Rect{x_in - w, y, x_in, y + kChipH} : Rect{x_in, y, x_in + w, y + kChipH};
                chips.push_back({t, r, {left ? r.x1 : r.x0, r.Cy()}});
            }
        };
        const bool sticks = HasSticks(dev);
        if (sticks) {
            column(kLeftChips, true);
            column(kRightChips, false);
        } else {
            column(kMouseLeftChips, true);
            column(kMouseRightChips, false);
        }
        // AzaharFrame padsui: a filter chip (Recentre at Edge) at `x` in the row `row`.
        const auto filter_chip = [&](float x, const Rect& row, Id id, bool on) {
            const std::string& label = LabelFor(id);
            constexpr float kCheckW = 18.0f + 8.0f;
            const float w = (on ? 8.0f + kCheckW : 16.0f) + kit.TextWidth(Type::Label, label) + 16.0f;
            const Rect r{x, row.y0, x + w, row.y1};
            const Target tg{id};
            if (on) {
                kit.Fill(r, kChipRadius, Tok::kSecondaryContainer);
            } else {
                kit.Fill(r, kChipRadius, Tok::kSurface);
                kit.Stroke(r, kChipRadius, 1.0f, Tok::kOutline);
            }
            const uint32_t fg = on ? Tok::kOnSecondaryContainer : Tok::kOnSurfaceVariant;
            kit.StateLayer(r, kChipRadius, fg, StateOf(view, tg));
            float tx = r.x0 + 16.0f;
            if (on) {
                kit.Icon(r.x0 + 8.0f + 9.0f, r.Cy(), 18.0f, Sym::kCheck, false, fg);
                tx = r.x0 + 8.0f + kCheckW;
            }
            kit.TextMid(tx, r.Cy(), Type::Label, fg, label);
            AddHit({r.x0, r.y0 - kChipGap * 0.5f, r.x1, r.y1 + kChipGap * 0.5f}, tg, pane);
            return r.x1 + kChipGap;
        };
        const Rect under{c.x0, area.y1 - kChipH, c.x1, area.y1}; // the row under the drawing
        float row_x = c.x0;
        if (mouse_page) {
            // The settings row: Open Menu (a chip like the others, editable), Recentre at Edge
            // (Bottom Screen mode), Cursor Speed at the end.
            const float w = chip_w(DsButton::OpenMenu, c.W());
            const Rect om{settings.x0, settings.y0, settings.x0 + w, settings.y1};
            chips.push_back({DsButton::OpenMenu, om, {om.Cx(), om.y0}});
            const std::string& label = LabelFor(Id::CursorSpeed);
            const float lw = kit.TextWidth(Type::Body, label);
            float sx = om.x1 + 16.0f;
            if (HasRecentre(dev)) {
                sx = filter_chip(sx, settings, Id::RecentreAtEdge, dev.recentre) + 8.0f;
            }
            const float slider_x0 = std::max(sx + lw + 16.0f, settings.x1 - 180.0f);
            kit.TextMid(slider_x0 - 12.0f, settings.Cy(), Type::Body, Tok::kOnSurface, label, 2);
            const Rect sl{slider_x0, settings.Cy() - 24.0f, settings.x1, settings.Cy() + 24.0f};
            const SliderRange range = SliderFor(Id::CursorSpeed);
            float t = static_cast<float>(dev.speed) / range.max;
            if (view.dragging == Id::CursorSpeed) {
                t = std::round(view.drag_t * range.max) / range.max;
            }
            kit.Slider(sl, t, {},
                       view.dragging == Id::CursorSpeed ? State::Pressed
                                                        : StateOf(view, {Id::CursorSpeed}),
                       static_cast<int>(range.max), 0.0f);
            layout.sliders.push_back({Id::CursorSpeed, sl.x0, sl.x1});
            AddHit({sl.x0 - 8.0f, settings.y0 - kChipGap * 0.5f, sl.x1, settings.y1 + kChipGap * 0.5f},
                   {Id::CursorSpeed}, pane);
            // Touch Screen leads up into the bottom screen from the row under the drawing.
            const float tw = chip_w(DsButton::Touch, c.W());
            const Rect ts{under.x0, under.y0, under.x0 + tw, under.y1};
            chips.push_back({DsButton::Touch, ts, {ts.x1, ts.Cy()}});
            row_x = ts.x1 + kChipGap;
        } else if (HasRecentre(dev)) {
            row_x = filter_chip(row_x, under, Id::RecentreAtEdge, dev.recentre);
        }
        // The row under the drawing: Select's and Start's chips either side of their buttons'
        // line (Select's leader then stays left of Start's), Home's under Home or, when that
        // would meet Select's, just left of it; all three moved right when the chips before them
        // need the room.
        {
            const float sel_w = chip_w(DsButton::Select, c.W());
            const float start_w = chip_w(DsButton::Start, c.W());
            const float home_w = chip_w(DsButton::Home, c.W());
            float mid = ds.Anchor(DsButton::Start).x;
            mid = std::min(mid, c.x1 - start_w - kChipGap * 0.5f);
            Rect start{mid + kChipGap * 0.5f, under.y0, mid + kChipGap * 0.5f + start_w, under.y1};
            Rect sel{start.x0 - kChipGap - sel_w, start.y0, start.x0 - kChipGap, start.y1};
            float hx = std::min(ds.Anchor(DsButton::Home).x - home_w * 0.5f, sel.x0 - kChipGap - home_w);
            if (hx < row_x) {
                const float dx = std::min(row_x - hx, c.x1 - start.x1);
                hx += dx;
                start.x0 += dx;
                start.x1 += dx;
                sel.x0 += dx;
                sel.x1 += dx;
            }
            const Rect home{hx, start.y0, hx + home_w, start.y1};
            chips.push_back({DsButton::Home, home, {home.Cx(), home.y0}});
            chips.push_back({DsButton::Select, sel, {sel.Cx(), sel.y0}});
            chips.push_back({DsButton::Start, start, {start.Cx(), start.y0}});
        }
        // The mouse's Touch Screen leads into the bottom screen near its bottom left corner;
        // Open Menu has no leader.
        const Rect bs = ds.BottomScreen();
        const DsPoint touch_at{bs.x0 + 6.0f, bs.y0 + bs.H() * 0.8f};
        for (const Chip& ch : chips) {
            if (ch.target == DsButton::OpenMenu) {
                continue;
            }
            const DsPoint to = ch.target == DsButton::Touch ? touch_at : ds.Anchor(ch.target);
            kit.Line(ch.from.x, ch.from.y, to.x, to.y, 1.5f, Tok::kOutline);
        }
        for (const Chip& ch : chips) {
            if (ch.target == DsButton::Touch) {
                const bool on = ch.target == lit;
                if (on) {
                    kit.Dot(touch_at.x, touch_at.y, 7.0f, Tok::kPrimary, 0.3f);
                }
                kit.Dot(touch_at.x, touch_at.y, on ? 4.5f : 3.5f, Tok::kPrimary);
            } else if (ch.target != DsButton::OpenMenu) {
                DrawDsDot(kit, ds, ch.target, ch.target == lit);
            }
        }
    }
    for (const Chip& ch : chips) {
        const int t = ch.target;
        const Rect& r = ch.r;
        const bool on = t == lit;
        const Target tg{Id::RemapChip, t};
        std::string text = text_of(t);
        const bool none = text.empty();
        if (none) {
            text = "None";
        }
        uint32_t ink = none ? Tok::kOnSurfaceVariant : Tok::kOnSurface;
        uint32_t glyph_fg = Tok::kPrimary, glyph_bg = Tok::kSecondaryContainer;
        kit.Fill(r, kChipRadius, Tok::kSurface);
        if (on) {
            kit.Fill(r, kChipRadius, Tok::kPrimaryContainer);
            kit.Stroke(r, kChipRadius, 2.0f, Tok::kPrimary);
            ink = Tok::kOnPrimaryContainer;
            glyph_fg = Tok::kOnPrimary;
            glyph_bg = Tok::kPrimary;
        } else {
            kit.Stroke(r, kChipRadius, 1.0f, Tok::kOutline);
        }
        kit.StateLayer(r, kChipRadius, on ? Tok::kOnPrimaryContainer : Tok::kOnSurfaceVariant,
                       StateOf(view, tg));
        const Rect gb{r.x0 + kChipPadL, r.Cy() - kGlyph * 0.5f, r.x0 + kChipPadL + kGlyph,
                      r.Cy() + kGlyph * 0.5f};
        DrawDsGlyph(kit, gb, t, glyph_fg, glyph_bg);
        const float tx = gb.x1 + kChipGap;
        kit.TextSized(tx, r.Cy() + kit.CapHeightSized(kChipText, true) * 0.5f, kChipText, true, ink,
                      kit.Fit(Type::Label, text, r.x1 - kChipPadR - tx + 1.0f));
        // The target: the chip and half the gap over and under it (48 dp, Material's minimum).
        AddHit({r.x0, r.y0 - kChipGap * 0.5f, r.x1, r.y1 + kChipGap * 0.5f}, tg, pane);
    }

    // The Pointer Test: where the touchpad or the mouse touches the bottom screen now.
    const PointerTest& pt = model.devices.pointer;
    if (HasPointer(dev) && pt.shown) {
        const Rect bs = ds.BottomScreen();
        const float x = bs.x0 + std::clamp(pt.x, 0.0f, 1.0f) * bs.W();
        const float y = bs.y0 + std::clamp(pt.y, 0.0f, 1.0f) * bs.H();
        const float op = 1.0f; // AzaharFrame menuux: the Touchpad row has no Off now
        constexpr float kRing = 10.0f;
        kit.Clip(bs);
        if (pt.down) {
            kit.Dot(x, y, kRing, Tok::kPrimary, 0.3f * op);
            kit.Dot(x, y, 4.5f, Tok::kPrimary, op);
        }
        kit.Stroke({x - kRing, y - kRing, x + kRing, y + kRing}, kRing, 2.0f, Tok::kPrimary, op);
        kit.ClearClip();
    }

    // ---- Bottom: the devices and Reset to Default, or the snackbar ----
    if (listening) {
        // Material snackbar: inverse surface, 4 dp corners, the message, the action in inverse
        // primary (a text button) at its end.
        std::string prompt = !IsStick(view.remap.listening) ? "Press a Button\xE2\x80\xA6"
                             : view.remap.step == 0       ? "Push the Stick Up\xE2\x80\xA6"
                                                          : "Push the Stick Right\xE2\x80\xA6";
        if (view.remap.countdown > 0) {
            // AzaharFrame input2: 3, 2, 1, then it closes and the binding stays as it was.
            prompt += " " + std::to_string(view.remap.countdown);
        }
        const std::string& action = LabelFor(Id::RemapCancel);
        const float text = 16.0f;
        const float tw = kit.TextWidthSized(text, false, prompt);
        const float aw = kit.TextWidth(Type::Label, action) + 24.0f;
        const float w = std::max(344.0f, 16.0f + tw + 24.0f + aw + 8.0f);
        const Rect bar{c.Cx() - w * 0.5f, slot.y0, c.Cx() + w * 0.5f, slot.y1};
        kit.Fill(bar, 4.0f, Tok::kInverseSurface);
        kit.TextSized(bar.x0 + 16.0f, bar.Cy() + kit.CapHeightSized(text, false) * 0.5f, text, false,
                      Tok::kInverseOnSurface, prompt);
        const Rect cancel{bar.x1 - 8.0f - aw, bar.y0 + 4.0f, bar.x1 - 8.0f, bar.y1 - 4.0f};
        kit.StateLayer(cancel, cancel.H() * 0.5f, Tok::kInversePrimary, StateOf(view, {Id::RemapCancel}));
        kit.TextMid(cancel.Cx(), cancel.Cy(), Type::Label, Tok::kInversePrimary, action, 1);
        AddHit({cancel.x0, bar.y0, cancel.x1, bar.y1}, {Id::RemapCancel}, pane);
        return;
    }
    // AzaharFrame padsui: Use This Device at the end of the row, for every device but the Frame
    // Controllers (always used).
    float right = slot.x1;
    if (sel && dev.kind != DeviceKind::Frame) {
        const std::string& label = LabelFor(Id::UseDevice);
        const float w = kit.TextWidth(Type::Label, label) + 12.0f + 52.0f;
        const Rect sw{slot.x1 - w, slot.y0, slot.x1, slot.y1};
        const State st = StateOf(view, {Id::UseDevice});
        kit.StateLayer({sw.x0 - 12.0f, sw.y0, sw.x1 + 8.0f, sw.y1}, 16.0f, Tok::kOnSurface, st);
        kit.TextMid(sw.x0, sw.Cy(), Type::Label, Tok::kOnSurface, label);
        kit.Switch(sw, dev.enabled, st);
        AddHit({sw.x0 - 12.0f, sw.y0, sw.x1, sw.y1}, {Id::UseDevice}, pane);
        right = sw.x0 - 12.0f - 16.0f;
    }
    // Material filter chips: 8 dp corners, an outline, the chosen one in the secondary container
    // with a check. Each has a dot when its device is connected (hollow while it is not used).
    // Too many for the row: each gets an equal share and its name is shortened.
    const int n = static_cast<int>(model.devices.list.size());
    constexpr float kCheckW = 18.0f + 8.0f;
    constexpr float kDotW = 8.0f + 8.0f; ///< the status dot and its gap
    std::vector<float> widths;
    float total = 0.0f;
    for (int i = 0; i < n; i++) {
        const EditDevice& d = model.devices.list[static_cast<std::size_t>(i)];
        const bool chosen = i == model.devices.selected;
        const float w = (chosen ? 8.0f + kCheckW : 16.0f) + (d.connected ? kDotW : 0.0f) +
                        kit.TextWidth(Type::Label, d.name) + 16.0f;
        widths.push_back(w);
        total += w + (i > 0 ? kChipGap : 0.0f);
    }
    const float avail = right - slot.x0;
    const float share = n > 0 ? (avail - kChipGap * static_cast<float>(n - 1)) / static_cast<float>(n) : 0.0f;
    float x = slot.x0;
    for (int i = 0; i < n; i++) {
        const EditDevice& d = model.devices.list[static_cast<std::size_t>(i)];
        const bool chosen = i == model.devices.selected;
        const float w = total > avail ? std::min(widths[static_cast<std::size_t>(i)], share)
                                      : widths[static_cast<std::size_t>(i)];
        const Rect r{x, slot.Cy() - kChipH * 0.5f, x + w, slot.Cy() + kChipH * 0.5f};
        const Target tg{Id::EditDevice, i};
        if (chosen) {
            kit.Fill(r, kChipRadius, Tok::kSecondaryContainer);
        } else {
            kit.Stroke(r, kChipRadius, 1.0f, Tok::kOutline);
        }
        const uint32_t fg = chosen ? Tok::kOnSecondaryContainer : Tok::kOnSurfaceVariant;
        kit.StateLayer(r, kChipRadius, fg, StateOf(view, tg));
        float tx = r.x0 + 16.0f;
        if (chosen) {
            kit.Icon(r.x0 + 8.0f + 9.0f, r.Cy(), 18.0f, Sym::kCheck, false, fg);
            tx = r.x0 + 8.0f + kCheckW;
        }
        if (d.connected) {
            const float dx = tx + 4.0f;
            if (d.enabled) {
                kit.Dot(dx, r.Cy(), 4.0f, Tok::kPrimary);
            } else {
                kit.Stroke({dx - 4.0f, r.Cy() - 4.0f, dx + 4.0f, r.Cy() + 4.0f}, 4.0f, 1.5f, fg);
            }
            tx += kDotW;
        }
        kit.TextMid(tx, r.Cy(), Type::Label, fg,
                    kit.Fit(Type::Label, d.name, r.x1 - 16.0f - tx + 1.0f));
        AddHit({r.x0, slot.y0, r.x1, slot.y1}, tg, pane);
        x += w + kChipGap;
    }
}

// AzaharFrame reticle: the scope control's measures (dp).
namespace {
constexpr float kScopeMoreW = 32.0f;    ///< the ⋮ button
constexpr float kScopeGapMore = 2.0f;   ///< between the toggle and ⋮
constexpr float kScopeGapX = 6.0f;      ///< between the tab's own controls and the toggle
constexpr float kScopeRowsGap = 16.0f;  ///< under the toggle's line, before the rows
/// Controls: Edit Controls' top under the toggle, clear of Per-Game's count tooltip (34 dp).
constexpr float kScopeEditGap = 40.0f;

/// The room a row's label takes.
float LabelRoom(const Kit& kit, const MenuModel& model, Id id) {
    (void)model;
    return kit.TextWidth(Type::Body, LabelFor(id));
}
} // namespace

void TabMenu::DrawControls(Kit& kit, const MenuModel& model, const MenuView& view,
                           const Rect& pane) {
    if (view.remap.editing) {
        DrawEditControls(kit, model, view, pane);
        return;
    }
    const Rect c{pane.x0 + kPad, pane.y0 + kPad, pane.x1 - kPad, pane.y1 - kPad};
    // AzaharFrame reticle: the scope control at the top right, as on Settings and Position.
    const float scope_x0 = DrawScope(kit, model, view, c, pane);
    const auto& pages = OptionsFor(Id::ControlsPage);
    const float w = kit.SegmentedWidth(pages, scope_x0 - kScopeGapX - c.x0);
    // AzaharFrame menu: the pages on the left. AzaharFrame reticle: Edit Controls at the top of
    // a column on the right, under the scope control, beside the controller picture.
    const Rect seg{c.x0, c.y0, c.x0 + w, c.y0 + kControlH};
    const int hover = view.hover.id == Id::ControlsPage ? view.hover.index : -1;
    const int pressed = view.pressed.id == Id::ControlsPage ? view.pressed.index : -1;
    kit.Segmented(seg, pages, model.controls_page, hover, pressed);
    const auto pe = kit.SegmentEdges(seg, pages);
    for (int i = 0; i < static_cast<int>(pages.size()); i++) {
        AddHit({pe[static_cast<std::size_t>(i)], seg.y0, pe[static_cast<std::size_t>(i + 1)], seg.y1},
               {Id::ControlsPage, i}, pane);
    }
    const float edit_w =
        std::min(200.0f, kit.TextWidth(Type::Label, LabelFor(Id::EditControls)) + 82.0f);
    const Rect edit{c.x1 - edit_w, seg.y1 + kScopeEditGap, c.x1,
                    seg.y1 + kScopeEditGap + kControlH};
    kit.Button(edit, ButtonStyle::Tonal, Sym::kEdit, LabelFor(Id::EditControls),
               StateOf(view, {Id::EditControls}));
    AddHit(edit, {Id::EditControls}, pane);
    // Touchpad: a label and its two choices; AzaharFrame menuux: under them one line saying what
    // the chosen one does (the only row with a supporting line: the owner asked for it).
    // AzaharFrame mouse: Bottom Screen Reticle under that line, the same way.
    // AzaharFrame ui2: the controller picture at the top, under the pages; these rows at the
    // bottom of the pane, under the picture (owner's report 14).
    // AzaharFrame reticle: in Global, a row the game has its own value for is greyed (its
    // tooltip says why), as on the Settings tab.
    const auto choice_row = [&](Id id, float y0, int value) {
        const Rect row{c.x0 - 12.0f, y0, c.x1 + 12.0f, y0 + kControlH + 8.0f};
        const Lock lock = model.locked[static_cast<std::size_t>(id)];
        const bool enabled = lock == Lock::None;
        AddHit(row, {id, kLabelIndex}, pane);
        anchors.push_back({id, c.x0, row.y0 + 6.0f, lock});
        kit.TextMid(c.x0, row.Cy(), Type::Body, Tok::kOnSurface, LabelFor(id), 0,
                    enabled ? 1.0f : kDisabledContent);
        const auto& opts = OptionsFor(id);
        // AzaharFrame input2: up to 0.64 of the width (the reticle's four choices need it).
        const float tw = kit.SegmentedWidth(opts, c.W() * 0.64f);
        const Rect ts{c.x1 - tw, row.Cy() - kControlH * 0.5f, c.x1, row.Cy() + kControlH * 0.5f};
        const int th = view.hover.id == id ? view.hover.index : -1;
        const int tpr = view.pressed.id == id ? view.pressed.index : -1;
        kit.Segmented(ts, opts, value, th, tpr, !enabled);
        if (enabled) {
            const auto e = kit.SegmentEdges(ts, opts);
            for (int i = 0; i < static_cast<int>(opts.size()); i++) {
                AddHit({e[static_cast<std::size_t>(i)], ts.y0, e[static_cast<std::size_t>(i + 1)], ts.y1},
                       {id, i}, pane);
                if (HasOptionTips(id)) { // AzaharFrame xfergpu: a choice's own tooltip
                    anchors.push_back({id, c.x0, row.y0 + 6.0f, lock, i,
                                       (e[static_cast<std::size_t>(i)] +
                                        e[static_cast<std::size_t>(i + 1)]) *
                                           0.5f,
                                       ExplainOption(id, i)});
                }
            }
        }
        return row;
    };
    // AzaharFrame reticle: Reticle Speed, a slider row as on the Settings tab (its value at the
    // right, "2.0x"), as wide as the reticle's choices.
    const auto slider_row = [&](Id id, float y0, int value) {
        const Rect row{c.x0 - 12.0f, y0, c.x1 + 12.0f, y0 + kControlH + 8.0f};
        const Lock lock = model.locked[static_cast<std::size_t>(id)];
        const bool enabled = lock == Lock::None;
        AddHit(row, {id, kLabelIndex}, pane);
        anchors.push_back({id, c.x0, row.y0 + 6.0f, lock});
        kit.TextMid(c.x0, row.Cy(), Type::Body, Tok::kOnSurface, LabelFor(id), 0,
                    enabled ? 1.0f : kDisabledContent);
        const SliderRange range = SliderFor(id);
        float v = std::clamp(static_cast<float>(value), range.min, range.max);
        if (view.dragging == id) {
            v = range.min + std::round(view.drag_t * (range.max - range.min) / range.step) *
                                range.step;
        }
        const float tw = kit.SegmentedWidth(OptionsFor(Id::BottomScreenReticle), c.W() * 0.64f);
        const Rect sl{c.x1 - tw, row.y0, c.x1, row.y1};
        kit.Slider(sl, (v - range.min) / (range.max - range.min), SliderText(id, v),
                   view.dragging == id ? State::Pressed : StateOf(view, {id}, enabled));
        layout.sliders.push_back({id, sl.x0, sl.x1 - 80.0f});
        if (enabled) {
            AddHit(sl, {id}, pane);
        }
        return row;
    };
    constexpr float kChoiceH = kControlH + 8.0f;
    constexpr float kSupportH = 16.0f; ///< the Touchpad's supporting line and its gaps
    // At Edge and Reticle Speed only while the reticle is on.
    const bool reticle_on = model.bottom_screen_reticle != 0;
    const float rows_h = kChoiceH + kSupportH + kChoiceH + (reticle_on ? 2.0f * kChoiceH : 0.0f);
    const float rows_y0 = c.y1 - rows_h;
    const Rect area{pane.x0 + 8.0f, seg.y1 + 8.0f, edit.x0 - 12.0f, rows_y0 - 4.0f};
    if (diagram) {
        diagram(kit, area, model.controls_page);
    }
    const Rect tp_row = choice_row(Id::TouchpadTouch, rows_y0, model.touchpad_touch);
    const float support_y = tp_row.y1 + 6.0f;
    const bool tp_enabled = model.locked[static_cast<std::size_t>(Id::TouchpadTouch)] == Lock::None;
    kit.TextMid(c.x0, support_y, Type::Tooltip, Tok::kOnSurfaceVariant,
                kit.Fit(Type::Tooltip, TouchpadSupporting(model.touchpad_touch), c.W()), 0,
                tp_enabled ? 1.0f : kDisabledContent);
    const Rect reticle_row =
        choice_row(Id::BottomScreenReticle, support_y + 10.0f, model.bottom_screen_reticle);
    if (reticle_on) {
        const Rect speed_row = slider_row(Id::ReticleSpeed, reticle_row.y1, model.reticle_speed);
        choice_row(Id::ReticleAtEdge, speed_row.y1, model.reticle_at_edge);
    }
    DrawScopeMenu(kit, model, view, pane);
}

// ---- AzaharFrame reticle: the scope control (tab_menu.h) -------------------------------------
// One place draws it for the Settings, Controls and Position tabs: the same size and spot (the
// top right of the pane's content), so a later restyle changes all three. It replaced the
// Settings tab's scope menu button and the per-row "This Game" chips (owner's design, round 24).

float TabMenu::DrawScope(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& c,
                         const Rect& pane) {
    static const std::vector<std::string> kGlobalOnly{"Global"};
    const auto& both = OptionsFor(Id::Scope);
    const bool per_game_ok = model.locked[static_cast<std::size_t>(Id::Scope)] == Lock::None;
    const auto& opts = per_game_ok ? both : kGlobalOnly;
    // As wide with or without a game, so nothing beside it moves.
    const float seg_w = kit.SegmentedWidth(both);
    const Rect more{c.x1 - kScopeMoreW, c.y0, c.x1, c.y0 + kControlH};
    const Rect seg{more.x0 - kScopeGapMore - seg_w, c.y0, more.x0 - kScopeGapMore,
                   c.y0 + kControlH};
    const int hover = view.hover.id == Id::Scope ? view.hover.index : -1;
    const int pressed = view.pressed.id == Id::Scope ? view.pressed.index : -1;
    kit.Segmented(seg, opts, per_game_ok ? std::clamp(model.scope, 0, 1) : 0, hover, pressed);
    if (per_game_ok) {
        const auto e = kit.SegmentEdges(seg, opts);
        for (int i = 0; i < static_cast<int>(opts.size()); i++) {
            AddHit({e[static_cast<std::size_t>(i)], seg.y0, e[static_cast<std::size_t>(i + 1)],
                    seg.y1},
                   {Id::Scope, i}, pane);
        }
    } else {
        // No game that can have its own settings: Global alone, and its tooltip says why.
        AddHit(seg, {Id::Scope, kLabelIndex}, pane);
        anchors.push_back({Id::Scope, seg.x0, seg.y0 + 6.0f,
                           model.locked[static_cast<std::size_t>(Id::Scope)], -1, seg.Cx()});
    }
    // ⋮: Use Global Values for All, while this tab (the Settings tab: this section) has any.
    const std::vector<Id> rows = GameValueRows(model, view.tab, view.section);
    const bool can_clear = per_game_ok && !rows.empty();
    const Target more_t{Id::ScopeMore};
    const State ms = view.open_menu == Id::ScopeMore ? State::Pressed
                                                     : StateOf(view, more_t, can_clear);
    kit.StateLayer({more.Cx() - 16.0f, more.Cy() - 20.0f, more.Cx() + 16.0f, more.Cy() + 20.0f},
                   16.0f, Tok::kOnSurfaceVariant, ms);
    for (int i = -1; i <= 1; i++) {
        kit.Dot(more.Cx(), more.Cy() + 6.0f * static_cast<float>(i), 2.0f,
                Tok::kOnSurfaceVariant, can_clear ? 1.0f : kDisabledContent);
    }
    if (can_clear) {
        AddHit(more, more_t, pane);
    } else {
        AddHit(more, {Id::ScopeMore, kLabelIndex}, pane);
        anchors.push_back({Id::ScopeMore, more.x0, more.y0 + 6.0f,
                           per_game_ok ? Lock::NoGameValues : Lock::NoGame, -1, more.Cx()});
    }
    scope_more_ = more;
    // Pointing at Per-Game while it is picked: how many values on the tab are this game's own,
    // in a tooltip under the toggle (owner, round 24: no line of its own; not over the section
    // buttons beside it).
    if (per_game_ok && model.scope == 1) {
        const auto e = kit.SegmentEdges(seg, opts);
        anchors.push_back({Id::Scope, e[1], seg.y1, Lock::None, 1, (e[1] + e[2]) * 0.5f,
                           GameValueCount(rows.size()), true});
    }
    return seg.x0;
}

void TabMenu::DrawScopeMenu(Kit& kit, const MenuModel& model, const MenuView& view,
                            const Rect& pane) {
    (void)model;
    if (view.open_menu != Id::ScopeMore || scope_more_.W() <= 0.0f) {
        return;
    }
    const std::vector<std::string> opts{LabelFor(Id::ScopeMore)};
    const float w = kit.TextWidth(Type::Label, opts[0]) + 72.0f;
    const float item = Kit::MenuItemHeight();
    const float y0 = scope_more_.y1 + 4.0f;
    const Rect box = kit.Menu({scope_more_.x1 - w, y0, scope_more_.x1,
                               y0 + item + 16.0f},
                              opts, -1, view.hover.id == Id::ScopeMore ? view.hover.index : -1);
    (void)pane;
    const Rect everything{0, 0, kPanelW, kPanelH};
    AddHit(box, {Id::None, kMenuAreaIndex}, everything); // the menu's own area eats clicks
    AddHit({box.x0, box.y0 + 8.0f, box.x1, box.y0 + 8.0f + item}, {Id::ScopeMore, 0}, everything);
}

void TabMenu::DrawRows(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane) {
    const Rect c{pane.x0 + kPad, pane.y0 + kPad, pane.x1 - kPad, pane.y1 - kPad};
    float top = c.y0;
    if (TabHasScope(view.tab)) { // AzaharFrame reticle: Settings and Position
        const float scope_x0 = DrawScope(kit, model, view, c, pane);
        const float room = scope_x0 - kScopeGapX - c.x0;
        if (view.tab == Tab::Position) {
            // The two actions of CitraVR's Position tab (Reset) and of the right stick (Swap),
            // left of the scope control.
            const float bw = (room - 12.0f - 10.0f) * 0.5f; // filled: a wider gap to the toggle
            const Rect a{c.x0, top, c.x0 + bw, top + kControlH};
            const Rect b{a.x1 + 12.0f, top, a.x1 + 12.0f + bw, top + kControlH};
            kit.Button(a, ButtonStyle::Tonal, Sym::kRecenter, LabelFor(Id::ResetPosition),
                       StateOf(view, {Id::ResetPosition}));
            kit.Button(b, ButtonStyle::Tonal, Sym::kSwapVert, LabelFor(Id::SwapScreens),
                       StateOf(view, {Id::SwapScreens}));
            AddHit(a, {Id::ResetPosition}, pane);
            AddHit(b, {Id::SwapScreens}, pane);
        } else {
            // AzaharFrame autosave: the section (General, Graphics, Immersive).
            const auto& sections = OptionsFor(Id::SettingsSection);
            const Rect seg{c.x0, top, c.x0 + kit.SegmentedWidth(sections, room),
                           top + kControlH};
            const int hover = view.hover.id == Id::SettingsSection ? view.hover.index : -1;
            const int pressed = view.pressed.id == Id::SettingsSection ? view.pressed.index : -1;
            kit.Segmented(seg, sections, static_cast<int>(view.section), hover, pressed);
            const auto se = kit.SegmentEdges(seg, sections);
            for (int i = 0; i < static_cast<int>(sections.size()); i++) {
                AddHit({se[static_cast<std::size_t>(i)], seg.y0,
                        se[static_cast<std::size_t>(i + 1)], seg.y1},
                       {Id::SettingsSection, i}, pane);
            }
        }
        top += kControlH + kScopeRowsGap;
    }

    const std::vector<Row> rows = RowsFor(view.tab, model, view.section);
    // The labels' column: as wide as the widest label of a row whose control starts at it (a
    // slider, a menu button, the offset). AzaharFrame menuux: a segmented button or a switch
    // starts after its own label, so a long label elsewhere does not squeeze it.
    float label_w = 0.0f;
    for (const Row& r : rows) {
        if (r.kind == RowKind::Slider || r.kind == RowKind::MenuButton ||
            r.kind == RowKind::Offset) {
            label_w = std::max(label_w, LabelRoom(kit, model, r.id)); // AzaharFrame useglobal
        }
    }
    const float control_x0 = c.x0 + label_w + kLabelGap;
    const Rect list{pane.x0, top, pane.x1, pane.y1 - 8.0f};
    const float content_h = kRowH * static_cast<float>(rows.size());
    // Whole rows: the list scrolls in row steps, so no row shows cut at the top.
    const float max_scroll =
        std::max(0.0f, std::ceil((content_h - (list.y1 - list.y0)) / kRowH) * kRowH);
    layout.max_scroll = max_scroll;
    const float scroll = std::clamp(view.scroll[static_cast<int>(view.tab)], 0.0f, max_scroll);
    kit.Clip(list);
    Rect open_anchor{};
    int open_selected = -1;
    float y = top - scroll;
    for (const Row& r : rows) {
        const Rect row{c.x0 - 12.0f, y + 4.0f, c.x1 + 12.0f, y + kRowH - 4.0f};
        const float cy = row.Cy();
        const Target whole{r.id};
        const bool enabled = r.Enabled();
        if (r.kind == RowKind::Header) {
            // AzaharFrame autosave: a divider and the group's name in the primary colour, at the
            // bottom of the row so it reads with the rows under it.
            kit.Line(c.x0, y + 20.0f, c.x1, y + 20.0f, 1.0f, Tok::kOutlineVariant);
            kit.TextSized(c.x0, y + 58.0f, 14.0f, true, Tok::kPrimary, LabelFor(r.id));
            y += kRowH;
            continue;
        }
        if (r.kind == RowKind::Button) {
            // AzaharFrame menuux: a full-width tonal button (Switch to Flat), no label.
            const Rect b{c.x0, cy - 28.0f, c.x1, cy + 28.0f};
            kit.Button(b, ButtonStyle::Tonal, Sym::kTv, LabelFor(r.id),
                       StateOf(view, whole, enabled), Type::Title);
            if (enabled) {
                AddHit(b, whole, list);
            }
            y += kRowH;
            continue;
        }
        const float own_x0 = c.x0 + LabelRoom(kit, model, r.id) + kLabelGap; // useglobal
        const bool row_hit = r.kind == RowKind::Switch || r.kind == RowKind::Link;
        if (row.y1 > list.y0 && row.y0 < list.y1) {
            anchors.push_back({r.id, c.x0, std::max(row.y0, list.y0) + 6.0f, r.lock});
        }
        if (row_hit && enabled) {
            kit.StateLayer(row, 16.0f, Tok::kOnSurface, StateOf(view, whole));
            AddHit(row, whole, list);
        } else {
            // The row itself (its label, and a disabled control): only the tooltip.
            AddHit(row, {r.id, kLabelIndex}, list);
        }
        kit.TextMid(c.x0, cy, Type::Body, Tok::kOnSurface, LabelFor(r.id), 0,
                    enabled ? 1.0f : kDisabledContent);
        const Rect ctl{control_x0, cy - kControlH * 0.5f, c.x1, cy + kControlH * 0.5f};
        switch (r.kind) {
        case RowKind::Switch:
            kit.Switch(ctl, r.on, StateOf(view, whole, enabled));
            break;
        case RowKind::Segmented: {
            const auto& opts = OptionsFor(r.id);
            const float w = kit.SegmentedWidth(opts, ctl.x1 - own_x0);
            const Rect seg{ctl.x1 - w, ctl.y0, ctl.x1, ctl.y1};
            const int hover = view.hover.id == r.id ? view.hover.index : -1;
            const int pressed = view.pressed.id == r.id ? view.pressed.index : -1;
            kit.Segmented(seg, opts, r.selected, hover, pressed, !enabled);
            if (enabled) {
                const auto e = kit.SegmentEdges(seg, opts);
                for (int i = 0; i < static_cast<int>(opts.size()); i++) {
                    AddHit({e[static_cast<std::size_t>(i)], seg.y0, e[static_cast<std::size_t>(i + 1)],
                            seg.y1},
                           {r.id, i}, list);
                    // AzaharFrame xfergpu: a choice's own tooltip, over its segment
                    if (HasOptionTips(r.id) && row.y1 > list.y0 && row.y0 < list.y1) {
                        anchors.push_back({r.id, c.x0, std::max(row.y0, list.y0) + 6.0f, r.lock,
                                           i,
                                           (e[static_cast<std::size_t>(i)] +
                                            e[static_cast<std::size_t>(i + 1)]) *
                                               0.5f,
                                           ExplainOption(r.id, i)});
                    }
                }
            }
            break;
        }
        case RowKind::Slider: {
            const Rect sl{std::max(ctl.x0, ctl.x1 - 420.0f), row.y0, ctl.x1, row.y1};
            float t = r.t;
            std::string value = r.value;
            if (view.dragging == r.id) {
                // Held: where the beam is, at the slider's steps.
                const SliderRange range = SliderFor(r.id);
                const float v = range.min + std::round(view.drag_t * (range.max - range.min) /
                                                       range.step) * range.step;
                t = (v - range.min) / (range.max - range.min);
                value = SliderText(r.id, v);
            }
            const Target held{r.id};
            kit.Slider(sl, t, value,
                       view.dragging == r.id ? State::Pressed : StateOf(view, held, enabled),
                       r.stops);
            layout.sliders.push_back({r.id, sl.x0, sl.x1 - 80.0f});
            if (enabled) {
                AddHit(sl, whole, list);
            }
            if (r.id == Id::AutosaveEvery) {
                // AzaharFrame autosave: the stops' minutes under the track (they are uneven).
                const float x0 = sl.x0 + 2.0f, x1 = sl.x1 - 80.0f - 2.0f;
                for (std::size_t i = 0; i < kAutosaveEvery.size(); i++) {
                    const float sx = x0 + (x1 - x0) * static_cast<float>(i) / 4.0f;
                    kit.TextSized(sx, cy + 31.0f, 12.0f, false, Tok::kOnSurfaceVariant,
                                  std::to_string(kAutosaveEvery[i]), 1,
                                  enabled ? 1.0f : kDisabledContent);
                }
            }
            break;
        }
        case RowKind::MenuButton: {
            const auto& opts = OptionsFor(r.id);
            const Rect mb{std::max(ctl.x0, ctl.x1 - 420.0f), ctl.y0, ctl.x1, ctl.y1};
            const std::size_t sel =
                std::min(static_cast<std::size_t>(std::max(r.selected, 0)), opts.size() - 1);
            kit.MenuButton(mb, r.value.empty() ? opts[sel] : r.value, view.open_menu == r.id,
                           StateOf(view, whole, enabled));
            if (enabled) {
                AddHit(mb, whole, list);
            }
            if (view.open_menu == r.id) {
                open_anchor = mb;
                open_selected = r.selected;
            }
            break;
        }
        case RowKind::Offset: {
            // AzaharFrame menuux: Reset an outlined button as wide as its label (Material: 24 dp
            // each side), the value in the room left of it, cut to fit, 16 dp clear of it.
            const Target reset{Id::ImmersiveOffsetReset};
            const std::string& reset_label = LabelFor(Id::ImmersiveOffsetReset);
            const float rw = std::max(64.0f, kit.TextWidth(Type::Label, reset_label) + 48.0f);
            const Rect rb{ctl.x1 - rw, ctl.y0, ctl.x1, ctl.y1};
            const bool can = enabled && r.on;
            const State rs = StateOf(view, reset, can);
            kit.Stroke(rb, rb.H() * 0.5f, 1.0f, Tok::kOutline, can ? 1.0f : kDisabledContainer);
            kit.Button(rb, ButtonStyle::Text, 0, reset_label, rs);
            if (can) {
                AddHit(rb, reset, list);
            }
            const float room = rb.x0 - 16.0f - ctl.x0;
            kit.TextMid(rb.x0 - 16.0f, cy, Type::Value, Tok::kOnSurfaceVariant,
                        kit.Fit(Type::Value, r.value, room), 2);
            break;
        }
        case RowKind::Link:
            kit.Icon(c.x1 - 12.0f, cy, 24.0f, Sym::kOpenInNew, false, Tok::kOnSurfaceVariant);
            break;
        case RowKind::Button:
        case RowKind::Header:
            break;
        }
        y += kRowH;
    }
    kit.ClearClip();
    DrawScrollbar(kit, view, list, pane.x1, content_h, max_scroll, scroll); // AzaharFrame scroll
    // An open menu goes over everything, its items before the rest in hit testing.
    if (view.open_menu != Id::None && open_anchor.W() > 0.0f) {
        const std::vector<std::string>& opts = OptionsFor(view.open_menu);
        const float item = Kit::MenuItemHeight();
        const float h = item * static_cast<float>(opts.size()) + 16.0f;
        float y0 = open_anchor.y1 + 4.0f;
        if (y0 + h > pane.y1) {
            y0 = std::max(pane.y0, open_anchor.y0 - 4.0f - h);
        }
        const int hover = view.hover.id == view.open_menu ? view.hover.index : -1;
        const Rect box = kit.Menu({open_anchor.x0, y0, open_anchor.x1, y0 + h}, opts, open_selected,
                                  hover);
        const Rect everything{0, 0, kPanelW, kPanelH};
        AddHit(box, {Id::None, kMenuAreaIndex}, everything); // the menu's own area eats clicks
        for (int i = 0; i < static_cast<int>(opts.size()); i++) {
            AddHit({box.x0, box.y0 + 8.0f + item * static_cast<float>(i), box.x1,
                    box.y0 + 8.0f + item * static_cast<float>(i + 1)},
                   {view.open_menu, i}, everything);
        }
    }
    DrawScopeMenu(kit, model, view, pane); // AzaharFrame reticle
}

// The tooltip (owner option A: after the beam rests on a row) over the row's label.
// AzaharFrame scroll: one scrollbar for every list (it was a thin indicator only).
void TabMenu::DrawScrollbar(Kit& kit, const MenuView& view, const Rect& list, float right,
                            float content_h, float max_scroll, float scroll) {
    const Scrolling::Bar bar{list, right, content_h, max_scroll};
    if (!bar.Valid()) {
        return;
    }
    layout.bar = bar;
    const bool held = view.pressed.id == Id::Scrollbar;
    const bool wide = held || view.hover.id == Id::Scrollbar;
    if (wide) {
        // Material 3: the track shows with a beam on the bar; the thumb widens.
        kit.Fill(Scrolling::TrackRect(bar, true), Scrolling::kBarHoverW * 0.5f, Tok::kOnSurface, 0.08f);
    }
    const Rect thumb = Scrolling::ThumbRect(bar, scroll, wide);
    kit.Fill(thumb, thumb.W() * 0.5f, held ? Tok::kPrimary : Tok::kOnSurfaceVariant,
             held ? 1.0f : wide ? 0.72f : 0.38f);
    const Rect everything{0.0f, 0.0f, kPanelW, kPanelH};
    AddHit(Scrolling::HitRect(bar), {Id::Scrollbar}, everything);
}

void TabMenu::DrawTooltip(Kit& kit, const MenuView& view, const Rect& pane) {
    if (view.tooltip == Id::None || view.open_menu != Id::None) {
        return;
    }
    // AzaharFrame xfergpu: the anchor of the index the beam rests on (a segment's own tooltip)
    // wins over the row's.
    const Anchor* found = nullptr;
    for (const Anchor& a : anchors) {
        if (a.id != view.tooltip) {
            continue;
        }
        if (a.index >= 0 && a.index == view.tooltip_index) {
            found = &a;
            break;
        }
        if (a.index < 0 && !found) {
            found = &a;
        }
    }
    if (found) {
        const Anchor& a = *found;
        const std::string text = !a.text.empty()       ? a.text
                                 : a.lock != Lock::None ? ExplainLock(a.lock)
                                                        : ExplainFor(a.id);
        if (text.empty()) {
            return;
        }
        const float half = kit.TextWidth(Type::Tooltip, text) * 0.5f + 10.0f;
        const float lx = a.cx >= 0.0f ? a.cx : a.x + kit.TextWidth(Type::Body, LabelFor(a.id)) * 0.5f;
        const float cx = std::clamp(lx, pane.x0 + half + 4.0f, pane.x1 - half - 4.0f);
        if (a.below) { // AzaharFrame reticle: under the scope toggle
            kit.Tooltip(cx, a.y, text, false);
            return;
        }
        // Above the row; under it when the row is the first one shown.
        const bool above = a.y - 40.0f > pane.y0;
        kit.Tooltip(cx, above ? a.y : a.y + kRowH - 16.0f, text, above);
        return;
    }
}

} // namespace FrameXr::Menu

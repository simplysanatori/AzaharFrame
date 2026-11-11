// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame
// The art of the vis panels. SteamVR allows 16 swapchains a session and the rest of the port
// uses most of them, so the controls help, the Controls and Stats buttons and the beam share one
// atlas (kAtlas*, at most 1000 pixels high), and only the stats, which change while a game runs,
// have a swapchain of their own.

#pragma once

#include <string>
#include <vector>

#include "frame_xr/vis/controller_diagram.h"
#include "frame_xr/vis/help_content.h"
#include "frame_xr/vis/help_layout.h"
#include "frame_xr/vis/panel_art.h"

namespace FrameXr::Vis {

struct AtlasRect {
    int x, y, w, h;
};

// The atlas: the help panel at the top left, the beam art to its right, the buttons under the
// help panel.
constexpr AtlasRect kHelpRect{0, 0, 1240, 900};
constexpr AtlasRect kBeamRect{1240, 0, 128, 256}; // kBeamLevels x kBeamTexWidth, kBeamTexHeight
constexpr AtlasRect kStripRect{0, 900, 512, 80};
constexpr int kAtlasWidth = 1368;
constexpr int kAtlasHeight = 980;

// In the help panel: the close button (centre and radius, panel pixels).
constexpr float kCloseX = 1180.0f, kCloseY = 58.0f, kCloseR = 30.0f;
// In the strip: the two buttons, [x0, x1) in strip pixels, the full height.
constexpr float kStripControls0 = 8.0f, kStripControls1 = 252.0f;
constexpr float kStripStats0 = 260.0f, kStripStats1 = 504.0f;

/// What the strip shows.
struct StripView {
    bool help_on = false;
    bool stats_on = false;
    int hover = -1; ///< 0 Controls, 1 Stats
    bool operator==(const StripView&) const = default;
};

/// Draws into `atlas` (kAtlasWidth x kAtlasHeight): each part only when asked.
void DrawHelp(PanelArt& atlas, const HelpPage& page);
/// Only the close button (under the ray or not), cheap enough to draw on any frame.
void DrawHelpClose(PanelArt& atlas, bool close_hot);
void DrawStrip(PanelArt& atlas, const StripView& view);

// AzaharFrame ctrldiag: the controls help as a diagram (help_content.h BuildHelpDiagram): line
// drawings of the controllers made on the device from the model files (controller_diagram.h),
// each label joined to its control by a leader line. Two tabs in the title row switch between
// its pages; without the model files the help is the list (DrawHelp).
struct HelpArt {
    std::array<DiagramImage, 2> top;   ///< left, right: from above, the front at the top
    std::array<DiagramImage, 2> front; ///< left, right: from the front, cut below the trigger
};
/// The drawings, a few hundred milliseconds of work: off the XR thread.
HelpArt MakeHelpArt(const ModelFile& left, const ModelFile& right);
/// Where the drawings show each part, in help panel pixels.
DiagramAnchors HelpArtAnchors(const HelpArt& art);
void DrawHelpDiagram(PanelArt& atlas, const HelpDiagram& diagram, DiagramPageId page,
                     const HelpArt& art);
/// Only the tabs (one under the ray or not, -1 none).
void DrawHelpTabs(PanelArt& atlas, const HelpDiagram& diagram, DiagramPageId page, int hot);
/// The tab at panel pixels (px, py), or -1.
int HelpTabAt(float px, float py);
void DrawBeam(PanelArt& atlas);

/// The stats panel's own art.
constexpr int kStatsWidth = 512;
constexpr int kStatsHeight = 320;
struct StatsRow {
    std::string label;
    std::string value;
};
void DrawStats(PanelArt& art, const std::vector<StatsRow>& rows);
/// AzaharFrame statspos: the same rows for the wrist (a panel a third the size): a 3 x 3 grid of
/// tiles, each a large value over its small label, so it reads at arm's length.
void DrawStatsWrist(PanelArt& art, const std::vector<StatsRow>& rows);

} // namespace FrameXr::Vis

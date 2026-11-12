// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kbd: the game's keyboard on the XR thread (swkbd_panel.h) and the requests from the
// Qt side (swkbd.h).

#include "frame_xr/swkbd_panel.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <utility>

#include "common/logging/log.h"
#include "frame_xr/input/geometry.h"
#include "frame_xr/menu/md3.h"
#include "frame_xr/menu/swkbd_view.h"
#include "frame_xr/vis/async_overlay.h"

namespace FrameXr {

namespace {

// The in-game menu's swapchain (menu_panel.cpp): 1.75 pixels a dp, 1470 x 980.
constexpr float kPxPerDp = 1.75f;
constexpr int kTexW = static_cast<int>(Swkbd::kPanelW * kPxPerDp + 0.5f);
constexpr int kTexH = static_cast<int>(Swkbd::kPanelH * kPxPerDp + 0.5f);
constexpr float kPress = 0.55f, kLetGo = 0.40f;            ///< trigger hysteresis
constexpr float kRepeatFirst = 0.5f, kRepeatEvery = 0.07f; ///< Backspace held (seconds)

Input::Posef ToInput(const XrPosef& p) {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

XrPosef ToXr(const Input::Posef& p) {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

/// A key from a physical keyboard.
struct KeyEvent {
    bool is_text = false;
    std::string text;
    GameKeyboardKey key = GameKeyboardKey::Enter;
};

/// The request from the Qt side, shared by every thread.
struct Requests {
    std::mutex mutex;
    uint64_t seq = 0;   ///< bumped by each request
    bool open = false;  ///< `done` not yet called
    bool fresh = false; ///< the XR side has not taken `spec` yet
    Swkbd::Spec spec;
    uint8_t cancel_id = 0;
    std::function<void(const SwkbdOutcome&)> done;
    std::deque<KeyEvent> keys;
};

Requests& Req() {
    static Requests r;
    return r;
}

/// Takes `done` if request `seq` is still open (any thread): the caller calls it, unlocked.
std::function<void(const SwkbdOutcome&)> TakeDone(uint64_t seq) {
    Requests& r = Req();
    std::scoped_lock lock{r.mutex};
    if (!r.open || (seq != 0 && seq != r.seq)) {
        return {};
    }
    r.open = false;
    r.fresh = false;
    r.keys.clear();
    return std::move(r.done);
}

} // namespace

bool OpenGameKeyboard(Swkbd::Spec spec, std::function<void(const SwkbdOutcome&)> done) {
    if (!VrShowing()) {
        return false;
    }
    const uint8_t cancel_id = Swkbd::Model(spec).CancelId();
    std::function<void(const SwkbdOutcome&)> old;
    {
        Requests& r = Req();
        std::scoped_lock lock{r.mutex};
        if (r.open) {
            old = std::move(r.done); // never two at once; the older one goes to the dialog
        }
        r.seq++;
        r.open = true;
        r.fresh = true;
        r.spec = std::move(spec);
        r.cancel_id = cancel_id;
        r.done = std::move(done);
        r.keys.clear();
    }
    if (old) {
        old({});
    }
    LOG_INFO(Frontend, "xr: game keyboard: asked for");
    return true;
}

bool GameKeyboardOpen() {
    Requests& r = Req();
    std::scoped_lock lock{r.mutex};
    return r.open;
}

void GameKeyboardText(const std::string& utf8) {
    Requests& r = Req();
    std::scoped_lock lock{r.mutex};
    if (r.open) {
        r.keys.push_back({true, utf8, GameKeyboardKey::Enter});
    }
}

void GameKeyboardPress(GameKeyboardKey key) {
    Requests& r = Req();
    std::scoped_lock lock{r.mutex};
    if (r.open) {
        r.keys.push_back({false, {}, key});
    }
}

void CancelGameKeyboard(bool fallback) {
    uint8_t cancel_id;
    {
        Requests& r = Req();
        std::scoped_lock lock{r.mutex};
        cancel_id = r.cancel_id;
    }
    auto done = TakeDone(0);
    if (!done) {
        return;
    }
    LOG_INFO(Frontend, "xr: game keyboard: {}",
             fallback ? "VR ended, Azahar's dialog instead" : "closed (the game stops)");
    SwkbdOutcome o;
    o.shown = !fallback;
    o.button = cancel_id;
    done(o);
}

struct SwkbdPanel::Shared {
    std::mutex mutex;
    std::condition_variable cv;
    bool quit = false;
    bool job = false;
    uint64_t job_seq = 0;
    std::optional<Swkbd::Model> job_model;
    int job_hover = -1, job_pressed = -1;
    uint64_t done_seq = 0;
    std::vector<uint32_t> pixels;
};

SwkbdPanel::SwkbdPanel() : shared(std::make_unique<Shared>()) {}

SwkbdPanel::~SwkbdPanel() {
    DestroyXr();
}

void SwkbdPanel::Create() {
    if (!worker.joinable()) {
        {
            std::scoped_lock lock{shared->mutex};
            shared->quit = false;
        }
        worker = std::thread([this] { Worker(); });
    }
    created = std::chrono::steady_clock::now();
    posted_any = false;
    ready = false;
}

void SwkbdPanel::DestroyXr() {
    if (model) {
        CancelGameKeyboard(true);
        model.reset();
    }
    if (worker.joinable()) {
        {
            std::scoped_lock lock{shared->mutex};
            shared->quit = true;
        }
        shared->cv.notify_all();
        worker.join();
    }
    request = 0;
    posted_any = false;
    ready = false;
}

// Test driver (AZAHARFRAME_TEST_SWKBD=<seconds>, for mock runs without a game that asks): a
// keyboard as Majora's Mask asks for a name, that long after the session began; the answer is
// logged.
void SwkbdPanel::TestRequest() {
    static const char* env = std::getenv("AZAHARFRAME_TEST_SWKBD");
    if (!env || test_fired) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - created < std::chrono::milliseconds(static_cast<int>(std::atof(env) * 1000.0))) {
        return;
    }
    test_fired = true;
    Swkbd::Spec s;
    s.hint = "Enter your name.";
    s.max_length = 8;
    s.buttons = {{"Cancel", 0, false}, {"OK", 1, true}};
    s.validate = [](const std::string& t) {
        return t.find_first_not_of(' ') == std::string::npos
                   ? std::string{"Blank input is not allowed"}
                   : std::string{};
    };
    OpenGameKeyboard(std::move(s), [](const SwkbdOutcome& o) {
        LOG_INFO(Frontend, "xr test: game keyboard answered: shown {}, button {}, text \"{}\"",
                 o.shown, o.button, o.text);
    });
}

bool SwkbdPanel::Poll() {
    TestRequest();
    std::deque<KeyEvent> keys;
    {
        Requests& r = Req();
        std::scoped_lock lock{r.mutex};
        if (r.open && r.fresh) {
            r.fresh = false;
            request = r.seq;
            model.emplace(r.spec);
            hover = pressed = press_hand = -1;
            trigger_down = {true, true}; // held from the game: no key until let go
            a_down = b_down = true;
            LOG_INFO(Frontend, "xr: game keyboard: open ({} buttons, up to {} characters{})",
                     model->GetSpec().buttons.size(), model->GetSpec().max_length,
                     model->GetSpec().multiline ? ", multi-line" : "");
        } else if (model && (!r.open || r.seq != request)) {
            model.reset(); // answered or cancelled from outside
            request = 0;
        }
        keys.swap(r.keys);
    }
    if (!model) {
        return false;
    }
    std::optional<Swkbd::Result> result;
    for (const KeyEvent& k : keys) {
        if (k.is_text) {
            model->InsertText(k.text);
            continue;
        }
        switch (k.key) {
        case GameKeyboardKey::Backspace:
            model->Backspace();
            break;
        case GameKeyboardKey::Delete:
            model->DeleteForward();
            break;
        case GameKeyboardKey::Left:
            model->MoveCaret(-1);
            break;
        case GameKeyboardKey::Right:
            model->MoveCaret(1);
            break;
        case GameKeyboardKey::Home:
            model->CaretHome();
            break;
        case GameKeyboardKey::End:
            model->CaretEnd();
            break;
        case GameKeyboardKey::Enter:
            result = model->Enter();
            break;
        }
        if (result) {
            break;
        }
    }
    if (result) {
        Finish(*result);
        return false;
    }
    return true;
}

void SwkbdPanel::Finish(const Swkbd::Result& result) {
    LOG_INFO(Frontend, "xr: game keyboard: closed with button {}", result.button);
    auto done = TakeDone(request);
    model.reset();
    request = 0;
    if (done) {
        done({true, result.text, result.button});
    }
}

void SwkbdPanel::Place(const XrPosef& at, float width) {
    pose = at;
    width_m = width;
    posted_any = false;
    ready = false;
    our_upload = false;
}

void SwkbdPanel::Frame(const FrameInfo& info, const Input::InputFrame& in, bool have_input,
                       Vis::AsyncOverlay& chain, std::vector<Quad>& layers,
                       std::optional<XrPosef>& cursor) {
    if (!model) {
        return;
    }
    const XrTime now = info.display_time;
    const float dt = last_time ? std::min(0.1f, static_cast<float>(now - last_time) * 1e-9f) : 0.0f;
    last_time = now;
    chain.Poll();

    // ---- The beams ----
    const Input::PanelQuad quad{ToInput(pose), width_m, Height(), true};
    std::array<Input::QuadHit, 2> hit{};
    std::array<bool, 2> edge{};
    for (int h = 0; h < Input::kNumHands; h++) {
        const auto& hand = in.hand[static_cast<std::size_t>(h)];
        const bool down = have_input && hand.trigger > (trigger_down[h] ? kLetGo : kPress);
        edge[h] = down && !trigger_down[h];
        trigger_down[h] = down;
        if (have_input && hand.active) {
            hit[h] = Input::IntersectQuad(Input::RayFromPose(hand.aim), quad);
        }
        if (edge[h] && hit[h].in_bounds) {
            active_hand = h;
        }
    }
    int p = -1;
    if (press_hand >= 0) {
        p = press_hand;
    } else if (hit[active_hand].in_bounds) {
        p = active_hand;
    } else if (hit[1 - active_hand].in_bounds) {
        p = 1 - active_hand;
    }
    hover = -1;
    if (p >= 0 && hit[p].in_bounds) {
        hover = model->KeyAt(hit[p].u * Swkbd::kPanelW, hit[p].v * Swkbd::kPanelH);
        Input::Posef dot = ToInput(pose);
        dot.position = hit[p].point_world;
        cursor = ToXr(dot);
    }
    const bool a = have_input && in.a;
    const bool a_edge = a && !a_down;
    a_down = a;
    const bool b = have_input && in.b;
    const bool b_edge = b && !b_down;
    b_down = b;

    // ---- Keys type on the press, the game's buttons answer on the release ----
    std::optional<Swkbd::Result> result;
    const auto kind_of = [&](int i) {
        return model->GetLayout().keys[static_cast<std::size_t>(i)].kind;
    };
    if (press_hand < 0 && p >= 0 && (edge[p] || a_edge) && hover >= 0) {
        press_hand = p;
        pressed = hover;
        repeat_s = 0.0f;
        if (kind_of(pressed) != Swkbd::KeyKind::Button) {
            const Swkbd::KeyKind kind = kind_of(pressed);
            model->Press(pressed); // a key's place may move (Shift, the pages): keep the index
            if (pressed >= static_cast<int>(model->GetLayout().keys.size()) ||
                kind_of(pressed) != kind) {
                pressed = -1;
            }
        }
    }
    if (press_hand >= 0) {
        const bool held = trigger_down[press_hand] || a_down;
        if (held && pressed >= 0 && kind_of(pressed) == Swkbd::KeyKind::Backspace &&
            hover == pressed) {
            repeat_s += dt;
            while (repeat_s >= kRepeatFirst) {
                model->Backspace();
                repeat_s -= kRepeatEvery;
            }
        }
        if (!held) {
            if (pressed >= 0 && hover == pressed && kind_of(pressed) == Swkbd::KeyKind::Button) {
                result = model->Press(pressed);
            }
            pressed = -1;
            press_hand = -1;
        }
    }
    if (b_edge) {
        model->Backspace();
    }
    if (result) {
        Finish(*result);
        return;
    }

    // ---- A redraw when anything changed; the newest drawing to the swapchain ----
    if (chain.Valid() && (!posted_any || model->Revision() != posted_rev || hover != posted_hover ||
                          pressed != posted_pressed)) {
        posted_any = true;
        posted_rev = model->Revision();
        posted_hover = hover;
        posted_pressed = pressed;
        {
            std::scoped_lock lock{shared->mutex};
            shared->job = true;
            shared->job_seq = ++posted;
            shared->job_model = *model;
            shared->job_hover = hover;
            shared->job_pressed = pressed;
        }
        shared->cv.notify_one();
    }
    bool fresh_upload = false;
    {
        std::scoped_lock lock{shared->mutex};
        if (shared->done_seq != uploaded && !shared->pixels.empty() && chain.Valid() &&
            chain.Width() == static_cast<uint32_t>(kTexW) &&
            chain.Height() == static_cast<uint32_t>(kTexH)) {
            uploaded = shared->done_seq;
            chain.Upload(reinterpret_cast<const uint8_t*>(shared->pixels.data()));
            fresh_upload = true;
        }
    }
    // The chain is the menu's: show it only once our own pixels are in it.
    our_upload = our_upload || fresh_upload;
    if (!ready && our_upload && !chain.Busy()) {
        ready = true;
    }
    if (ready && chain.HasImage() && info.should_render) {
        Quad q;
        q.swapchain = chain.Handle();
        q.image_rect = {{0, 0}, {kTexW, kTexH}};
        q.pose = pose;
        q.size = {width_m, Height()};
        q.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        layers.push_back(q);
    }
}

void SwkbdPanel::Worker() {
    Menu::Kit kit(kPxPerDp);
    frame_xr::ui::Canvas canvas;
    while (true) {
        std::optional<Swkbd::Model> m;
        int h, pr;
        uint64_t seq;
        {
            std::unique_lock lock{shared->mutex};
            shared->cv.wait(lock, [this] { return shared->quit || shared->job; });
            if (shared->quit) {
                return;
            }
            shared->job = false;
            seq = shared->job_seq;
            m = std::move(shared->job_model);
            h = shared->job_hover;
            pr = shared->job_pressed;
        }
        if (!m) {
            continue;
        }
        canvas.Init(kTexW, kTexH);
        kit.Begin(canvas);
        Swkbd::Draw(kit, *m, h, pr);
        std::scoped_lock lock{shared->mutex};
        shared->pixels.swap(canvas.px);
        shared->done_seq = seq;
    }
}

} // namespace FrameXr

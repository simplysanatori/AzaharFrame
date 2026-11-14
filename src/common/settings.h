// Copyright 2014-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv3 or any later version
// Refer to the LICENSE.txt file included.

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>
#include "audio_core/input_details.h"
#include "audio_core/sink_details.h"
#include "common/common_types.h"
#include "common/setting_keys.h"
#include "common/string_util.h"
#include "core/hle/service/cam/cam_params.h"

namespace Settings {

bool IsRuntimeSettingUpdateAllowed(const std::string& label);

class RuntimeSettingsGuard {
public:
    explicit RuntimeSettingsGuard(const std::vector<std::string>& non_runtime_editable_keys);
    ~RuntimeSettingsGuard();

    RuntimeSettingsGuard(const RuntimeSettingsGuard&) = delete;
    RuntimeSettingsGuard& operator=(const RuntimeSettingsGuard&) = delete;

private:
    const std::vector<std::string>* previous_keys;
};

enum class GraphicsAPI {
    Software = 0,
    OpenGL = 1,
    Vulkan = 2,
};

enum class InitClock : u32 {
    SystemTime = 0,
    FixedTime = 1,
};

enum class InitTicks : u32 {
    Random = 0,
    Fixed = 1,
};

enum class BatteryLevelSource : u32 {
    System = 0,
    Fixed = 1,
};

/** Defines the layout option for desktop and mobile landscape */
enum class LayoutOption : u32 { // Shouldn't these have set numbers to prevent last two from
                                // shifting? -OS
    Default,
    SingleScreen,
    LargeScreen,
    SideScreen,
#ifndef ANDROID
    SeparateWindows,
#endif
    HybridScreen,
    CustomLayout,
};

enum class InputMappingType : u8 { AllControllers, Guid, GuidPort };

/** Defines the layout option for mobile portrait */
enum class PortraitLayoutOption : u32 {
    // formerly mobile portrait
    PortraitTopFullWidth,
    PortraitCustomLayout,
    PortraitOriginal
};

enum class SecondaryDisplayLayout : u32 {
    None,
    TopScreenOnly,
    BottomScreenOnly,
    SideBySide,
    OppositeScreenOnly,
    Original,
    Hybrid,
    LargeScreen
};
/** Defines where the small screen will appear relative to the large screen
 * when in Large Screen mode
 */
enum class SmallScreenPosition : u32 {
    TopRight,
    MiddleRight,
    BottomRight,
    TopLeft,
    MiddleLeft,
    BottomLeft,
    AboveLarge,
    BelowLarge
};

enum class StereoRenderOption : u32 {
    Off = 0,
    SideBySide = 1,
    SideBySideFull = 2,
    Anaglyph = 3,
    Interlaced = 4,
    ReverseInterlaced = 5,
    CardboardVR = 6
};

// Which eye to render when 3d is off. 800px wide mode could be added here in the future, when
// implemented
enum class MonoRenderOption : u32 {
    LeftEye = 0,
    RightEye = 1,
};

// on android, which displays to render stereo mode to
enum class StereoWhichDisplay : u32 {
    None = 0, // equivalent to StereoRenderOption = Off
    Both = 1,
    PrimaryOnly = 2,
    SecondaryOnly = 3
};

enum class AudioEmulation : u32 {
    HLE = 0,
    LLE = 1,
    LLEMultithreaded = 2,
};

/// AzaharFrame ffaudio: how the sound plays while fast forwarding (a frame limit above 100%).
enum class FastForwardAudio : u32 {
    SpedUp = 0,      ///< as the game makes it: faster and higher pitched
    NormalPitch = 1, ///< time stretched: the pitch stays, the tempo follows the game speed
    Muted = 2,
    NormalSpeed = 3, ///< real speed and pitch: short stretches played as they are, skipping ahead
};

/// AzaharFrame ffspeed: the order the menus list the choices in (Sped Up | Normal Pitch |
/// Normal Speed | Muted). The stored values stay as they were so older configs keep their choice.
inline constexpr FastForwardAudio kFastForwardAudioOrder[] = {
    FastForwardAudio::SpedUp, FastForwardAudio::NormalPitch, FastForwardAudio::NormalSpeed,
    FastForwardAudio::Muted};

constexpr u32 FastForwardAudioIndex(FastForwardAudio mode) {
    for (u32 i = 0; i < 4; i++) {
        if (kFastForwardAudioOrder[i] == mode) {
            return i;
        }
    }
    return 1; // an unknown value shows as Normal Pitch
}

constexpr FastForwardAudio FastForwardAudioFromIndex(u32 index) {
    return kFastForwardAudioOrder[index < 4 ? index : 1];
}

enum class TextureFilter : u32 {
    NoFilter = 0,
    Anime4K = 1,
    Bicubic = 2,
    ScaleForce = 3,
    xBRZ = 4,
    MMPX = 5,
};

/// AzaharFrame xfergpu: Accurate Image Copies (image_copy_accuracy). Off: every display
/// transfer and texture copy is a blit between the rasterizer cache's surfaces, as upstream.
/// Automatic: only what a game needs is fixed: memory a surface of another stride holds is
/// copied between the strides on the GPU (Fire Emblem's glow), and an overlapping transfer the
/// GPU path can't do goes to software. On: every display transfer and texture copy takes the
/// software path (in guest memory, as the 3DS does).
enum class ImageCopyAccuracy : u32 {
    Off = 0,
    Automatic = 1,
    On = 2,
};
/// AzaharFrame fovonce: Match Game Camera (vr_immersive_match_camera_mode).
enum class MatchGameCamera : u32 {
    Off = 0,
    SetOnce = 1,
    Always = 2,
};

/// AzaharFrame xfergpu: "Off", "Automatic", "On" (the log and the crash report)
std::string_view GetImageCopyAccuracyName(ImageCopyAccuracy accuracy);

enum class TextureSampling : u32 {
    GameControlled = 0,
    NearestNeighbor = 1,
    Linear = 2,
};

enum class AspectRatio : u32 {
    Default = 0,
    R16_9 = 1,
    R4_3 = 2,
    R21_9 = 3,
    R16_10 = 4,
    Stretch = 5,
};

namespace NativeButton {

enum Values {
    A,
    B,
    X,
    Y,
    Up,
    Down,
    Left,
    Right,
    L,
    R,
    Start,
    Select,
    Debug,
    Gpio14,

    ZL,
    ZR,

    Home,
    Power,

    NumButtons,
};

constexpr int BUTTON_HID_BEGIN = A;
constexpr int BUTTON_IR_BEGIN = ZL;
constexpr int BUTTON_NS_BEGIN = Power;

constexpr int BUTTON_HID_END = BUTTON_IR_BEGIN;
constexpr int BUTTON_IR_END = BUTTON_NS_BEGIN;
constexpr int BUTTON_NS_END = NumButtons;

constexpr int NUM_BUTTONS_HID = BUTTON_HID_END - BUTTON_HID_BEGIN;
constexpr int NUM_BUTTONS_IR = BUTTON_IR_END - BUTTON_IR_BEGIN;
constexpr int NUM_BUTTONS_NS = BUTTON_NS_END - BUTTON_NS_BEGIN;

static const std::array<const char*, NumButtons> mapping = {{
    "button_a",
    "button_b",
    "button_x",
    "button_y",
    "button_up",
    "button_down",
    "button_left",
    "button_right",
    "button_l",
    "button_r",
    "button_start",
    "button_select",
    "button_debug",
    "button_gpio14",
    "button_zl",
    "button_zr",
    "button_home",
    "button_power",
}};

} // namespace NativeButton

namespace NativeAnalog {
enum Values {
    CirclePad,
    CStick,
    NumAnalogs,
};

constexpr std::array<const char*, NumAnalogs> mapping = {{
    "circle_pad",
    "c_stick",
}};
} // namespace NativeAnalog

/** The Setting class is a simple resource manager. It defines a label and default value alongside
 * the actual value of the setting for simpler and less-error prone use with frontend
 * configurations. Specifying a default value and label is required. A minimum and maximum range can
 * be specified for sanitization.
 */
template <typename Type, bool ranged = false>
class Setting {
protected:
    Setting() = default;

    /**
     * Only sets the setting to the given initializer, leaving the other members to their default
     * initializers.
     *
     * @param global_val Initial value of the setting
     */
    explicit Setting(const Type& val) : value{val} {}

public:
    /**
     * Sets a default value, label, and setting value.
     *
     * @param default_val Intial value of the setting, and default value of the setting
     * @param name Label for the setting
     */
    explicit Setting(const Type& default_val, const std::string_view& name)
        requires(!ranged)
        : value{default_val}, default_value{default_val}, label{name} {}
    virtual ~Setting() = default;

    /**
     * Sets a default value, minimum value, maximum value, and label.
     *
     * @param default_val Intial value of the setting, and default value of the setting
     * @param min_val Sets the minimum allowed value of the setting
     * @param max_val Sets the maximum allowed value of the setting
     * @param name Label for the setting
     */
    explicit Setting(const Type& default_val, const Type& min_val, const Type& max_val,
                     const std::string_view& name)
        requires(ranged)
        : value{default_val}, default_value{default_val}, maximum{max_val}, minimum{min_val},
          label{std::string(name)} {}

    /**
     *  Returns a reference to the setting's value.
     *
     * @returns A reference to the setting
     */
    [[nodiscard]] virtual const Type& GetValue() const {
        return value;
    }

    /**
     * Sets the setting to the given value.
     *
     * @param val The desired value
     */
    virtual void SetValue(const Type& val) {
        if (!IsRuntimeSettingUpdateAllowed(label)) {
            return;
        }
        Type temp{ranged ? std::clamp(val, minimum, maximum) : val};
        std::swap(value, temp);
    }

    /**
     * Returns the value that this setting was created with.
     *
     * @returns A reference to the default value
     */
    [[nodiscard]] const Type& GetDefault() const {
        return default_value;
    }

    /**
     * Returns the label this setting was created with.
     *
     * @returns A reference to the label
     */
    [[nodiscard]] const std::string& GetLabel() const {
        return label;
    }

    /**
     * Assigns a value to the setting.
     *
     * @param val The desired setting value
     *
     * @returns A reference to the setting
     */
    virtual const Type& operator=(const Type& val) {
        SetValue(val);
        return value;
    }

    /**
     * Returns a reference to the setting.
     *
     * @returns A reference to the setting
     */
    explicit virtual operator const Type&() const {
        return value;
    }

protected:
    Type value{};               ///< The setting
    const Type default_value{}; ///< The default value
    const Type maximum{};       ///< Maximum allowed value of the setting
    const Type minimum{};       ///< Minimum allowed value of the setting
    const std::string label{};  ///< The setting's label
};

/**
 * The SwitchableSetting class is a slightly more complex version of the Setting class. This adds a
 * custom setting to switch to when a guest application specifically requires it. The effect is that
 * other components of the emulator can access the setting's intended value without any need for the
 * component to ask whether the custom or global setting is needed at the moment.
 *
 * By default, the global setting is used.
 */
template <typename Type, bool ranged = false>
class SwitchableSetting : virtual public Setting<Type, ranged> {
public:
    /**
     * Sets a default value, label, and setting value.
     *
     * @param default_val Intial value of the setting, and default value of the setting
     * @param name Label for the setting
     */
    explicit SwitchableSetting(const Type& default_val, const std::string_view& name)
        requires(!ranged)
        : Setting<Type>{default_val, name} {}
    virtual ~SwitchableSetting() = default;

    /**
     * Sets a default value, minimum value, maximum value, and label.
     *
     * @param default_val Intial value of the setting, and default value of the setting
     * @param min_val Sets the minimum allowed value of the setting
     * @param max_val Sets the maximum allowed value of the setting
     * @param name Label for the setting
     */
    explicit SwitchableSetting(const Type& default_val, const Type& min_val, const Type& max_val,
                               const std::string_view& name)
        requires(ranged)
        : Setting<Type, true>{default_val, min_val, max_val, name} {}

    /**
     * Tells this setting to represent either the global or custom setting when other member
     * functions are used.
     *
     * @param to_global Whether to use the global or custom setting.
     */
    void SetGlobal(bool to_global) {
        use_global = to_global;
    }

    /**
     * Returns whether this setting is using the global setting or not.
     *
     * @returns The global state
     */
    [[nodiscard]] bool UsingGlobal() const {
        return use_global;
    }

    /**
     * Returns either the global or custom setting depending on the values of this setting's global
     * state or if the global value was specifically requested.
     *
     * @param need_global Request global value regardless of setting's state; defaults to false
     *
     * @returns The required value of the setting
     */
    [[nodiscard]] virtual const Type& GetValue() const override {
        if (use_global) {
            return this->value;
        }
        return custom;
    }
    [[nodiscard]] virtual const Type& GetValue(bool need_global) const {
        if (use_global || need_global) {
            return this->value;
        }
        return custom;
    }

    /**
     * Sets the current setting value depending on the global state.
     *
     * @param val The new value
     */
    void SetValue(const Type& val) override {
        if (!IsRuntimeSettingUpdateAllowed(this->label)) {
            return;
        }
        Type temp{ranged ? std::clamp(val, this->minimum, this->maximum) : val};
        if (use_global) {
            std::swap(this->value, temp);
        } else {
            std::swap(custom, temp);
        }
    }

    /**
     * Assigns the current setting value depending on the global state.
     *
     * @param val The new value
     *
     * @returns A reference to the current setting value
     */
    const Type& operator=(const Type& val) override {
        SetValue(val);
        return this->GetValue();
    }

    /**
     * Returns the current setting value depending on the global state.
     *
     * @returns A reference to the current setting value
     */
    virtual explicit operator const Type&() const override {
        if (use_global) {
            return this->value;
        }
        return custom;
    }

protected:
    bool use_global{true}; ///< The setting's global state
    Type custom{};         ///< The custom value of the setting
};

struct InputProfile {
    std::string name;
    std::array<std::string, NativeButton::NumButtons> buttons;
    std::array<std::string, NativeAnalog::NumAnalogs> analogs;
    std::string motion_device;
    std::string touch_device;
    std::string controller_touch_device;
    bool use_touchpad;
    bool use_touch_from_button;
    int touch_from_button_map_index;
    std::string udp_input_address;
    u16 udp_input_port;
    u8 udp_pad_index;
    InputMappingType maptype = Settings::InputMappingType::GuidPort;
};

struct TouchFromButtonMap {
    std::string name;
    std::vector<std::string> buttons;
};

/// A special region value indicating that citra will automatically select a region
/// value to fit the region lockout info of the game
static constexpr s32 REGION_VALUE_AUTO_SELECT = -1;

struct Values {
    // Controls
    InputProfile current_input_profile;       ///< The current input profile
    int current_input_profile_index;          ///< The current input profile index
    std::vector<InputProfile> input_profiles; ///< The list of input profiles
    std::vector<TouchFromButtonMap> touch_from_button_maps;
    Setting<bool> use_artic_base_controller{false, Keys::use_artic_base_controller};

    SwitchableSetting<bool> enable_gamemode{true, Keys::enable_gamemode};

    // Core
    Setting<bool> use_cpu_jit{true, Keys::use_cpu_jit};
    Setting<bool> use_fastinterp{true, Keys::use_fastinterp};
    SwitchableSetting<s32, true> cpu_clock_percentage{100, 5, 400, Keys::cpu_clock_percentage};
    SwitchableSetting<bool> is_new_3ds{true, Keys::is_new_3ds};
    SwitchableSetting<bool> lle_applets{true, Keys::lle_applets};
    SwitchableSetting<bool> deterministic_async_operations{false,
                                                           Keys::deterministic_async_operations};
    SwitchableSetting<bool> enable_required_online_lle_modules{
        false, Keys::enable_required_online_lle_modules};

    // Data Storage
    Setting<bool> use_virtual_sd{true, Keys::use_virtual_sd};
    Setting<bool> use_custom_storage{false, Keys::use_custom_storage};
    Setting<bool> compress_cia_installs{false, Keys::compress_cia_installs};
    Setting<bool> async_fs_operations{true, Keys::async_fs_operations};

    // System
    SwitchableSetting<s32> region_value{REGION_VALUE_AUTO_SELECT, Keys::region_value};
    Setting<InitClock> init_clock{InitClock::SystemTime, Keys::init_clock};
    Setting<u64> init_time{946681277ULL, Keys::init_time};
    Setting<s64> init_time_offset{0, Keys::init_time_offset};
    Setting<InitTicks> init_ticks_type{InitTicks::Random, Keys::init_ticks_type};
    Setting<s64> init_ticks_override{0, Keys::init_ticks_override};
    Setting<BatteryLevelSource> battery_state_source{BatteryLevelSource::System,
                                                     Keys::battery_state_source};
    Setting<bool> battery_charging{true, Keys::battery_charging};
    SwitchableSetting<u8, true> battery_level{0, 0, 4, Keys::battery_level};
    Setting<bool> plugin_loader_enabled{false, Keys::plugin_loader};
    Setting<bool> allow_plugin_loader{true, Keys::allow_plugin_loader};
    Setting<u16> steps_per_hour{0, Keys::steps_per_hour};
    Setting<bool> apply_region_free_patch{true, Keys::apply_region_free_patch};

    // Renderer
    // clang-format off
    SwitchableSetting<GraphicsAPI, true> graphics_api{
#if defined(ANDROID) && defined(ENABLE_VULKAN) // Prefer Vulkan on Android, OpenGL on everything else
        GraphicsAPI::Vulkan,
#elif defined(ENABLE_OPENGL)
        GraphicsAPI::OpenGL,
#elif defined(ENABLE_VULKAN)
        GraphicsAPI::Vulkan,
#elif defined(ENABLE_SOFTWARE_RENDERER)
        GraphicsAPI::Software,
#else
// TODO: Add a null renderer backend for this, perhaps.
#error "At least one renderer must be enabled."
#endif
        GraphicsAPI::Software, GraphicsAPI::Vulkan, Keys::graphics_api};
    // clang-format on
    SwitchableSetting<u32> physical_device{0, Keys::physical_device};
    Setting<bool> use_gles{false, Keys::use_gles};
    Setting<bool> renderer_debug{false, Keys::renderer_debug};
    Setting<bool> pica_debugging{false, Keys::pica_debugging};
    Setting<bool> dump_command_buffers{false, Keys::dump_command_buffers};
    SwitchableSetting<bool> spirv_shader_gen{true, Keys::spirv_shader_gen};
    SwitchableSetting<bool> disable_spirv_optimizer{true, Keys::disable_spirv_optimizer};
    SwitchableSetting<bool> async_shader_compilation{false, Keys::async_shader_compilation};
    SwitchableSetting<bool> async_presentation{true, Keys::async_presentation};
    SwitchableSetting<bool> use_hw_shader{true, Keys::use_hw_shader};
    SwitchableSetting<bool> use_disk_shader_cache{true, Keys::use_disk_shader_cache};
    SwitchableSetting<bool> use_skip_duplicate_frames{false, Keys::use_skip_duplicate_frames};
    SwitchableSetting<bool> shaders_accurate_mul{true, Keys::shaders_accurate_mul};
#ifdef ANDROID // TODO: Fuck this -OS
    SwitchableSetting<bool> use_vsync{false, Keys::use_vsync};
#else
    SwitchableSetting<bool> use_vsync{true, Keys::use_vsync};
#endif
    SwitchableSetting<bool> use_display_refresh_rate_detection{
        true, Keys::use_display_refresh_rate_detection};
    Setting<bool> use_shader_jit{true, Keys::use_shader_jit};
    // CitraVR: default 0 (auto) so the VR layer picks the per-HMD scale factor
    SwitchableSetting<u32, true> resolution_factor{0, 0, 18, Keys::resolution_factor};
    SwitchableSetting<bool> use_integer_scaling{false, Keys::use_integer_scaling};
    SwitchableSetting<double, true> frame_limit{100, 0, 1000, Keys::frame_limit};
    SwitchableSetting<double, true> turbo_limit{200, 0, 1000, Keys::turbo_limit};
    SwitchableSetting<TextureFilter> texture_filter{TextureFilter::NoFilter, Keys::texture_filter};
    SwitchableSetting<TextureSampling> texture_sampling{TextureSampling::GameControlled,
                                                        Keys::texture_sampling};
    SwitchableSetting<u16, true> delay_game_render_thread_us{0, 0, 65000,
                                                             Keys::delay_game_render_thread_us};
    SwitchableSetting<bool> simulate_3ds_gpu_timings{false, Keys::simulate_3ds_gpu_timings};

    SwitchableSetting<LayoutOption> layout_option{LayoutOption::Default, Keys::layout_option};
    SwitchableSetting<bool> swap_screen{false, Keys::swap_screen};
    SwitchableSetting<bool> upright_screen{false, Keys::upright_screen};
    SwitchableSetting<SecondaryDisplayLayout> secondary_display_layout{
        SecondaryDisplayLayout::OppositeScreenOnly, Keys::secondary_display_layout};
    SwitchableSetting<std::vector<LayoutOption>> layouts_to_cycle{
        {LayoutOption::Default, LayoutOption::SingleScreen, LayoutOption::LargeScreen,
         LayoutOption::SideScreen,
#ifndef ANDROID
         LayoutOption::SeparateWindows,
#endif
         LayoutOption::HybridScreen, LayoutOption::CustomLayout},
        Keys::layouts_to_cycle};
    SwitchableSetting<float, true> large_screen_proportion{4.f, 1.f, 16.f,
                                                           Keys::large_screen_proportion};
    SwitchableSetting<int> screen_gap{0, Keys::screen_gap};
    SwitchableSetting<SmallScreenPosition> small_screen_position{SmallScreenPosition::BottomRight,
                                                                 Keys::small_screen_position};
    Setting<u16> custom_top_x{0, Keys::custom_top_x};
    Setting<u16> custom_top_y{0, Keys::custom_top_y};
    Setting<u16> custom_top_width{800, Keys::custom_top_width};
    Setting<u16> custom_top_height{480, Keys::custom_top_height};
    Setting<u16> custom_bottom_x{80, Keys::custom_bottom_x};
    Setting<u16> custom_bottom_y{500, Keys::custom_bottom_y};
    Setting<u16> custom_bottom_width{640, Keys::custom_bottom_width};
    Setting<u16> custom_bottom_height{480, Keys::custom_bottom_height};
    Setting<u16> custom_second_layer_opacity{100, Keys::custom_second_layer_opacity};
    SwitchableSetting<AspectRatio> aspect_ratio{AspectRatio::Default, Keys::aspect_ratio};
    SwitchableSetting<bool> screen_top_stretch{false, Keys::screen_top_stretch};
    Setting<u16> screen_top_leftright_padding{0, Keys::screen_top_leftright_padding};
    Setting<u16> screen_top_topbottom_padding{0, Keys::screen_top_topbottom_padding};
    SwitchableSetting<bool> screen_bottom_stretch{false, Keys::screen_bottom_stretch};
    Setting<u16> screen_bottom_leftright_padding{0, Keys::screen_bottom_leftright_padding};
    Setting<u16> screen_bottom_topbottom_padding{0, Keys::screen_bottom_topbottom_padding};

    SwitchableSetting<PortraitLayoutOption> portrait_layout_option{
        PortraitLayoutOption::PortraitTopFullWidth, Keys::portrait_layout_option};
    Setting<u16> custom_portrait_top_x{0, Keys::custom_portrait_top_x};
    Setting<u16> custom_portrait_top_y{0, Keys::custom_portrait_top_y};
    Setting<u16> custom_portrait_top_width{800, Keys::custom_portrait_top_width};
    Setting<u16> custom_portrait_top_height{480, Keys::custom_portrait_top_height};
    Setting<u16> custom_portrait_bottom_x{80, Keys::custom_portrait_bottom_x};
    Setting<u16> custom_portrait_bottom_y{500, Keys::custom_portrait_bottom_y};
    Setting<u16> custom_portrait_bottom_width{640, Keys::custom_portrait_bottom_width};
    Setting<u16> custom_portrait_bottom_height{480, Keys::custom_portrait_bottom_height};

    SwitchableSetting<float> bg_red{0.f, Keys::bg_red};
    SwitchableSetting<float> bg_green{0.f, Keys::bg_green};
    SwitchableSetting<float> bg_blue{0.f, Keys::bg_blue};

    SwitchableSetting<StereoRenderOption> render_3d{StereoRenderOption::Off, Keys::render_3d};
    // CitraVR: default 50 (upstream 0) so stereo depth is on out of the box
    SwitchableSetting<u32> factor_3d{50, Keys::factor_3d};
    SwitchableSetting<bool> swap_eyes_3d{false, Keys::swap_eyes_3d};

    SwitchableSetting<StereoWhichDisplay> render_3d_which_display{StereoWhichDisplay::None,
                                                                  Keys::render_3d_which_display};
    SwitchableSetting<MonoRenderOption> mono_render_option{MonoRenderOption::LeftEye,
                                                           Keys::mono_render_option};

    Setting<u32> cardboard_screen_size{85, Keys::cardboard_screen_size};
    Setting<s32> cardboard_x_shift{0, Keys::cardboard_x_shift};
    Setting<s32> cardboard_y_shift{0, Keys::cardboard_y_shift};

    SwitchableSetting<bool> filter_mode{true, Keys::filter_mode};
    SwitchableSetting<std::string> pp_shader_name{"None (builtin)", Keys::pp_shader_name};
    SwitchableSetting<std::string> anaglyph_shader_name{"Dubois (builtin)",
                                                        Keys::anaglyph_shader_name};

    SwitchableSetting<bool> dump_textures{false, Keys::dump_textures};
    SwitchableSetting<bool> custom_textures{false, Keys::custom_textures};
    /// AzaharFrame memwarn: the memory dialog's Turn Off Custom Textures turned custom_textures
    /// off for the running game only (citra_qt/frame_memwarn.cpp): 1 its global value, 2 the
    /// game's own; 0 not. The config files keep it on (QtConfig::SaveUtilityValues); it is put
    /// back when the game stops.
    int custom_textures_off_for_memory = 0;
    SwitchableSetting<bool> preload_textures{false, Keys::preload_textures};
    SwitchableSetting<bool> async_custom_loading{true, Keys::async_custom_loading};
    SwitchableSetting<bool> disable_right_eye_render{false, Keys::disable_right_eye_render};
    // AzaharFrame accxfer, xfergpu: Accurate Image Copies (ImageCopyAccuracy). On costs GPU
    // readbacks and the picture comes from memory at 1x. Read at each transfer and validation
    // (rasterizer_cache.h), so it applies at once.
    SwitchableSetting<ImageCopyAccuracy> image_copy_accuracy{ImageCopyAccuracy::Automatic,
                                                             Keys::image_copy_accuracy};

    // Audio
    bool audio_muted;
    SwitchableSetting<AudioEmulation> audio_emulation{AudioEmulation::HLE, Keys::audio_emulation};
    SwitchableSetting<bool> enable_audio_stretching{false, Keys::enable_audio_stretching};
    SwitchableSetting<bool> enable_realtime_audio{false, Keys::enable_realtime_audio};
    SwitchableSetting<FastForwardAudio> fast_forward_audio{FastForwardAudio::NormalPitch,
                                                           Keys::fast_forward_audio}; // ffaudio
    SwitchableSetting<float, true> volume{1.f, 0.f, 1.f, Keys::volume};
    Setting<AudioCore::SinkType> output_type{AudioCore::SinkType::Auto, Keys::output_type};
    Setting<std::string> output_device{"Auto", Keys::output_device};
#ifdef ENABLE_QT
    Setting<bool> bypass_audio_effects{false, Keys::bypass_audio_effects};
#endif
    Setting<AudioCore::InputType> input_type{AudioCore::InputType::Auto, Keys::input_type};
    Setting<std::string> input_device{"Auto", Keys::input_device};
    SwitchableSetting<bool> simulate_headphones_plugged{false, Keys::simulate_headphones_plugged};

    // Camera
    std::array<std::string, Service::CAM::NumCameras> camera_name;
    std::array<std::string, Service::CAM::NumCameras> camera_config;
    std::array<int, Service::CAM::NumCameras> camera_flip;

    // Debugging
    bool record_frame_times;
    std::unordered_map<std::string, bool> lle_modules;
    Setting<bool> delay_start_for_lle_modules{true, Keys::delay_start_for_lle_modules};
    Setting<bool> use_gdbstub{false, Keys::use_gdbstub};
    Setting<u16> gdbstub_port{24689, Keys::gdbstub_port};
    Setting<bool> instant_debug_log{false, Keys::instant_debug_log};
    Setting<bool> enable_rpc_server{false, Keys::enable_rpc_server};
    Setting<bool> toggle_unique_data_console_type{false, Keys::toggle_unique_data_console_type};
    Setting<bool> enable_exception_handler{false, Keys::enable_exception_handler};

    // WebService
    Setting<std::string> web_api_url{"", Keys::web_api_url};
    Setting<std::string> network_token{"", Keys::network_token};

    // Miscellaneous
    Setting<std::string> log_filter{"*:Info", Keys::log_filter};
    Setting<std::string> log_regex_filter{"", Keys::log_regex_filter};

    // Video Dumping
    std::string output_format;
    std::string format_options;

    std::string video_encoder;
    std::string video_encoder_options;
    u64 video_bitrate;

    std::string audio_encoder;
    std::string audio_encoder_options;
    u64 audio_bitrate;

    // VR
    Setting<bool> vr_extra_performance_mode_enabled{false, "vr_extra_performance_mode"};
    // AzaharFrame imglag: internal (no menu): the XR runtime's image lag in display refreshes, 0
    // or 1. SteamVR shows an image one refresh after the pose sent with it, so 1 (frame_xr/
    // image_lag.h); 0 for a runtime that shows them together. The test switch
    // AZAHARFRAME_XR_IMAGE_LAG wins while it is set. A new key: older versions drop it.
    Setting<u32, true> vr_xr_image_lag{1, 0, 1, "vr_xr_image_lag"};
    // AzaharFrame: switchable, so the Qt window can set them per game (same keys as CitraVR).
    SwitchableSetting<u32> vr_immersive_mode{0, "vr_immersive_mode"};
    SwitchableSetting<int32_t> vr_si_mode_register_offset{-1, "vr_si_mode_register_offset"};
    SwitchableSetting<u32> vr_immersive_positional_game_scaler{
        0, "vr_immersive_positional_game_scaler"};
    SwitchableSetting<std::string> vr_immersive_eye_indicator{"", "vr_immersive_eye_indicator"};
    // AzaharFrame natural: in the Full modes, 3D Depth (percent of the natural 3D: 100 puts the
    // farthest scenery at infinity for the screen's size, distance and the IPD; 0 flat), World
    // Scale (percent; 100 Automatic: the screen at 3 m, the world true to the game's own 3D; the
    // screen's distance follows it, and the head moves the camera to match) and Follow Screen
    // Size (the world grows with the screen when it is resized; off: it keeps its size). Hidden,
    // per game: World Scale migrated from 1.2.0 (vr_immersive_world_set) and the game's own 3D
    // last measured (vr_immersive_eye_per_far, game units per picture width of far scenery;
    // 0 none). Where the game's 3D cannot be read, 1.2.0's scale (factor_3d with
    // vr_immersive_positional_game_scaler) is the fallback. frame_xr/immersive_layout.h "natural".
    SwitchableSetting<u32, true> vr_immersive_depth{100, 0, 100, "vr_immersive_depth"};
    SwitchableSetting<float, true> vr_immersive_world_scale{100.0f, 10.0f, 400.0f,
                                                            "vr_immersive_world_scale"};
    SwitchableSetting<bool> vr_immersive_follow_screen{true, "vr_immersive_follow_screen"};
    SwitchableSetting<bool> vr_immersive_world_set{false, "vr_immersive_world_set"};
    SwitchableSetting<float> vr_immersive_eye_per_far{0.0f, "vr_immersive_eye_per_far"};
    // AzaharFrame: show only the top screen in VR (the bottom one comes back with the bumper
    // chord, see frame_xr/immersive.h). Read when a game starts.
    SwitchableSetting<bool> vr_hide_bottom_screen{false, "vr_hide_bottom_screen"};
    // AzaharFrame: the shape of the top screen in VR (0 = 3DS 5:3, 1 = Widescreen 16:9,
    // 2 = Ultrawide 21:9, 3 = 4:3, 4 = 3:2, 5 = 16:10) for games and mods that render widescreen into the 400x240 frame.
    // Read when a game starts (frame_xr/immersive.h). AzaharFrame fixes7: 6 = Automatic, the shape
    // the game camera draws with, measured while the game runs (frame_xr/auto_shape.h).
    SwitchableSetting<u32, true> vr_top_aspect{0, 0, 6, "vr_top_aspect"};
    // AzaharFrame: the vertical field of view in degrees the game camera has, for sizing the
    // immersive screen (frame_xr/immersive.h). Read when a game starts.
    SwitchableSetting<s32, true> vr_immersive_game_fov{60, 30, 110, "vr_immersive_game_fov"};
    // AzaharFrame depth: Match Game Camera. On: the Full modes size the head-locked screen with the
    // game camera's angle and shape, measured from its projection while it runs, instead of
    // Immersive Game FOV and Screen Shape. Off: nothing is measured. Read every frame
    // (frame_xr/immersive.cpp); AZAHARFRAME_IMMERSIVE_MEASURE=0 forces it off.
    // AzaharFrame fovonce: the switch of earlier builds, read only to convert a config that has
    // nothing else (QtConfig::ReadVrValues: on is Always, off is Off); never written.
    SwitchableSetting<bool> vr_immersive_match_camera{true, "vr_immersive_match_camera"};
    // AzaharFrame fovonce: what replaces it (MatchGameCamera): Off, the two settings as they are;
    // Set Once, the game camera is measured for a while once, written into the game's own
    // Immersive Game FOV (vr_immersive_fov_measured), and the settings are used from then on;
    // Always, the earlier "on" (followed, held and eased). Read every frame.
    SwitchableSetting<u32, true> vr_immersive_match_camera_mode{2, 0, 2,
                                                                "vr_immersive_match_camera_mode"};
    // AzaharFrame fovonce: per game only: Set Once has measured this game's camera and written its
    // Immersive Game FOV. Choosing Set Once again clears it (the game measures again).
    SwitchableSetting<bool> vr_immersive_fov_measured{false, "vr_immersive_fov_measured"};
    // AzaharFrame: Full (6DoF) only. Wider View: a bigger immersive screen also widens the game's
    // view (periphery factor 1.0 to 1.4) instead of only enlarging its picture. Off by default:
    // past its own view a game's culling and scenery run out. Read when a game starts.
    SwitchableSetting<bool> vr_immersive_wider_view{false, "vr_immersive_wider_view"};
    // AzaharFrame widerview: what replaces that switch: the periphery factor of the Full modes,
    // 0 = Off (1.0x), 1 to 4 = 1.1x to 1.4x (frame_xr/immersive_layout.h). A config that has only
    // the old switch is converted when read (QtConfig::ReadVrValues). Grip + D-pad up or down on
    // the immersive screen sets it for the game on view. Read every frame. AzaharFrame
    // immersive2: 1.4x by default (the owner's Majora's Mask 3D settings).
    SwitchableSetting<u32, true> vr_immersive_wider_view_level{4, 0, 4,
                                                               "vr_immersive_wider_view_level"};
    // AzaharFrame kiu: Horizontal Scale and Vertical Scale (Full (6DoF) only): the immersive
    // screen this many percent larger (positive) or smaller (negative) than the picture on that
    // axis, -30 to +30 in 5 % steps, 0 by default, read every frame (frame_xr/immersive_layout.h
    // ScalePercent). Both grips + D-pad left or right (up or down) set it for the game on view.
    // They replace Narrow Screen and Narrow Height (vr_immersive_squeeze, _y, percent narrower):
    // a file with only those is converted once when read (FrameConfig::MigratedScale), so Narrow
    // Screen 10 % is -10 here.
    SwitchableSetting<s32, true> vr_immersive_scale_x{0, -30, 30, "vr_immersive_scale_x"};
    SwitchableSetting<s32, true> vr_immersive_scale_y{0, -30, 30, "vr_immersive_scale_y"};
    // AzaharFrame fovscale: how the scales act (Full (6DoF) only, per game, read every frame):
    // 0 = Stretch (the picture stretched on the larger or smaller screen, as every build before),
    // 1 = Field of View (the game draws that much more or less on that axis; the default). The
    // scales keep their meaning (the screen's size), so an older build reading the same file
    // shows the same screen, stretched (frame_xr/immersive_layout.h ScaleSplit).
    SwitchableSetting<u32, true> vr_immersive_scale_mode{1, 0, 1, "vr_immersive_scale_mode"};
    // AzaharFrame: how the Frame's A, B, X and Y map to the 3DS (global only): 0 = by position
    // (Frame A, at the bottom, is the 3DS B, ...), 1 = by label (A is A). Read every frame
    // (frame_xr/input/mapper.h).
    SwitchableSetting<u32, true> vr_face_buttons{0, 0, 1, "vr_face_buttons"};
    // AzaharFrame: which controller drives the 3DS gyroscope and accelerometer: 0 = off,
    // 1 = right controller, 2 = left controller, 3 = headset. Read every frame
    // (frame_xr/input/motion.h). AzaharFrame kiu: per game too (was global only).
    SwitchableSetting<u32, true> vr_motion_controls{1, 0, 3, "vr_motion_controls"};
    // AzaharFrame: what is behind the screens in VR (global only): 0 = Black, 1 = Passthrough (the
    // room, when the runtime offers it). Read every frame (frame_xr/hud.h).
    SwitchableSetting<u32, true> vr_background{0, 0, 1, "vr_background"};
    // AzaharFrame vrvis (global only): draw the Frame controllers in VR, and the stats panel. Read
    // every frame (frame_xr/vis/visuals.h).
    SwitchableSetting<bool> vr_show_controllers{true, "vr_show_controllers"};
    SwitchableSetting<bool> vr_show_stats{false, "vr_show_stats"};
    // AzaharFrame statspos (global only): where the stats panel shows: 0 = In Front (placed ahead
    // of you, then fixed in the room; a grip moves it), 1 = On Wrist (the left controller). Read
    // every frame (frame_xr/vis/stats_place.h).
    SwitchableSetting<u32, true> vr_stats_position{0, 0, 1, "vr_stats_position"};
    // AzaharFrame (global only): how large Azahar's window looks on the VR panel, in percent
    // (100, 150 or 200 in the settings; any value in between works). Read by the home panel
    // (citra_qt/frame_mirror.cpp); flat mode ignores it.
    SwitchableSetting<u32, true> vr_window_scale{150, 100, 200, "vr_window_scale"};
    // AzaharFrame vrsize (global only: it fits the player's lenses, not a game): the immersive
    // screen turned this many degrees right (x) and up (y), in 0.5 degree steps. Grip + D-pad in
    // VR sets it; read every frame (frame_xr/immersive_layout.h).
    SwitchableSetting<float, true> vr_immersive_offset_x{0.f, -10.f, 10.f, "vr_immersive_offset_x"};
    SwitchableSetting<float, true> vr_immersive_offset_y{0.f, -10.f, 10.f, "vr_immersive_offset_y"};
    // AzaharFrame ipd (global only): the headset's IPD in mm as last read from SteamVR (0 = never),
    // the fallback at the next start until it is read again (frame_xr/ipd_smoother.h). Written by
    // the XR thread only when the settled IPD moves more than 0.5 mm.
    SwitchableSetting<float, true> vr_last_ipd_mm{0.f, 0.f, 100.f, "vr_last_ipd_mm"};
    // AzaharFrame curve (global only): the normal (not immersive) screens bent into cylinder
    // layers, as the angle in degrees the main screen's width spans (0 = flat quads). The steps
    // are 15, 30, 45 and 60; grip + D-pad up or down in VR changes it. Read every frame
    // (frame_xr/input/screen_rig.h).
    SwitchableSetting<u32, true> vr_screen_curve{0, 0, 180, "vr_screen_curve"};
    // AzaharFrame layout: where the second (smaller) screen sits in VR, global: 0 Stacked (below
    // the main one, tilted back), 1 Side by Side (to its right, facing you). Left stick click held
    // 1 s switches it. Read every frame (frame_xr/input/screen_rig.h SecondFromMain).
    SwitchableSetting<u32, true> vr_screen_layout{0, 0, 1, "vr_screen_layout"};
    // AzaharFrame menu: Detach Screens, global: 0 Off (both screens move as one), 1 Secondary (the
    // Secondary Screen moves freely), 2 Both (each screen moves on its own). Normal VR screens only.
    SwitchableSetting<u32, true> vr_move_screens{0, 0, 2, "vr_move_screens"};
    // AzaharFrame menu: Touchpad (AzaharFrame reticle: per game too): 1 Tap for Input (a finger on the pad touches), 2 Click
    // for Input (pressing the pad touches, held it drags); 0 Off (config file only). The
    // controller touchpad maps onto the bottom screen. AzaharFrame menuux: Tap by default.
    SwitchableSetting<u32, true> vr_touchpad_touch{1, 0, 2, "vr_touchpad_touch"};
    // AzaharFrame menuux: Dynamic Resolution, global: the internal resolution goes down a step
    // while the headset misses frames and back up when it holds them (frame_xr/dynamic_resolution.h).
    SwitchableSetting<bool> vr_dynamic_resolution{false, "vr_dynamic_resolution"};
    // AzaharFrame mouse: Bottom Screen Reticle: 0 Off, 1 Left Controller, 2 Right Controller,
    // 3 Headset (AzaharFrame input2). That controller (or the head) aims a point on the bottom
    // screen; its trigger (with Headset: either trigger) touches there. AzaharFrame reticle: a
    // global value and, for a game, its own (Per-Game), as the Controls tab's other rows.
    SwitchableSetting<u32, true> vr_bottom_screen_reticle{0, 0, 3, "vr_bottom_screen_reticle"};
    // AzaharFrame mouse: the reticle's At Edge: 0 Follow (the edges drift), 1 Snap to Middle
    // (past an edge it re-zeroes; a held touch lifts and touches again at the middle). Per game
    // too (AzaharFrame reticle).
    SwitchableSetting<u32, true> vr_reticle_at_edge{0, 0, 1, "vr_reticle_at_edge"};
    // AzaharFrame reticle: Reticle Speed, how far the reticle moves for a turn of the head or
    // the controller, times the base mapping (0.5 to 4; frame_xr/input/reticle.h). One value
    // for every aim source; per game too.
    SwitchableSetting<float, true> vr_reticle_speed{2.0f, 0.5f, 4.0f, "vr_reticle_speed"};
    // AzaharFrame fixes7: the reticle's Aim Only While Pressed (it stays in the middle of the
    // bottom screen until the trigger touches) and Right Stick Moves Touch (AzaharFrame kiu:
    // was Stick Moves Reticle; the right stick moves the touch point too, instead of being the
    // C-Stick); frame_xr/input/reticle.h ReticleModes. Per game too.
    SwitchableSetting<bool> vr_reticle_center_until_pressed{false,
                                                            "vr_reticle_center_until_pressed"};
    SwitchableSetting<bool> vr_reticle_stick_moves{false, "vr_reticle_stick_moves"};
    // AzaharFrame kiu: C-Stick at Edge: while the reticle touches near an edge of the bottom
    // screen, the C-Stick is pushed toward that edge too (frame_xr/input/reticle.h
    // ReticleEdgeStick), so a game that turns its camera when the C-Stick aim reaches the edge
    // (Kid Icarus: Uprising with the C-Stick & Gyro mod) turns. Per game too.
    SwitchableSetting<bool> vr_reticle_edge_cstick{false, "vr_reticle_edge_cstick"};
    // AzaharFrame kiu: in a game's own file only: the Controls preset's values already decided
    // for this game (FrameXr::ControlsPresetField bits). A value the preset gave, or the game
    // already had, is never given again, so a later choice (Use Global Value too) stays.
    SwitchableSetting<u32> vr_controls_preset_done{0, "vr_controls_preset_done"};
    // AzaharFrame mouse: one-time toasts, shown once per install: the reticle's controls when it
    // is first set to a controller, and the mouse's when one is first found.
    SwitchableSetting<bool> vr_hint_reticle_shown{false, "vr_hint_reticle_shown"};
    SwitchableSetting<bool> vr_hint_mouse_shown{false, "vr_hint_mouse_shown"};
    // AzaharFrame autosave (VR and Flat; a global value and a game's own): a save state of its own
    // every few minutes of play, when the VR menu opens and when the game is closed
    // (core/autosave.h). Every is in minutes (1, 2, 5, 10 or 15); Keep is how many the game keeps
    // (1 to 10), the oldest go.
    SwitchableSetting<bool> autosave_enabled{false, "autosave_enabled"};
    SwitchableSetting<u32, true> autosave_every{5, 1, 15, "autosave_every"};
    SwitchableSetting<u32, true> autosave_keep{3, 1, 10, "autosave_keep"};
};

extern Values values;

bool IsConfiguringGlobal();
void SetConfiguringGlobal(bool is_global);

float Volume();

void LogSettings();

// Restore the global state of all applicable settings in the Values struct
void RestoreGlobalState(bool is_powered_on);

/// Gets the graphics API that should be used; not necessarily one set in settings
Settings::GraphicsAPI GetWorkingGraphicsAPI();

// Input profiles
void LoadProfile(int index);
void SaveProfile(int index);
void CreateProfile(std::string name);
void DeleteProfile(int index);
void RenameCurrentProfile(std::string new_name);

extern bool is_temporary_frame_limit;
extern double temporary_frame_limit;
static inline void ResetTemporaryFrameLimit() {
    is_temporary_frame_limit = false;
    temporary_frame_limit = 0;
}
static inline double GetFrameLimit() {
    return is_temporary_frame_limit ? temporary_frame_limit : values.frame_limit.GetValue();
}

} // namespace Settings

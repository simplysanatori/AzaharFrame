// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kbd: the game's keyboard's buttons and checks (swkbd_config.h).

#include "frame_xr/swkbd_config.h"

#include <fmt/format.h>

namespace FrameXr {

namespace {

std::string Label(const Frontend::KeyboardConfig& config, std::size_t index, const char* fallback) {
    return index < config.button_text.size() && !config.button_text[index].empty()
               ? config.button_text[index]
               : std::string{fallback};
}

} // namespace

std::string SwkbdErrorText(Frontend::ValidationError error,
                           const Frontend::KeyboardConfig& config) {
    using Frontend::ValidationError;
    switch (error) {
    case ValidationError::None:
        return {};
    // Azahar's own messages (citra_qt/applets/swkbd.cpp).
    case ValidationError::FixedLengthRequired:
        return fmt::format("Text length is not correct (should be {} characters)",
                           config.max_text_length);
    case ValidationError::MaxLengthExceeded:
        return fmt::format("Text is too long (should be no more than {} characters)",
                           config.max_text_length);
    case ValidationError::BlankInputNotAllowed:
        return "Blank input is not allowed";
    case ValidationError::EmptyInputNotAllowed:
        return "Empty input is not allowed";
    // The filters, which Azahar's dialog refuses without a word.
    case ValidationError::MaxDigitsExceeded:
        return fmt::format("Too many digits (should be no more than {})", config.max_digits);
    case ValidationError::AtSignNotAllowed:
        return "@ is not allowed";
    case ValidationError::PercentNotAllowed:
        return "% is not allowed";
    case ValidationError::BackslashNotAllowed:
        return "\\ is not allowed";
    default:
        return "Input does not match the requested software keyboard format";
    }
}

Swkbd::Spec MakeSwkbdSpec(const Frontend::SoftwareKeyboard& keyboard,
                          const Frontend::KeyboardConfig& config) {
    using Frontend::ButtonConfig;
    Swkbd::Spec spec;
    spec.hint = config.hint_text;
    spec.max_length = config.max_text_length;
    spec.multiline = config.multiline_mode;
    spec.numpad = config.type == Frontend::KeyboardType::NumPad;
    // The ids the game gets back, and the labels' places in button_text, as Azahar's dialog
    // (Cancel 0, I Forgot 1, the confirm button the last). The confirm button's id is the
    // button config's own number, which Finalize checks the text for.
    const std::string cancel = Label(config, 0, Frontend::SWKBD_BUTTON_CANCEL);
    const std::string forgot = Label(config, 1, Frontend::SWKBD_BUTTON_FORGOT);
    const std::string ok = Label(config, 2, Frontend::SWKBD_BUTTON_OKAY);
    switch (config.button_config) {
    case ButtonConfig::Single:
        spec.buttons = {{ok, 0, true}};
        break;
    case ButtonConfig::Dual:
        spec.buttons = {{cancel, 0, false}, {ok, 1, true}};
        break;
    case ButtonConfig::Triple:
        spec.buttons = {{cancel, 0, false}, {forgot, 1, false}, {ok, 2, true}};
        break;
    case ButtonConfig::None:
    default:
        // No buttons asked for (Azahar's dialog then has none): one, so the keyboard can close.
        spec.buttons = {{Frontend::SWKBD_BUTTON_OKAY, 0, true}};
        break;
    }
    const Frontend::KeyboardConfig copy = config;
    spec.filter = [&keyboard, copy](const std::string& text) {
        return SwkbdErrorText(keyboard.ValidateFilters(text), copy);
    };
    spec.validate = [&keyboard, copy](const std::string& text) {
        return SwkbdErrorText(keyboard.ValidateInput(text), copy);
    };
    return spec;
}

} // namespace FrameXr

#include "script_inputbox.h"

#include "ai.h"

InputBox::InputBox(Ai *parent, std::unique_ptr<FontCache> &font_cache, std::string event) : BaseInputBox(parent, font_cache), event_(event) {
}

InputBox::~InputBox() {
}

std::string InputBox::name() {
    return "Input Box";
}

std::filesystem::path InputBox::filename() {
    return "balloonc3.png";
}

void InputBox::activate(std::string text) {
    if (event_.starts_with("On")) {
        Request req = {"NOTIFY", event_, {text}};
        parent_->enqueueDirectSSTP({req});
    }
    else {
        Request req = {"NOTIFY", "OnUserInput", {event_, text}};
        parent_->enqueueDirectSSTP({req});
    }
    kill();
}

void InputBox::cancel() {
    // TODO timeout
    Request req = {"NOTIFY", "OnUserInputCancel", {event_, "close"}};
    parent_->enqueueDirectSSTP({req});
    kill();
}

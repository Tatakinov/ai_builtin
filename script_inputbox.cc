#include "script_inputbox.h"

#include "ai.h"

ScriptInputBox::ScriptInputBox(Ai *parent, std::unique_ptr<FontCache> &font_cache) : BaseInputBox(parent, font_cache) {
}

ScriptInputBox::~ScriptInputBox() {
}

std::string ScriptInputBox::name() {
    return "Script Input Box";
}

std::filesystem::path ScriptInputBox::filename() {
    return "balloonc0.png";
}

void ScriptInputBox::activate(std::string text) {
    Request req = {"SEND", "", {}, text};
    parent_->enqueueDirectSSTP({req});
}

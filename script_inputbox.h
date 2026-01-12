#ifndef SCRIPT_INPUTBOX_H_
#define SCRIPT_INPUTBOX_H_

#include "base_inputbox.h"

class ScriptInputBox : public BaseInputBox {
    private:
    public:
        ScriptInputBox(Ai *parent, std::unique_ptr<FontCache> &font_cache);
        ~ScriptInputBox();
        std::string name() override;
        std::filesystem::path filename() override;
        void activate(std::string text) override;
};

#endif // SCRIPT_INPUTBOX_H_

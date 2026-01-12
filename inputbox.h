#ifndef INPUTBOX_H_
#define INPUTBOX_H_

#include "base_inputbox.h"

class InputBox : public BaseInputBox {
    private:
        std::string event_;
    public:
        InputBox(Ai *parent, std::unique_ptr<FontCache> &font_cache, std::string event);
        ~InputBox();
        std::string name() override;
        std::filesystem::path filename() override;
        void activate(std::string text) override;
        void cancel() override;
};

#endif // INPUTBOX_H_

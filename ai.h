#ifndef AI_H_
#define AI_H_

#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "character.h"
#include "font_cache.h"
#include "image_cache.h"
#include "inputbox.h"
#include "misc.h"
#include "script_inputbox.h"
#include "util.h"

class Ai {
    private:
        std::mutex mutex_;
        std::condition_variable cond_;
        std::queue<std::vector<std::string>> queue_;
        std::queue<std::vector<Request>> event_queue_;
        std::unique_ptr<std::thread> th_recv_;
        std::unique_ptr<std::thread> th_send_;
        std::filesystem::path ai_dir_;
        std::unordered_map<std::string, std::string> info_;
        std::unordered_map<int, std::unique_ptr<Character>> characters_;
        std::unordered_map<std::string, std::unique_ptr<InputBox>> inputbox_;
        std::unique_ptr<ScriptInputBox> script_inputbox_;
        std::unique_ptr<ImageCache> image_cache_;
        std::unique_ptr<FontCache> font_cache_;
        std::string path_;
        std::string uuid_;
        bool alive_;
        int scale_;
        bool loaded_;
        bool redrawn_;

    public:
        Ai();
        ~Ai();

        bool init();

        void load();

        std::string getInfo(int id, std::string key, std::string default_);

        void create(int side);

        Rect getRect(int side);

        void setBalloonPosition(int side, int x, int y);
        void setBalloonDirection(int side, int direction);

        void resetBalloonPosition();

        std::optional<Offset> getCharacterOffset(int side);

        void show(int side);

        void hideAll();
        void hide(int side);
        void clearTextAll();
        void clearText(int side, bool initialize);

        void setBalloonID(int side, int id);

        void appendText(int side, const std::string &text);
        void appendLinkBegin(int side, bool is_anchor, const std::string &event, const std::vector<std::string> &args);
        void appendLinkEnd(int side);

        void setCursorPosition(int side, std::string axis, double value, bool is_absolute, MoveUnit unit);
        void newLine(int side);

        void raiseOnTalk();

        void clearCache();

        operator bool() {
            return alive_;
        }

        void run();

        std::string sendDirectSSTP(std::string method, std::string command, std::vector<std::string> args, std::string script = "");

        void enqueueDirectSSTP(std::vector<Request> list);
};

#endif // GL_AYU_H_

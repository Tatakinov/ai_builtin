#include "ai.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#if defined(_WIN32) || defined(WIN32)
#include <fcntl.h>
#include <io.h>
#include <ws2tcpip.h>
#include <afunix.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef max
#undef min
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif // WIN32

#include "character.h"
#include "font.h"
#include "logger.h"
#include "misc.h"
#include "sorakado.h"
#include "sstp.h"
#include "util.h"
#include "window.h"

namespace {
#ifndef IS_WINDOWS
    inline int closesocket(int fd) {
        return close(fd);
    }
#endif
}

Ai::~Ai() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        alive_ = false;
    }
    th_send_->join();
    th_recv_->join();
    characters_.clear();
#ifdef IS_WINDOWS
    WSACleanup();
#endif // Windows
}

Ai::Ai() : alive_(true), scale_(100), loaded_(false), redrawn_(false) {
#ifdef IS_WINDOWS
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif // Windows

    th_recv_ = std::make_unique<std::thread>([&]() {
        uint32_t len;
        while (true) {
            std::cin.read(reinterpret_cast<char *>(&len), sizeof(uint32_t));
            if (std::cin.eof() || len == 0) {
                break;
            }
            char *buffer = new char[len];
            std::cin.read(buffer, len);
            std::string request(buffer, len);
            delete[] buffer;
            if (std::cin.gcount() < len) {
                break;
            }
            auto req = sorakado::Request::parse(request);
            Logger::log(request);
            auto event = req().value();

            sorakado::Response res {204, "No Content"};

            if (event == "Initialize" && req(0)) {
                std::string tmp;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    tmp = req(0).value();
                }
                std::u8string dir(tmp.begin(), tmp.end());
                ai_dir_ = dir;
            }
            else if (event == "Endpoint" && req(0) && req(1)) {
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    path_ = req(0).value();
                    uuid_ = req(1).value();
                }
                loaded_ = true;
                cond_.notify_one();
            }
            else {
                std::vector<std::string> args;
                args.push_back(event);
                for (int i = 0; ; i++) {
                    if (req(i)) {
                        args.push_back(req(i).value());
                    }
                    else {
                        break;
                    }
                }
                std::unique_lock<std::mutex> lock(mutex_);
                queue_.push(args);
            }

            res["Charset"] = "UTF-8";

            std::string response = res;
            Logger::log(response);
            len = response.size();
            std::cout.write(reinterpret_cast<char *>(&len), sizeof(uint32_t));
            std::cout.write(response.c_str(), len);
        }
        {
            std::unique_lock<std::mutex> lock(mutex_);
            loaded_ = true;
            alive_ = false;
            event_queue_.push({{"", "", {}}});
        }
        cond_.notify_one();
    });

#if !defined(DEBUG)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [&] { return loaded_; });
    }
#else
    ai_dir_ = "./balloon";
#endif // DEBUG

    {
        std::string descript = util::readDescript(ai_dir_ / "descript.txt");
        std::istringstream iss(descript);
        std::string value;
        while (std::getline(iss, value, '\n')) {
            auto pos = value.find(',');
            if (pos == std::string::npos) {
                continue;
            }
            auto key = value.substr(0, pos);
            value = value.substr(pos + 1);
            info_[key] = value;
        }
    }

#ifdef IS_WINDOWS
    std::wstring exe_path;
    exe_path.resize(1024);
    GetModuleFileName(nullptr, exe_path.data(), 1024);
#else
    std::string exe_path;
    exe_path.resize(1024);
    readlink("/proc/self/exe", exe_path.data(), 1024);
#endif // OS
    std::filesystem::path exe_dir = exe_path;
    exe_dir = exe_dir.parent_path();
    image_cache_ = std::make_unique<ImageCache>(ai_dir_, exe_dir, false);
    font_cache_ = std::make_unique<FontCache>();
#if defined(DEBUG)
    auto family = fontlist::get_default_font();
    font_cache_->setDefaultFont(family);
#endif // DEBUG

    th_send_ = std::make_unique<std::thread>([&]() {
        while (true) {
            std::vector<Request> list;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cond_.wait(lock, [this] { return !event_queue_.empty(); });
                if (!alive_) {
                    break;
                }
                list = event_queue_.front();
                event_queue_.pop();
            }
            for (auto &request : list) {
                auto res = sstp::Response::parse(sendDirectSSTP(request.method, request.command, request.args, request.script));
                if (res.getStatusCode() != 204) {
                    break;
                }
            }
        }
    });
}

std::string Ai::getInfo(int id, std::string key, std::string default_) {
    // TODO implement override[id][key]
    if (info_.contains(key) && !info_.at(key).empty()) {
        return info_.at(key);
    }
    return default_;
}


void Ai::create(int side) {
    if (characters_.contains(side)) {
        return;
    }
    auto s = util::side2str(side);
    std::ostringstream oss;
    oss << "Balloon(" << side << ")";
    auto name = oss.str();
    characters_.emplace(side, std::make_unique<Character>(this, image_cache_, font_cache_, side, name.c_str()));
    if (util::isWayland() && getenv("NINIX_ENABLE_MULTI_MONITOR")) {
        int count = 0;
        auto *monitors = SDL_GetDisplays(&count);
        for (int i = 0; i < count; i++) {
            characters_.at(side)->create(monitors[i]);
        }
        SDL_free(monitors);
    }
    else if (util::isWayland()) {
        SDL_DisplayID id = util::getCurrentDisplayID();
        characters_.at(side)->create(id);
    }
    else {
        characters_.at(side)->create(0);
    }
    return;
}

void Ai::show(int side) {
    if (!characters_.contains(side)) {
        return;
    }
    characters_.at(side)->show();
}

void Ai::hideAll() {
    for (auto &[k, _v] : characters_) {
        hide(k);
    }
}

void Ai::hide(int side) {
    if (!characters_.contains(side)) {
        return;
    }
    characters_.at(side)->hide();
}

void Ai::clearTextAll() {
    for (auto &[k, _v] : characters_) {
        clearText(k, true);
    }
}

void Ai::clearText(int side, bool initialize) {
    if (!characters_.contains(side)) {
        return;
    }
    characters_.at(side)->clearText(initialize);
}

void Ai::setBalloonID(int side, int id) {
    if (!characters_.contains(side)) {
        return;
    }
    characters_.at(side)->setBalloonID(id);
}

void Ai::resetBalloonID(int side) {
    if (side == -1) {
        for (auto &[_, v] : characters_) {
            v->setBalloonID(0);
        }
    }
    else {
        if (!characters_.contains(side)) {
            return;
        }
        characters_.at(side)->setBalloonID(0);
    }
}

void Ai::appendText(int side, const std::string &text) {
    if (!characters_.contains(side)) {
        return;
    }
    characters_.at(side)->appendText(text);
}

void Ai::appendLinkBegin(int side, bool is_anchor, const std::string &event, const std::vector<std::string> &args) {
    if (!characters_.contains(side)) {
        return;
    }
    characters_.at(side)->appendLinkBegin(is_anchor, event, args);
}

void Ai::appendLinkEnd(int side) {
    if (!characters_.contains(side)) {
        return;
    }
    characters_.at(side)->appendLinkEnd();
}

void Ai::setCursorPosition(int side, std::string axis, double value, bool is_absolute, MoveUnit unit) {
    if (!characters_.contains(side)) {
        return;
    }
    characters_.at(side)->setCursorPosition(axis, value, is_absolute, unit);
}

void Ai::newLine(int side) {
    if (!characters_.contains(side)) {
        return;
    }
    characters_.at(side)->newLine();
}

void Ai::clearCache() {
    image_cache_->clearCache();
    for (auto &[_, v] : characters_) {
        v->clearCache();
    }
}

void Ai::run() {
    SDL_Event event;
    while ((redrawn_) ? (SDL_PollEvent(&event)) : (SDL_WaitEventTimeout(&event, 10))) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                alive_ = false;
                return;
            case SDL_EVENT_DISPLAY_ADDED:
                if (util::isWayland()) {
                    for (auto &[_, v] : characters_) {
                        v->create(event.display.displayID);
                    }
                }
                break;
            case SDL_EVENT_DISPLAY_REMOVED:
                if (util::isWayland()) {
                    for (auto &[_, v] : characters_) {
                        v->destroy(event.display.displayID);
                    }
                }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                for (auto &[_, v] : characters_) {
                    v->motion(event.motion);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                for (auto &[_, v] : characters_) {
                    v->button(event.button);
                }
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                for (auto &[_, v] : characters_) {
                    v->wheel(event.wheel);
                }
                break;
            case SDL_EVENT_TEXT_INPUT:
                if (script_inputbox_) {
                    script_inputbox_->input(event.text);
                }
                for (auto &[_, v] : inputbox_) {
                    v->input(event.text);
                }
                break;
            case SDL_EVENT_TEXT_EDITING:
                if (script_inputbox_) {
                    script_inputbox_->edit(event.edit);
                }
                for (auto &[_, v] : inputbox_) {
                    v->edit(event.edit);
                }
                break;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                if (script_inputbox_) {
                    script_inputbox_->key(event.key);
                }
                for (auto &[_, v] : inputbox_) {
                    v->key(event.key);
                }
            default:
                break;
        }
    }

    if (script_inputbox_ && !script_inputbox_->alive()) {
        script_inputbox_.reset();
    }
    std::unordered_set<std::string> erase_set;
    for (auto &[k, v] : inputbox_) {
        if (!v->alive()) {
            erase_set.emplace(k);
        }
    }
    for (auto &v : erase_set) {
        inputbox_.erase(v);
    }

    std::queue<std::vector<std::string>> queue;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            queue.push(queue_.front());
            queue_.pop();
        }
    }
    bool changed = false;
    while (!queue.empty()) {
        std::vector<std::string> args = queue.front();
        queue.pop();
        if (args[0] == "Create" && args.size() == 2) {
            int side;
            std::istringstream iss(args[1]);
            iss >> side;
            create(side);
        }
        else if (args[0] == "Show" && args.size() == 2) {
            int side;
            std::istringstream iss(args[1]);
            iss >> side;
            show(side);
        }
        else if (args[0] == "SetBalloonID" && args.size() == 3) {
            int side, id;
            util::to_x(args[1], side);
            util::to_x(args[2], id);
            setBalloonID(side, id);
        }
        else if (args[0] == "ResetBalloonID") {
            int side = -1;
            if (args.size() == 2) {
                util::to_x(args[1], side);
            }
            resetBalloonID(side);
        }
        else if (args[0] == "ConfigurationChanged") {
            for (int i = 1; i < args.size(); i++) {
                auto value = args[i];
                auto pos = value.find(',');
                if (pos == std::string::npos) {
                    continue;
                }
                auto key = value.substr(0, pos);
                value = value.substr(pos + 1);
                if (key == "scale") {
                    int scale;
                    util::to_x(value, scale);
                    if (scale == scale_ || scale < 10) {
                        continue;
                    }
                    scale_ = scale;
                    clearCache();
                    image_cache_->setScale(scale);
                    changed = true;
                }
                if (key == "font") {
                    auto &font = font_cache_->getDefaultFont();
                    if (font && font->name() == value) {
                        continue;
                    }
                    auto family = fontlist::get_default_font();
                    auto family_list = fontlist::enumerate_font();
                    for (auto &f : family_list) {
                        if (f.name == value) {
                            family = f;
                            break;
                        }
                    }
                    if (font && font->name() == family.name) {
                        continue;
                    }
                    font_cache_->setDefaultFont(family);
                }
            }
        }
        else if (args[0] == "Position" && args.size() == 4) {
            int side, x, y;
            util::to_x(args[1], side);
            util::to_x(args[2], x);
            util::to_x(args[3], y);
            setBalloonPosition(side, x, y);
        }
        else if (args[0] == "Direction" && args.size() == 3) {
            int side, direction;
            util::to_x(args[1], side);
            util::to_x(args[2], direction);
            setBalloonDirection(side, direction);
        }
        else if (args[0] == "AppendText" && args.size() == 3) {
            int side, id;
            util::to_x(args[1], side);
            appendText(side, args[2]);
        }
        else if (args[0] == "AppendLinkBegin" && args.size() >= 4) {
            int side;
            util::to_x(args[1], side);
            bool is_anchor = args[2] == "true";
            std::string event = args[3];
            std::vector<std::string> a;
            for (int i = 4; i < args.size(); i++) {
                a.push_back(args[i]);
            }
            appendLinkBegin(side, is_anchor, event, a);
        }
        else if (args[0] == "AppendLinkEnd" && args.size() == 2) {
            int side;
            util::to_x(args[1], side);
            appendLinkEnd(side);
        }
        else if (args[0] == "SetCursorPosition" && args.size() == 6) {
            int side;
            std::string axis;
            double value;
            bool is_absolute;
            MoveUnit unit = MoveUnit::Px;
            util::to_x(args[1], side);
            axis = args[2];
            util::to_x(args[3], value);
            is_absolute = args[4] == "true";
            if (args[5] == "px") {
                unit = MoveUnit::Px;
            }
            else if (args[5] == "em") {
                unit = MoveUnit::Em;
            }
            else if (args[5] == "lh") {
                unit = MoveUnit::Lh;
            }
            setCursorPosition(side, axis, value, is_absolute, unit);
        }
        else if (args[0] == "NewLine" && args.size() == 2) {
            int side;
            util::to_x(args[1], side);
            newLine(side);
        }
        else if (args[0] == "HideAll") {
            hideAll();
        }
        else if (args[0] == "Hide" && args.size() == 2) {
            int side;
            util::to_x(args[1], side);
            hide(side);
        }
        else if (args[0] == "ClearTextAll") {
            clearTextAll();
        }
        else if (args[0] == "ClearText" && args.size() == 2) {
            int side;
            util::to_x(args[1], side);
            clearText(side, false);
        }
        else if (args[0] == "OpenInputBox" && args.size() >= 2) {
            // TODO timeout, text, etc
            inputbox_[args[1]] = std::make_unique<InputBox>(this, font_cache_, args[1]);
            inputbox_.at(args[1])->init(image_cache_);
        }
        else if (args[0] == "OpenScriptInputBox" && args.size() == 1) {
            script_inputbox_ = std::make_unique<ScriptInputBox>(this, font_cache_);
            script_inputbox_->init(image_cache_);
        }
        else if (args[0] == "ScriptBegin") {
            raiseOnTalk();
        }
        else if (args[0] == "ScriptEnd") {
        }
    }
    if (script_inputbox_) {
        script_inputbox_->draw();
    }
    for (auto &[_, v] : inputbox_) {
        v->draw();
    }
    std::vector<int> keys;
    for (auto &[k, _] : characters_) {
        keys.push_back(k);
    }
    std::sort(keys.begin(), keys.end());
    for (auto k : keys) {
        characters_.at(k)->draw();
    }
    redrawn_ = false;
    if (script_inputbox_) {
        redrawn_ = script_inputbox_->swapBuffers() || redrawn_;
    }
    for (auto &[_, v] : inputbox_) {
        redrawn_ = v->swapBuffers() || redrawn_;
    }
    for (auto k : keys) {
        redrawn_ = characters_.at(k)->swapBuffers() || redrawn_;
    }
}

Rect Ai::getRect(int side) {
    if (!characters_.contains(side)) {
        return {0, 0, 0, 0};
    }
    return characters_.at(side)->getRect();
}

void Ai::setBalloonPosition(int side, int x, int y) {
    if (!characters_.contains(side)) {
        return;
    }
    characters_.at(side)->setBalloonPosition(x, y);
}

void Ai::setBalloonDirection(int side, int direction) {
    if (!characters_.contains(side)) {
        return;
    }
    characters_.at(side)->setBalloonDirection(direction);
}

void Ai::raiseOnTalk() {
    for (auto &[_, v] : characters_) {
        v->raiseOnTalk();
    }
}

std::string Ai::sendDirectSSTP(std::string method, std::string command, std::vector<std::string> args, std::string script) {
    sstp::Request req {method};
    sstp::Response res {500, "Internal Server Error"};
    req["Charset"] = "UTF-8";
    req["Ai"] = uuid_;
    if (path_.empty()) {
        return res;
    }
    req["Sender"] = "Ai_builtin";
    req["Option"] = "nodescript";
    if (req.getCommand() == "EXECUTE") {
        req["Command"] = command;
    }
    else if (req.getCommand() == "NOTIFY") {
        req["Event"] = command;
    }
    else if (req.getCommand() == "SEND") {
        req["Script"] = script;
    }
    for (int i = 0; i < args.size(); i++) {
        req(i) = args[i];
    }
    Logger::log(static_cast<std::string>(req));
    sockaddr_un addr;
    if (path_.length() >= sizeof(addr.sun_path)) {
        return res;
    }
    int soc = socket(AF_UNIX, SOCK_STREAM, 0);
    if (soc == -1) {
        return res;
    }
    memset(&addr, 0, sizeof(sockaddr_un));
    addr.sun_family = AF_UNIX;
    // null-terminatedも書き込ませる
    strncpy(addr.sun_path, path_.c_str(), path_.length() + 1);
    if (connect(soc, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) == -1) {
        return res;
    }
    std::string request = req;
    if (send(soc, request.c_str(), request.size(), 0) != request.size()) {
        closesocket(soc);
        return res;
    }
    shutdown(soc, SHUT_WR);
    char buffer[BUFFER_SIZE] = {};
    std::string data;
    while (true) {
        int ret = recv(soc, buffer, BUFFER_SIZE, 0);
        if (ret == -1) {
            closesocket(soc);
            return res;
        }
        if (ret == 0) {
            closesocket(soc);
            break;
        }
        data.append(buffer, ret);
    }
    return data;
}

void Ai::enqueueDirectSSTP(std::vector<Request> list) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        event_queue_.push(list);
    }
    cond_.notify_one();
}

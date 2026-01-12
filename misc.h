#ifndef MISC_H_
#define MISC_H_

#include <string>
#include <vector>

#if defined(_WIN32) || defined(WIN32)
#define IS_WINDOWS 1
#endif // WIN32

#if defined(__unix__) && !defined(__APPLE__)
#define IS__NIX 1
#endif // Linux

enum class BindFlag {
    True, False, Toggle,
};

enum class From {
    System, Seriko, User, YenE, Talk,
};

enum class CursorType {
    Default, Hand,
};

enum class Method {
    Base, Overlay, OverlayFast, OverlayMultiply, Replace,
    Interpolate, Asis, Move, Bind, Add, Reduce, Insert,
    Start, Stop, AlternativeStart, AlternativeStop,
    ParallelStart, ParallelStop,
};

enum class Interval {
    Sometimes, Rarely, Random, Periodic, Always, Runonce,
    Never, YenE, Talk, Bind,
};

enum class CollisionType {
    Rect, Ellipse, Circle, Polygon, Region,
};

enum class Alignment {
    Bottom, Top, Free,
};

enum class MoveUnit {
    Px, Em, Lh,
};

template <typename T>
struct Position {
    T x, y;
};

struct DragPosition {
    double ox, oy;
    int x, y;
};

struct Offset {
    int x, y;
    bool operator==(const Offset &rhs) const {
        const Offset &lhs = *this;
        return lhs.x == rhs.x && lhs.y == rhs.y;
    }
};

struct Rect {
    int x, y, width, height;
    bool operator==(const Rect &rhs) const {
        const Rect &lhs = *this;
        return lhs.x == rhs.x && lhs.y == rhs.y && lhs.width == rhs.width && lhs.height == rhs.height;
    }
};

struct Request {
    std::string method;
    std::string command;
    std::vector<std::string> args;
    std::string script;
};

#endif // MISC_H_

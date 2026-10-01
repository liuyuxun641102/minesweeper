//  Win7 / Win8 兼容版 —— 由根目录 minesweeper_gui.cpp 复制而来
//  改动: (1) 声明 _WIN32_WINNT/WINVER = 0x0601 (2) 界面字体改用 Win7 自带的 Microsoft YaHei
//  (Microsoft YaHei UI 是 Win8 才有的字体, 在 Win7 上会回退成难看的默认字体)
// ============================================================================
//  扫雷 Minesweeper —— Win32 GDI 图形界面版（零依赖，不用任何第三方库）
//
//  编译 (g++)  : g++ -std=c++17 -O2 -mwindows -o minesweeper_gui.exe minesweeper_gui.cpp -lgdi32 -luser32
//  编译 (MSVC) : cl /nologo /std:c++17 /utf-8 /EHsc /O2 /DUNICODE /D_UNICODE
//                   /Fe:minesweeper_gui.exe minesweeper_gui.cpp user32.lib gdi32.lib
//                   /link /SUBSYSTEM:WINDOWS
//
//  操作:  左键翻开   右键插旗   中键 / 左键点已翻开的数字 快速展开
//         笑脸按钮 或 F2 重开   菜单可切换难度
// ============================================================================
#define UNICODE
#define _UNICODE
#ifndef NOMINMAX
#  define NOMINMAX
#endif

#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0601
#endif
#ifndef WINVER
#  define WINVER 0x0601
#endif

#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

// ------------------------------------------------------------------ 常量 --
namespace {

constexpr int kCell = 26;     // 单格边长(px)
constexpr int kMargin = 8;    // 窗口内边距
constexpr int kPanel = 48;    // 顶部信息栏高度
constexpr int kGap = 6;       // 信息栏与棋盘间距
constexpr int kFaceSize = 38; // 笑脸按钮尺寸
constexpr int kLedW = 64;     // LED 计数框宽
constexpr int kLedH = 32;     // LED 计数框高
constexpr int kStatus = 22;   // 底部状态条高度（显示胜负提示）

constexpr UINT_PTR kTimerId = 1;

constexpr int ID_NEW = 1000;
constexpr int ID_EXIT = 1001;
constexpr int ID_BEGINNER = 1010;
constexpr int ID_INTERMEDIATE = 1011;
constexpr int ID_EXPERT = 1012;
constexpr int ID_ABOUT = 1020;

// 配色
const COLORREF kBg = RGB(192, 192, 192);
const COLORREF kLight = RGB(255, 255, 255);
const COLORREF kDark = RGB(128, 128, 128);
const COLORREF kOpenFace = RGB(198, 198, 198);
const COLORREF kGridLine = RGB(170, 170, 170);
const COLORREF kPressed = RGB(178, 178, 178);
const COLORREF kLedBg = RGB(0, 0, 0);
const COLORREF kLedFg = RGB(255, 40, 40);
const COLORREF kBlown = RGB(255, 60, 60);

// ------------------------------------------------------------------ 逻辑 --
enum class State { Playing, Won, Lost };

struct Cell {
    bool mine = false;
    bool revealed = false;
    bool flagged = false;
    int adjacent = 0;
};

class Board {
public:
    void reset(int w, int h, int mines) {
        w_ = w;
        h_ = h;
        mines_ = std::min(std::max(mines, 1), w * h - 1);
        cells_.assign(static_cast<size_t>(w) * h, Cell{});
        rng_.seed(std::random_device{}());
        state_ = State::Playing;
        started_ = false;
        opened_ = 0;
        flags_ = 0;
    }

    int w() const { return w_; }
    int h() const { return h_; }
    int mineCount() const { return mines_; }
    State state() const { return state_; }
    bool started() const { return started_; }
    int remaining() const { return mines_ - flags_; }
    int opened() const { return opened_; }
    int openedTotal() const { return w_ * h_ - mines_; }

    const Cell& at(int x, int y) const { return cells_[static_cast<size_t>(y * w_ + x)]; }
    bool isRevealed(int x, int y) const {
        if (x < 0 || y < 0 || x >= w_ || y >= h_) return false;
        return at(x, y).revealed;
    }

    void reveal(int x, int y) {
        if (state_ != State::Playing) return;
        Cell& c = cells_[static_cast<size_t>(y * w_ + x)];
        if (c.revealed || c.flagged) return;

        if (!started_) firstClick(x, y);
        if (c.mine) { c.revealed = true; finish(State::Lost); return; }

        std::vector<int> stack{ y * w_ + x };
        while (!stack.empty()) {
            const int i = stack.back();
            stack.pop_back();
            Cell& cur = cells_[static_cast<size_t>(i)];
            if (cur.revealed || cur.flagged || cur.mine) continue;
            cur.revealed = true;
            ++opened_;
            if (cur.adjacent == 0) {
                const int cx = i % w_;
                const int cy = i / w_;
                forEachNeighbor(cx, cy, [&](int nx, int ny) {
                    const Cell& n = at(nx, ny);
                    if (!n.revealed && !n.flagged) stack.push_back(ny * w_ + nx);
                });
            }
        }
        checkWin();
    }

    void toggleFlag(int x, int y) {
        if (state_ != State::Playing) return;
        Cell& c = cells_[static_cast<size_t>(y * w_ + x)];
        if (c.revealed) return;
        c.flagged = !c.flagged;
        flags_ += c.flagged ? 1 : -1;
    }

    void chord(int x, int y) {
        if (state_ != State::Playing) return;
        const Cell& c = at(x, y);
        if (!c.revealed || c.adjacent == 0) return;

        int around = 0;
        forEachNeighbor(x, y, [&](int nx, int ny) {
            if (at(nx, ny).flagged) ++around;
        });
        if (around != c.adjacent) return;

        std::vector<std::pair<int, int>> targets;
        forEachNeighbor(x, y, [&](int nx, int ny) {
            const Cell& n = at(nx, ny);
            if (!n.revealed && !n.flagged) targets.emplace_back(nx, ny);
        });
        for (const auto& t : targets) {
            if (state_ != State::Playing) break;
            reveal(t.first, t.second);
        }
        checkWin();
    }

private:
    template <typename F>
    void forEachNeighbor(int x, int y, F&& fn) const {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                const int nx = x + dx;
                const int ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= w_ || ny >= h_) continue;
                fn(nx, ny);
            }
        }
    }

    // 首点保护：首格及其周围 8 格无雷
    void firstClick(int sx, int sy) {
        started_ = true;
        std::vector<int> pool;
        pool.reserve(static_cast<size_t>(w_) * h_);
        for (int y = 0; y < h_; ++y) {
            for (int x = 0; x < w_; ++x) {
                if (std::abs(x - sx) <= 1 && std::abs(y - sy) <= 1) continue;
                pool.push_back(y * w_ + x);
            }
        }
        if (static_cast<int>(pool.size()) < mines_) {
            pool.clear();
            for (int y = 0; y < h_; ++y) {
                for (int x = 0; x < w_; ++x) {
                    if (x == sx && y == sy) continue;
                    pool.push_back(y * w_ + x);
                }
            }
        }
        std::shuffle(pool.begin(), pool.end(), rng_);
        for (int i = 0; i < mines_; ++i) cells_[static_cast<size_t>(pool[static_cast<size_t>(i)])].mine = true;

        for (int y = 0; y < h_; ++y) {
            for (int x = 0; x < w_; ++x) {
                int count = 0;
                forEachNeighbor(x, y, [&](int nx, int ny) {
                    if (at(nx, ny).mine) ++count;
                });
                cells_[static_cast<size_t>(y * w_ + x)].adjacent = count;
            }
        }
    }

    void checkWin() {
        if (state_ == State::Playing && opened_ == w_ * h_ - mines_) state_ = State::Won;
    }

    void finish(State next) { state_ = next; }

    int w_ = 9;
    int h_ = 9;
    int mines_ = 10;
    std::vector<Cell> cells_;
    std::mt19937 rng_;
    State state_ = State::Playing;
    bool started_ = false;
    int opened_ = 0;
    int flags_ = 0;
};

// ------------------------------------------------------------------ 状态 --
struct Difficulty {
    const wchar_t* name;
    int w;
    int h;
    int mines;
};

const Difficulty kDiffs[] = {
    { L"初级", 9, 9, 10 },
    { L"中级", 16, 16, 40 },
    { L"高级", 30, 16, 99 },
};
constexpr int kDiffCount = static_cast<int>(sizeof(kDiffs) / sizeof(kDiffs[0]));

struct App {
    Board board;
    int diff = 0;

    bool timerRunning = false;
    DWORD startTick = 0;
    DWORD endTick = 0;

    int pressedX = -1;
    int pressedY = -1;
    bool pressedLeft = false;
    bool facePressed = false;

    int blownX = -1;
    int blownY = -1;

    HFONT fontNum = nullptr;
    HFONT fontLed = nullptr;
    HFONT fontUi = nullptr;

    HBRUSH brBlack = nullptr;
    HBRUSH brRed = nullptr;
    HBRUSH brYellow = nullptr;
    HBRUSH brGray = nullptr;

    HPEN penBlack = nullptr;
    HPEN penBlack2 = nullptr;
};

App g;

// ------------------------------------------------------------ 几何/绘制 --
struct Layout {
    int clientW = 0;
    int clientH = 0;
    RECT face{};
    RECT board{};
};

Layout layout() {
    Layout l;
    const int bw = g.board.w() * kCell;
    const int bh = g.board.h() * kCell;
    l.clientW = kMargin * 2 + bw;
    l.clientH = kMargin * 2 + kPanel + kGap + bh + kStatus;
    l.board.left = kMargin;
    l.board.top = kMargin + kPanel + kGap;
    l.board.right = kMargin + bw;
    l.board.bottom = kMargin + kPanel + kGap + bh;
    const int fx = kMargin + (bw - kFaceSize) / 2;
    const int fy = kMargin + (kPanel - kFaceSize) / 2;
    l.face.left = fx;
    l.face.top = fy;
    l.face.right = fx + kFaceSize;
    l.face.bottom = fy + kFaceSize;
    return l;
}

RECT ledRect(bool leftSide) {
    const Layout l = layout();
    const int y = kMargin + (kPanel - kLedH) / 2;
    RECT r{};
    if (leftSide) {
        r.left = kMargin;
        r.right = kMargin + kLedW;
    } else {
        r.right = l.clientW - kMargin;
        r.left = r.right - kLedW;
    }
    r.top = y;
    r.bottom = y + kLedH;
    return r;
}

void fillRect(HDC dc, const RECT& r, COLORREF c) {
    SetDCBrushColor(dc, c);
    FillRect(dc, &r, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}

void frameRect(HDC dc, const RECT& r, COLORREF c, int width = 1) {
    HGDIOBJ oldPen = SelectObject(dc, GetStockObject(DC_PEN));
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    SetDCPenColor(dc, c);
    for (int i = 0; i < width; ++i) {
        Rectangle(dc, r.left + i, r.top + i, r.right - i, r.bottom - i);
    }
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
}

void raised(HDC dc, const RECT& r) {
    fillRect(dc, r, kBg);
    fillRect(dc, RECT{ r.left, r.top, r.right - 1, r.top + 2 }, kLight);
    fillRect(dc, RECT{ r.left, r.top, r.left + 2, r.bottom - 1 }, kLight);
    fillRect(dc, RECT{ r.left, r.bottom - 2, r.right, r.bottom }, kDark);
    fillRect(dc, RECT{ r.right - 2, r.top, r.right, r.bottom }, kDark);
}

void sunken(HDC dc, const RECT& r) {
    fillRect(dc, r, kBg);
    fillRect(dc, RECT{ r.left, r.top, r.right - 1, r.top + 2 }, kDark);
    fillRect(dc, RECT{ r.left, r.top, r.left + 2, r.bottom - 1 }, kDark);
    fillRect(dc, RECT{ r.left, r.bottom - 2, r.right, r.bottom }, kLight);
    fillRect(dc, RECT{ r.right - 2, r.top, r.right, r.bottom }, kLight);
}

int numberColor(int n) {
    switch (n) {
        case 1: return RGB(0, 0, 255);
        case 2: return RGB(0, 128, 0);
        case 3: return RGB(220, 0, 0);
        case 4: return RGB(0, 0, 140);
        case 5: return RGB(140, 0, 0);
        case 6: return RGB(0, 130, 130);
        case 7: return RGB(0, 0, 0);
        default: return RGB(110, 110, 110);
    }
}

void drawCenteredText(HDC dc, const RECT& r, const std::wstring& s, HFONT font, COLORREF color, UINT flags = DT_CENTER | DT_VCENTER | DT_SINGLELINE) {
    HGDIOBJ oldFont = SelectObject(dc, font);
    const int oldMode = SetBkMode(dc, TRANSPARENT);
    const COLORREF oldColor = SetTextColor(dc, color);
    RECT rc = r;
    DrawTextW(dc, s.c_str(), -1, &rc, flags);
    SetTextColor(dc, oldColor);
    SetBkMode(dc, oldMode);
    SelectObject(dc, oldFont);
}

void drawFlag(HDC dc, const RECT& r) {
    const int cx = (r.left + r.right) / 2;
    const int cy = (r.top + r.bottom) / 2;
    POINT tri[3] = {
        { cx, cy - 9 },
        { cx + 8, cy - 4 },
        { cx, cy + 1 },
    };
    HGDIOBJ oldBrush = SelectObject(dc, g.brRed);
    HGDIOBJ oldPen = SelectObject(dc, g.penBlack);
    Polygon(dc, tri, 3);
    // 旗杆
    MoveToEx(dc, cx, cy - 9, nullptr);
    LineTo(dc, cx, cy + 7);
    SelectObject(dc, oldBrush);
    SelectObject(dc, g.brBlack);
    Rectangle(dc, cx - 6, cy + 7, cx + 7, cy + 11);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
}

void drawMine(HDC dc, const RECT& r, bool highlight) {
    const int cx = (r.left + r.right) / 2;
    const int cy = (r.top + r.bottom) / 2;
    if (highlight) fillRect(dc, r, kBlown);

    HGDIOBJ oldBrush = SelectObject(dc, g.brBlack);
    HGDIOBJ oldPen = SelectObject(dc, g.penBlack2);
    for (int i = 0; i < 4; ++i) {
        static const int dx[4] = { 1, 0, 1, 1 };
        static const int dy[4] = { 0, 1, 1, -1 };
        MoveToEx(dc, cx - dx[i] * 9, cy - dy[i] * 9, nullptr);
        LineTo(dc, cx + dx[i] * 9, cy + dy[i] * 9);
    }
    Ellipse(dc, cx - 7, cy - 7, cx + 7, cy + 7);
    SelectObject(dc, oldBrush);
    // 高光
    HGDIOBJ ob = SelectObject(dc, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    HGDIOBJ op = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, cx - 5, cy - 5, cx - 1, cy - 1);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    SelectObject(dc, oldPen);
}

void drawFace(HDC dc, const RECT& r, bool pressed) {
    if (pressed) {
        sunken(dc, r);
    } else {
        raised(dc, r);
    }

    RECT in = r;
    InflateRect(&in, -5, -5);
    HGDIOBJ oldBrush = SelectObject(dc, g.brYellow);
    HGDIOBJ oldPen = SelectObject(dc, g.penBlack2);
    Ellipse(dc, in.left, in.top, in.right, in.bottom);

    const int cx = (in.left + in.right) / 2;
    const int cy = (in.top + in.bottom) / 2;

    auto dot = [&](int x, int y, int rad) {
        HGDIOBJ ob = SelectObject(dc, g.brBlack);
        HGDIOBJ op = SelectObject(dc, GetStockObject(NULL_PEN));
        Ellipse(dc, x - rad, y - rad, x + rad, y + rad);
        SelectObject(dc, ob);
        SelectObject(dc, op);
    };
    auto cross = [&](int x, int y) {
        MoveToEx(dc, x - 3, y - 3, nullptr);
        LineTo(dc, x + 3, y + 3);
        MoveToEx(dc, x + 3, y - 3, nullptr);
        LineTo(dc, x - 3, y + 3);
    };

    const State st = g.board.state();
    if (st == State::Lost) {
        SelectObject(dc, g.penBlack2);
        cross(cx - 6, cy - 5);
        cross(cx + 6, cy - 5);
        POINT mouth[4] = {
            { cx - 8, cy + 10 }, { cx - 4, cy + 2 }, { cx + 4, cy + 2 }, { cx + 8, cy + 10 },
        };
        PolyBezier(dc, mouth, 4);
    } else if (st == State::Won) {
        HGDIOBJ ob = SelectObject(dc, g.brBlack);
        HGDIOBJ op = SelectObject(dc, GetStockObject(NULL_PEN));
        Rectangle(dc, cx - 10, cy - 8, cx - 2, cy - 2);
        Rectangle(dc, cx + 2, cy - 8, cx + 10, cy - 2);
        SelectObject(dc, ob);
        SelectObject(dc, op);
        MoveToEx(dc, cx - 2, cy - 5, nullptr);
        LineTo(dc, cx + 2, cy - 5);
        POINT mouth[4] = {
            { cx - 8, cy + 3 }, { cx - 4, cy + 10 }, { cx + 4, cy + 10 }, { cx + 8, cy + 3 },
        };
        PolyBezier(dc, mouth, 4);
    } else {
        dot(cx - 6, cy - 5, 2);
        dot(cx + 6, cy - 5, 2);
        if (pressed || g.pressedLeft) {
            HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
            Ellipse(dc, cx - 4, cy + 2, cx + 4, cy + 10);
            SelectObject(dc, ob);
        } else {
            POINT mouth[4] = {
                { cx - 8, cy + 3 }, { cx - 4, cy + 10 }, { cx + 4, cy + 10 }, { cx + 8, cy + 3 },
            };
            PolyBezier(dc, mouth, 4);
        }
    }

    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
}

void drawLed(HDC dc, const RECT& r, int value) {
    sunken(dc, r);
    RECT inner = r;
    InflateRect(&inner, -3, -3);
    fillRect(dc, inner, kLedBg);

    int v = value;
    if (v > 999) v = 999;
    if (v < -99) v = -99;
    const bool neg = v < 0;
    std::wstring s = std::to_wstring(std::abs(v));
    while (s.size() < 3) s = L"0" + s;
    if (neg) s = L"-" + s;

    RECT text = inner;
    text.right -= 3;
    drawCenteredText(dc, text, s, g.fontLed, kLedFg, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

int elapsedSeconds() {
    if (!g.board.started()) return 0;
    const DWORD stop = (g.board.state() == State::Playing) ? GetTickCount() : g.endTick;
    if (stop < g.startTick) return 0;
    return static_cast<int>((stop - g.startTick) / 1000);
}

void drawBoard(HDC dc) {
    const Layout l = layout();
    const State st = g.board.state();

    for (int y = 0; y < g.board.h(); ++y) {
        for (int x = 0; x < g.board.w(); ++x) {
            RECT r{
                l.board.left + x * kCell,
                l.board.top + y * kCell,
                l.board.left + (x + 1) * kCell,
                l.board.top + (y + 1) * kCell,
            };
            const Cell& c = g.board.at(x, y);
            const bool hovered = (g.pressedLeft && g.pressedX == x && g.pressedY == y);

            if (!c.revealed) {
                if (hovered) {
                    fillRect(dc, r, kPressed);
                    frameRect(dc, r, kDark);
                } else {
                    raised(dc, r);
                }
                if (c.flagged) drawFlag(dc, r);
                else if (st == State::Lost && c.mine) drawMine(dc, r, false);
            } else {
                fillRect(dc, r, kOpenFace);
                frameRect(dc, r, kGridLine);
                if (c.mine) {
                    drawMine(dc, r, x == g.blownX && y == g.blownY);
                } else if (c.adjacent > 0) {
                    drawCenteredText(dc, r, std::to_wstring(c.adjacent), g.fontNum, numberColor(c.adjacent));
                }
            }
        }
    }
    frameRect(dc, l.board, kDark);
}

void drawScene(HDC dc, const RECT& client) {
    fillRect(dc, client, kBg);

    const State st = g.board.state();
    drawLed(dc, ledRect(true), g.board.remaining());

    int time = elapsedSeconds();
    if (time > 999) time = 999;
    drawLed(dc, ledRect(false), time);

    RECT face = layout().face;
    drawFace(dc, face, g.facePressed);

    drawBoard(dc);

    // 结束后在底部状态条给个文字提示
    if (st != State::Playing) {
        const wchar_t* msg = (st == State::Won) ? L"排雷成功！  按 F2 或点笑脸再来一局"
                                                : L"踩到雷了……  按 F2 或点笑脸再来一局";
        RECT r = client;
        r.top = layout().board.bottom + 2;
        r.bottom = client.bottom - 2;
        if (r.top + 8 < r.bottom) {
            drawCenteredText(dc, r, msg, g.fontUi, (st == State::Won) ? RGB(0, 110, 0) : RGB(170, 0, 0));
        }
    }
}

// ---------------------------------------------------------------- 交互 --
bool cellFromPoint(int px, int py, int& cx, int& cy) {
    const Layout l = layout();
    if (px < l.board.left || py < l.board.top || px >= l.board.right || py >= l.board.bottom) return false;
    cx = (px - l.board.left) / kCell;
    cy = (py - l.board.top) / kCell;
    return cx >= 0 && cy >= 0 && cx < g.board.w() && cy < g.board.h();
}

void syncTimer() {
    if (g.board.started() && !g.timerRunning) {
        g.timerRunning = true;
        g.startTick = GetTickCount();
        g.endTick = 0;
    }
}

void afterAction(HWND hwnd) {
    syncTimer();
    if (g.board.state() != State::Playing && g.endTick == 0) g.endTick = GetTickCount();
    InvalidateRect(hwnd, nullptr, FALSE);
}

void newGame(HWND hwnd) {
    g.board.reset(kDiffs[g.diff].w, kDiffs[g.diff].h, kDiffs[g.diff].mines);
    g.pressedX = g.pressedY = -1;
    g.pressedLeft = false;
    g.facePressed = false;
    g.blownX = g.blownY = -1;
    g.timerRunning = false;
    g.startTick = g.endTick = 0;
    InvalidateRect(hwnd, nullptr, FALSE);
}

void setDifficulty(HWND hwnd, int diff) {
    g.diff = diff;
    newGame(hwnd);

    const Layout l = layout();
    RECT r{ 0, 0, l.clientW, l.clientH };
    const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
    AdjustWindowRectEx(&r, style, FALSE, 0);
    const int frameW = (r.right - r.left) - l.clientW;
    const int frameH = (r.bottom - r.top) - l.clientH + GetSystemMetrics(SM_CYMENU);
    const int winW = l.clientW + frameW;
    const int winH = l.clientH + frameH;

    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int x = work.left + ((work.right - work.left) - winW) / 2;
    const int y = work.top + ((work.bottom - work.top) - winH) / 2;
    SetWindowPos(hwnd, nullptr, x, y, winW, winH, SWP_NOZORDER | SWP_NOACTIVATE);

    CheckMenuRadioItem(GetMenu(hwnd), ID_BEGINNER, ID_EXPERT, ID_BEGINNER + diff, MF_BYCOMMAND);
    InvalidateRect(hwnd, nullptr, FALSE);
}

// --------------------------------------------------------------- 窗口 --
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            g.fontNum = CreateFontW(-17, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                    DEFAULT_PITCH, L"Segoe UI");
            g.fontLed = CreateFontW(-21, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                    DEFAULT_PITCH, L"Consolas");
            g.fontUi = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   DEFAULT_PITCH, L"Microsoft YaHei");
            g.brBlack = CreateSolidBrush(RGB(20, 20, 20));
            g.brRed = CreateSolidBrush(RGB(220, 30, 30));
            g.brYellow = CreateSolidBrush(RGB(255, 214, 64));
            g.brGray = CreateSolidBrush(RGB(150, 150, 150));
            g.penBlack = CreatePen(PS_SOLID, 1, RGB(20, 20, 20));
            g.penBlack2 = CreatePen(PS_SOLID, 2, RGB(20, 20, 20));
            SetTimer(hwnd, kTimerId, 200, nullptr);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;  // 全部自绘 + 双缓冲，避免闪烁

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
            HGDIOBJ oldBmp = SelectObject(mem, bmp);
            drawScene(mem, rc);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, oldBmp);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_TIMER: {
            if (wp == kTimerId && g.board.state() == State::Playing && g.board.started()) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            const int px = GET_X_LPARAM(lp);
            const int py = GET_Y_LPARAM(lp);
            const POINT pt{ px, py };
            RECT face = layout().face;
            if (PtInRect(&face, pt)) {
                g.facePressed = true;
                InvalidateRect(hwnd, nullptr, FALSE);
                SetCapture(hwnd);
                return 0;
            }
            int cx = 0;
            int cy = 0;
            if (cellFromPoint(px, py, cx, cy)) {
                g.pressedX = cx;
                g.pressedY = cy;
                g.pressedLeft = true;
                InvalidateRect(hwnd, nullptr, FALSE);
                SetCapture(hwnd);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            ReleaseCapture();
            const bool wasFace = g.facePressed;
            const bool wasLeft = g.pressedLeft;
            const int px = g.pressedX;
            const int py = g.pressedY;
            g.facePressed = false;
            g.pressedLeft = false;
            g.pressedX = g.pressedY = -1;

            if (wasFace) {
                const POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                RECT face = layout().face;
                if (PtInRect(&face, pt)) {
                    newGame(hwnd);
                    return 0;
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }

            if (wasLeft && px >= 0) {
                int cx = 0;
                int cy = 0;
                if (cellFromPoint(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), cx, cy) && cx == px && cy == py) {
                    if (g.board.state() == State::Playing) {
                        if (g.board.isRevealed(cx, cy)) {
                            g.board.chord(cx, cy);
                        } else {
                            g.board.reveal(cx, cy);
                            if (g.board.state() == State::Lost) {
                                g.blownX = cx;
                                g.blownY = cy;
                            }
                        }
                    }
                }
            }
            afterAction(hwnd);
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (g.pressedLeft || g.facePressed) {
                const int px = GET_X_LPARAM(lp);
                const int py = GET_Y_LPARAM(lp);
                if (g.pressedLeft) {
                    int cx = 0;
                    int cy = 0;
                    const bool inside = cellFromPoint(px, py, cx, cy);
                    const int nx = inside ? cx : -1;
                    const int ny = inside ? cy : -1;
                    if (nx != g.pressedX || ny != g.pressedY) {
                        g.pressedX = nx;
                        g.pressedY = ny;
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                }
            }
            return 0;
        }

        case WM_RBUTTONUP: {
            int cx = 0;
            int cy = 0;
            if (cellFromPoint(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), cx, cy)) {
                g.board.toggleFlag(cx, cy);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_MBUTTONUP: {
            int cx = 0;
            int cy = 0;
            if (cellFromPoint(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), cx, cy)) {
                g.board.chord(cx, cy);
                afterAction(hwnd);
            }
            return 0;
        }

        case WM_KEYDOWN: {
            if (wp == VK_F2) {
                newGame(hwnd);
                return 0;
            }
            break;
        }

        case WM_COMMAND: {
            switch (LOWORD(wp)) {
                case ID_NEW:
                    newGame(hwnd);
                    return 0;
                case ID_BEGINNER:
                    setDifficulty(hwnd, 0);
                    return 0;
                case ID_INTERMEDIATE:
                    setDifficulty(hwnd, 1);
                    return 0;
                case ID_EXPERT:
                    setDifficulty(hwnd, 2);
                    return 0;
                case ID_ABOUT:
                    MessageBoxW(hwnd,
                                L"扫雷 Minesweeper —— Win32 GDI 图形版\n\n"
                                L"  左键点击    翻开格子\n"
                                L"  右键点击    插旗 / 取消旗\n"
                                L"  中键点击    快速展开\n"
                                L"  左键点已翻开的数字   周围旗数够时一键展开\n"
                                L"  笑脸按钮 / F2        重开一局\n\n"
                                L"第一个点开的格子及周围 8 格保证没有雷。\n"
                                L"菜单「游戏」里可以切换难度。",
                                L"操作说明", MB_OK | MB_ICONINFORMATION);
                    return 0;
                case ID_EXIT:
                    DestroyWindow(hwnd);
                    return 0;
                default:
                    break;
            }
            return 0;
        }

        case WM_DESTROY: {
            KillTimer(hwnd, kTimerId);
            if (g.fontNum) DeleteObject(g.fontNum);
            if (g.fontLed) DeleteObject(g.fontLed);
            if (g.fontUi) DeleteObject(g.fontUi);
            if (g.brBlack) DeleteObject(g.brBlack);
            if (g.brRed) DeleteObject(g.brRed);
            if (g.brYellow) DeleteObject(g.brYellow);
            if (g.brGray) DeleteObject(g.brGray);
            if (g.penBlack) DeleteObject(g.penBlack);
            if (g.penBlack2) DeleteObject(g.penBlack2);
            PostQuitMessage(0);
            return 0;
        }

        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

HMENU buildMenu() {
    HMENU bar = CreateMenu();

    HMENU game = CreatePopupMenu();
    AppendMenuW(game, MF_STRING, ID_NEW, L"新游戏(&N)\tF2");
    AppendMenuW(game, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(game, MF_STRING, ID_BEGINNER, L"初级  9×9，10 雷(&B)");
    AppendMenuW(game, MF_STRING, ID_INTERMEDIATE, L"中级  16×16，40 雷(&I)");
    AppendMenuW(game, MF_STRING, ID_EXPERT, L"高级  30×16，99 雷(&E)");
    AppendMenuW(game, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(game, MF_STRING, ID_EXIT, L"退出(&X)");
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(game), L"游戏(&G)");

    HMENU help = CreatePopupMenu();
    AppendMenuW(help, MF_STRING, ID_ABOUT, L"操作说明(&H)");
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(help), L"帮助(&H)");

    return bar;
}

}  // namespace

// ------------------------------------------------------------------ 入口 --
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    SetProcessDPIAware();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"MinesweeperGdiWindow";
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    if (!RegisterClassExW(&wc)) return 1;

    g.board.reset(kDiffs[g.diff].w, kDiffs[g.diff].h, kDiffs[g.diff].mines);

    const Layout l = layout();
    RECT r{ 0, 0, l.clientW, l.clientH };
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRectEx(&r, style, FALSE, 0);
    const int winW = (r.right - r.left) + (l.clientW - l.clientW);
    const int winH = (r.bottom - r.top) + GetSystemMetrics(SM_CYMENU);

    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int x = work.left + ((work.right - work.left) - winW) / 2;
    const int y = work.top + ((work.bottom - work.top) - winH) / 3;

    HWND hwnd = CreateWindowExW(0, L"MinesweeperGdiWindow", L"扫雷 Minesweeper",
                                style, x, y, winW, winH,
                                nullptr, nullptr, hInst, nullptr);
    if (!hwnd) return 1;

    SetMenu(hwnd, buildMenu());
    CheckMenuRadioItem(GetMenu(hwnd), ID_BEGINNER, ID_EXPERT, ID_BEGINNER, MF_BYCOMMAND);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

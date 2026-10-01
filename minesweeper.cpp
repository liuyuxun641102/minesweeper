// ============================================================================
//  扫雷 Minesweeper —— 单文件 C++17 控制台游戏
//
//  编译 (MSVC) : cl /std:c++17 /utf-8 /EHsc /O2 minesweeper.cpp
//  编译 (g++)  : g++ -std=c++17 -O2 -o minesweeper.exe minesweeper.cpp
//  运行        : minesweeper.exe [1|2|3]
//
//  操作:  A1 翻开   F A1 插旗   A1(已翻开的数字) 快速展开   N 重开  H 帮助  Q 退出
// ============================================================================
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace {

// ---------------------------------------------------------------- 终端控制 --
std::string paint(int code, const std::string& s) {
    return "\x1b[" + std::to_string(code) + "m" + s + "\x1b[0m";
}

void setupConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &mode)) {
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
}

// -------------------------------------------------------------------- 数据 --
struct Difficulty {
    const char* name;
    int width;
    int height;
    int mines;
};

struct Cell {
    bool mine = false;
    bool revealed = false;
    bool flagged = false;
    int adjacent = 0;
};

enum class State { Playing, Won, Lost };

struct Move {
    bool ok = false;
    bool flag = false;
    int x = 0;
    int y = 0;
};

// -------------------------------------------------------------------- 游戏 --
class Game {
public:
    explicit Game(const Difficulty& d) : w_(d.width), h_(d.height) {
        mines_ = std::min(std::max(d.mines, 1), w_ * h_ - 1);
        cells_.assign(static_cast<size_t>(w_) * h_, Cell{});
        std::random_device rd;
        rng_.seed(rd());
    }

    int width() const { return w_; }
    int height() const { return h_; }
    int mineCount() const { return mines_; }
    State state() const { return state_; }
    int remainingMines() const { return mines_ - flags_; }
    bool isRevealed(int x, int y) const { return at(x, y).revealed; }

    void reveal(int x, int y) {
        if (state_ != State::Playing) return;
        Cell& c = at(x, y);
        if (c.revealed || c.flagged) return;

        if (!started_) startFirstClick(x, y);

        if (c.mine) {
            c.revealed = true;
            finish(State::Lost);
            return;
        }

        // 迭代式洪水填充，避免大棋盘递归爆栈
        std::vector<int> stack{index(x, y)};
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
                    if (!n.revealed && !n.flagged) stack.push_back(index(nx, ny));
                });
            }
        }
        checkWin();
    }

    void toggleFlag(int x, int y) {
        if (state_ != State::Playing) return;
        Cell& c = at(x, y);
        if (c.revealed) return;
        c.flagged = !c.flagged;
        flags_ += c.flagged ? 1 : -1;
    }

    // 在已翻开的数字上按键：周围旗数够了就一次性打开其余邻格
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
    }

    void render() const {
        std::cout << '\n';

        // 状态栏
        std::string status = "  " + std::string(difficultyTag()) + "   ";
        status += "剩余雷 " + std::to_string(remainingMines());
        status += "   已翻开 " + std::to_string(opened_) + "/" + std::to_string(w_ * h_ - mines_);
        status += "   用时 " + std::to_string(elapsedSeconds()) + "s";
        std::cout << paint(state_ == State::Lost ? 91 : 96, status) << "\n\n";

        // 列标
        std::string head = "   ";
        for (int x = 0; x < w_; ++x) {
            head += static_cast<char>('A' + x);
            head += ' ';
        }
        std::cout << paint(90, head) << '\n';

        // 棋盘
        for (int y = 0; y < h_; ++y) {
            std::string row = y + 1 < 10 ? " " : "";
            row += std::to_string(y + 1) + " ";
            row = paint(90, row);
            for (int x = 0; x < w_; ++x) {
                row += cellText(x, y);
                row += ' ';
            }
            std::cout << row << '\n';
        }
        std::cout << '\n';
    }

private:
    using Clock = std::chrono::steady_clock;

    static int numberColor(int n) {
        switch (n) {
            case 1: return 94;  // 蓝
            case 2: return 92;  // 绿
            case 3: return 91;  // 红
            case 4: return 95;  // 紫
            case 5: return 93;  // 黄
            case 6: return 96;  // 青
            case 7: return 97;  // 白
            default: return 90; // 灰
        }
    }

    const char* difficultyTag() const {
        if (w_ <= 10) return "[初级]";
        if (w_ <= 18) return "[中级]";
        return "[高级]";
    }

    int index(int x, int y) const { return y * w_ + x; }

    const Cell& at(int x, int y) const { return cells_[static_cast<size_t>(index(x, y))]; }
    Cell& at(int x, int y) { return cells_[static_cast<size_t>(index(x, y))]; }

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

    // 首点保护：先落子再布雷，首个格子及周围 8 格保证无雷
    void startFirstClick(int sx, int sy) {
        started_ = true;
        began_ = Clock::now();

        std::vector<int> pool;
        pool.reserve(static_cast<size_t>(w_) * h_);
        for (int y = 0; y < h_; ++y) {
            for (int x = 0; x < w_; ++x) {
                if (std::abs(x - sx) <= 1 && std::abs(y - sy) <= 1) continue;
                pool.push_back(index(x, y));
            }
        }
        if (static_cast<int>(pool.size()) < mines_) {  // 棋盘太小则只保护首格
            pool.clear();
            for (int y = 0; y < h_; ++y) {
                for (int x = 0; x < w_; ++x) {
                    if (x == sx && y == sy) continue;
                    pool.push_back(index(x, y));
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
                at(x, y).adjacent = count;
            }
        }
    }

    void checkWin() {
        if (state_ == State::Playing && opened_ == w_ * h_ - mines_) finish(State::Won);
    }

    void finish(State next) {
        if (state_ != State::Playing) return;
        state_ = next;
        ended_ = Clock::now();
    }

    long long elapsedSeconds() const {
        if (!started_) return 0;
        const auto stop = state_ == State::Playing ? Clock::now() : ended_;
        return std::chrono::duration_cast<std::chrono::seconds>(stop - began_).count();
    }

    std::string cellText(int x, int y) const {
        const Cell& c = at(x, y);
        if (c.revealed) {
            if (c.mine) return paint(91, "*");
            if (c.adjacent == 0) return paint(90, ".");
            return paint(numberColor(c.adjacent), std::to_string(c.adjacent));
        }
        if (c.flagged) return paint(93, "F");
        if (state_ == State::Lost && c.mine) return paint(91, "*");
        return paint(90, "#");
    }

    int w_;
    int h_;
    int mines_ = 0;
    std::vector<Cell> cells_;
    std::mt19937 rng_;
    State state_ = State::Playing;
    bool started_ = false;
    int opened_ = 0;
    int flags_ = 0;
    Clock::time_point began_{};
    Clock::time_point ended_{};
};

// -------------------------------------------------------------------- 输入 --
std::string normalize(const std::string& raw) {
    std::string out;
    for (char ch : raw) {
        const unsigned char u = static_cast<unsigned char>(ch);
        if (std::isspace(u)) continue;
        out += static_cast<char>(std::toupper(u));
    }
    return out;
}

Move parseMove(const std::string& s, int w, int h) {
    Move m;
    size_t i = 0;
    if (!s.empty() && s[0] == 'F' && s.size() > 1) {
        m.flag = true;
        i = 1;
    }
    int x = -1;
    while (i < s.size() && std::isalpha(static_cast<unsigned char>(s[i]))) {
        x = (x < 0 ? 0 : x * 26) + (s[i] - 'A' + 1);
        ++i;
    }
    int y = -1;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
        y = (y < 0 ? 0 : y * 10) + (s[i] - '0');
        ++i;
    }
    if (i != s.size()) return m;                 // 有多余字符
    if (x < 1 || x > w || y < 1 || y > h) return m;
    m.x = x - 1;
    m.y = y - 1;
    m.ok = true;
    return m;
}

// -------------------------------------------------------------------- 界面 --
void printBanner() {
    std::cout << paint(96,
        "\n"
        "  +-----------------------------------+\n"
        "  |        M I N E S W E E P          |\n"
        "  |            扫  雷                 |\n"
        "  +-----------------------------------+\n\n");
}

void printHelp() {
    std::cout << paint(90,
        "  用法  A1            翻开 A 列 1 行\n"
        "        F A1 / FA1    插旗 / 取消旗\n"
        "        A1 (已翻开)   数字周围旗数够时一键展开\n"
        "        N             重开本难度    H 帮助    Q 退出\n\n");
}

void printMenu(const std::vector<Difficulty>& diffs) {
    std::cout << "  请选择难度：\n";
    for (size_t i = 0; i < diffs.size(); ++i) {
        std::cout << "    " << paint(93, std::to_string(i + 1)) << ") " << diffs[i].name
                  << paint(90, "  " + std::to_string(diffs[i].width) + "×" +
                                    std::to_string(diffs[i].height) + " / " +
                                    std::to_string(diffs[i].mines) + " 雷")
                  << '\n';
    }
    std::cout << "  " << paint(90, "输入序号开始，Q 退出") << "\n> " << std::flush;
}

bool playGame(Game& game) {
    printHelp();
    while (true) {
        game.render();
        if (game.state() != State::Playing) break;

        std::cout << "> " << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) return false;

        const std::string cmd = normalize(line);
        if (cmd.empty()) continue;
        if (cmd == "Q") return false;
        if (cmd == "H" || cmd == "?") { printHelp(); continue; }
        if (cmd == "N" || cmd == "R") return true;

        const Move m = parseMove(cmd, game.width(), game.height());
        if (!m.ok) {
            std::cout << paint(91, "  无法识别，输入 H 查看用法。\n");
            continue;
        }
        if (m.flag) {
            game.toggleFlag(m.x, m.y);
        } else if (game.isRevealed(m.x, m.y)) {
            game.chord(m.x, m.y);
        } else {
            game.reveal(m.x, m.y);
        }
    }

    if (game.state() == State::Won) {
        std::cout << paint(92, "  *** 排雷成功，全部安全区已清理！ ***\n\n");
    } else {
        std::cout << paint(91, "  *** 踩到雷了，游戏结束。 ***\n\n");
    }

    while (true) {
        std::cout << "  再来一局？(Y 换难度 / N 退出) > " << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) return false;
        const std::string cmd = normalize(line);
        if (cmd == "Y") return false;   // 回到难度菜单
        if (cmd == "N" || cmd == "Q") return true;
        if (cmd == "R") return true;    // 同难度重开
    }
}

}  // namespace

int main(int argc, char** argv) {
    setupConsole();

    const std::vector<Difficulty> kDifficulties = {
        {"初级 Beginner", 9, 9, 10},
        {"中级 Intermediate", 16, 16, 40},
        {"高级 Expert", 30, 16, 99},
    };

    printBanner();

    while (true) {
        int choice = 0;
        if (argc > 1) {
            const std::string a = argv[1];
            if (a == "1" || a == "easy") choice = 1;
            else if (a == "2" || a == "medium") choice = 2;
            else if (a == "3" || a == "hard") choice = 3;
            argc = 0;  // 参数只用一次
        }

        while (choice < 1 || choice > static_cast<int>(kDifficulties.size())) {
            printMenu(kDifficulties);
            std::string line;
            if (!std::getline(std::cin, line)) return 0;
            const std::string cmd = normalize(line);
            if (cmd == "Q") { std::cout << "  再见！\n"; return 0; }
            try {
                choice = std::stoi(cmd);
            } catch (...) {
                choice = 0;
            }
            if (choice < 1 || choice > static_cast<int>(kDifficulties.size())) {
                std::cout << paint(91, "  请输入 1 - 3。\n");
            }
        }

        Game game(kDifficulties[static_cast<size_t>(choice - 1)]);
        if (playGame(game)) break;  // 玩家选择退出
    }

    std::cout << "  再见！\n";
    return 0;
}

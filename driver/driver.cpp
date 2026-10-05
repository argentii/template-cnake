// Runtime shell for Template Snake. It reads keys, keeps time, launches the
// compiler and runs the binaries it produces. It does NO game logic: the frame,
// the key->slot table, the terminal flag and the successor headers all come
// out of the frame binary B(S), which the template engine computed.
//
// usage: driver [--tick MS] [--calibrate] [--seed N] [--script KEYS] [--no-delay] [--naive]
//               [--grid WxH] [--root DIR] [--log FILE]
//   --tick    slowest tick compiles need (floor); without it, measured at startup.
//             The game sets its own speed (B info "tick") and is slowed to the floor if needed.
//   --calibrate  measure, print the tick it would pick, and exit
//   --grid WxH   board size passed to the compiler (3x2 to 63x63; default 16x12)
//   --script  one input per tick: U D L R, or '.' for none; implies no terminal UI
//   keys: arrows/WASD steer, q quits, r starts a new game from the game-over screen
//   --no-delay  do not wait for the tick clock (replay as fast as compiles allow)
#include <cerrno>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

namespace {

// ---- options ----------------------------------------------------------------
struct Options {
    // Slowest the hardware allows: the game's own tick (from B info) is used
    // unless compiles can't keep up with it. -1 = measure at startup.
    int floorMs = -1;
    bool calibrateOnly = false;
    unsigned seed = 1;
    bool scripted = false;
    std::string script;
    bool noDelay = false;
    bool naive = false;
    std::string root = ".";
    int gridW = 0, gridH = 0;  // 0 = engine default
    std::string log = "tmp/driver.log";
};
Options opt;

// ---- clock & logging --------------------------------------------------------
double nowMs() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

FILE* logFile = nullptr;
void logf(const char* fmt, ...) {
    if (!logFile) return;
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(logFile, fmt, ap);
    va_end(ap);
    std::fputc('\n', logFile);
    std::fflush(logFile);
}

[[noreturn]] void die(const char* fmt, ...);

// ---- terminal ---------------------------------------------------------------
termios savedTermios;
bool rawMode = false;

void restoreTerminal() {
    if (rawMode) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &savedTermios);
        rawMode = false;
        static const char show[] = "\x1b[?25h\n";
        (void)!write(STDOUT_FILENO, show, sizeof show - 1);
    }
}

void enterRawMode() {
    if (!isatty(STDIN_FILENO)) die("stdin is not a terminal (use --script for non-interactive runs)");
    tcgetattr(STDIN_FILENO, &savedTermios);
    termios raw = savedTermios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    rawMode = true;
    static const char hide[] = "\x1b[?25l\x1b[2J";
    (void)!write(STDOUT_FILENO, hide, sizeof hide - 1);
}

// ---- child processes --------------------------------------------------------
// Children run in their own process group so a compile (clang + ld) can be
// killed as a unit.
std::vector<pid_t> liveGroups;

void forgetGroup(pid_t pg) {
    for (size_t i = 0; i < liveGroups.size(); ++i)
        if (liveGroups[i] == pg) { liveGroups.erase(liveGroups.begin() + i); return; }
}

void killAllChildren() {
    for (pid_t pg : liveGroups) kill(-pg, SIGKILL);
}

void onSignal(int sig) {
    // Async-signal-safe cleanup only.
    for (pid_t pg : liveGroups) kill(-pg, SIGKILL);
    if (rawMode) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &savedTermios);
        static const char show[] = "\x1b[?25h\n";
        (void)!write(STDOUT_FILENO, show, sizeof show - 1);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

void die(const char* fmt, ...) {
    killAllChildren();
    restoreTerminal();
    va_list ap;
    va_start(ap, fmt);
    std::fputs("driver: ", stderr);
    std::vfprintf(stderr, fmt, ap);
    std::fputc('\n', stderr);
    va_end(ap);
    logf("fatal");
    std::exit(1);
}

std::vector<char*> cargv(std::vector<std::string>& args) {
    std::vector<char*> v;
    for (auto& a : args) v.push_back(a.data());
    v.push_back(nullptr);
    return v;
}

// Run argv to completion; capture stdout into *out (if non-null). Returns exit status.
int runCapture(std::vector<std::string> args, std::string* out) {
    int fds[2];
    if (pipe(fds) != 0) die("pipe: %s", std::strerror(errno));
    pid_t pid = fork();
    if (pid < 0) die("fork: %s", std::strerror(errno));
    if (pid == 0) {
        setpgid(0, 0);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        close(fds[1]);
        auto v = cargv(args);
        execvp(v[0], v.data());
        _exit(127);
    }
    setpgid(pid, pid);
    liveGroups.push_back(pid);
    close(fds[1]);
    char buf[4096];
    ssize_t n;
    while ((n = read(fds[0], buf, sizeof buf)) > 0 || (n < 0 && errno == EINTR))
        if (n > 0 && out) out->append(buf, n);
    close(fds[0]);
    int st = 0;
    while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {}
    forgetGroup(pid);
    return WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
}

// ---- files ------------------------------------------------------------------
void mkdirP(const std::string& path) {
    std::string cur;
    for (size_t i = 0; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            if (!cur.empty() && mkdir(cur.c_str(), 0755) != 0 && errno != EEXIST)
                die("mkdir %s: %s", cur.c_str(), std::strerror(errno));
        }
        if (i < path.size()) cur += path[i];
    }
}

// Remove a build directory (flat: it only ever holds files we or clang wrote).
void removeDir(const std::string& path) {
    DIR* d = opendir(path.c_str());
    if (!d) return;
    while (dirent* e = readdir(d)) {
        if (!std::strcmp(e->d_name, ".") || !std::strcmp(e->d_name, "..")) continue;
        unlink((path + "/" + e->d_name).c_str());
    }
    closedir(d);
    rmdir(path.c_str());
}

void copyFile(const std::string& from, const std::string& to) {
    FILE* in = std::fopen(from.c_str(), "rb");
    if (!in) die("open %s: %s", from.c_str(), std::strerror(errno));
    FILE* o = std::fopen(to.c_str(), "wb");
    if (!o) die("open %s: %s", to.c_str(), std::strerror(errno));
    char buf[8192];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, in)) > 0) std::fwrite(buf, 1, n, o);
    std::fclose(in);
    if (std::fclose(o) != 0) die("write %s", to.c_str());
}

// ---- the compiler -----------------------------------------------------------
std::string workDir;
const std::string bestDir = "tmp/best";  // holds best.hpp, written by B best

// clang++ invocation that turns DIR/state.hpp into DIR/B.
std::vector<std::string> compileCommand(const std::string& dir, bool initial) {
    std::vector<std::string> a = {"clang++", "-std=c++20", "-O0", "-I" + dir};
    // Interactive games see the saved best score; replays never do, so they stay deterministic.
    if (!opt.scripted) a.push_back("-I" + bestDir);
    if (initial) a.push_back("-DTS_SEED=" + std::to_string(opt.seed));
    if (opt.gridW) {
        a.push_back("-DTS_GRID_W=" + std::to_string(opt.gridW));
        a.push_back("-DTS_GRID_H=" + std::to_string(opt.gridH));
    }
    a.push_back(opt.root + "/frame.cpp");
    a.push_back("-o");
    a.push_back(dir + "/B");
    return a;
}

void compileSync(const std::string& dir, bool initial) {
    double t0 = nowMs();
    std::string ignored;
    int st = runCapture(compileCommand(dir, initial), &ignored);
    if (st != 0) die("compile failed in %s (status %d)", dir.c_str(), st);
    logf("compile %s: %.1f ms", dir.c_str(), nowMs() - t0);
}

// ---- the frame binary's outputs ----------------------------------------------
struct Info {
    int keys[5];
    bool terminal;
    int tickMs;  // the tick length the game asks for
};

// Parse "keys a b c d e\nterminal t\n". Pure lookup data, no interpretation.
Info readInfo(const std::string& dir) {
    std::string out;
    if (runCapture({dir + "/B", "info"}, &out) != 0) die("B info failed in %s", dir.c_str());
    Info in{};
    int t = 0;
    if (std::sscanf(out.c_str(), "keys %d %d %d %d %d terminal %d tick %d", &in.keys[0], &in.keys[1],
                    &in.keys[2], &in.keys[3], &in.keys[4], &t, &in.tickMs) != 7)
        die("bad info output: %s", out.c_str());
    for (int k : in.keys)
        if (k < 0 || k > 2) die("bad slot in info output: %s", out.c_str());
    in.terminal = t != 0;
    return in;
}

std::string readFrame(const std::string& dir) {
    std::string out;
    if (runCapture({dir + "/B", "frame"}, &out) != 0) die("B frame failed in %s", dir.c_str());
    return out;
}

void emitSuccessors(const std::string& binDir, const std::string& outDir) {
    if (runCapture({binDir + "/B", "emit", outDir}, nullptr) != 0) die("B emit failed in %s", binDir.c_str());
}

// ---- input --------------------------------------------------------------------
// Key indices match the order of the key table: none, Up, Down, Left, Right.
enum { KNone = 0, KUp = 1, KDown = 2, KLeft = 3, KRight = 4, KQuit = -1 };

// Decode raw bytes into key indices; returns the last key seen (or KNone).
// Handles arrow escape sequences (ESC [ A..D) and WASD.
struct KeyDecoder {
    int state = 0;  // 0 normal, 1 got ESC, 2 got ESC [
    int last = KNone;
    bool quit = false;
    bool restart = false;  // only acted on at the game-over screen
    void feed(unsigned char c) {
        if (state == 1) { state = (c == '[' || c == 'O') ? 2 : 0; return; }
        if (state == 2) {
            state = 0;
            switch (c) {
                case 'A': last = KUp; break;
                case 'B': last = KDown; break;
                case 'C': last = KRight; break;
                case 'D': last = KLeft; break;
            }
            return;
        }
        switch (c) {
            case 27: state = 1; break;
            case 'w': case 'W': last = KUp; break;
            case 's': case 'S': last = KDown; break;
            case 'a': case 'A': last = KLeft; break;
            case 'd': case 'D': last = KRight; break;
            case 'q': case 'Q': case 3: quit = true; break;
            case 'r': case 'R': restart = true; break;
        }
    }
};
KeyDecoder decoder;

// Wait until the deadline, collecting keys. Calls onIdle(remainingMs) between polls.
template<class F> void collectInput(double deadline, F onWake) {
    for (;;) {
        double left = deadline - nowMs();
        if (left <= 0 || decoder.quit) return;
        pollfd p{STDIN_FILENO, POLLIN, 0};
        int timeout = (int)left + 1;
        if (timeout > 5) timeout = 5;  // wake often enough to reap compiles
        int r = poll(&p, 1, timeout);
        if (r > 0 && (p.revents & POLLIN)) {
            unsigned char buf[64];
            ssize_t n = read(STDIN_FILENO, buf, sizeof buf);
            for (ssize_t i = 0; i < n; ++i) decoder.feed(buf[i]);
        }
        onWake();
    }
}

int scriptKey(int tick) {
    if (tick >= (int)opt.script.size()) return KNone;
    switch (opt.script[tick]) {
        case 'U': case 'u': return KUp;
        case 'D': case 'd': return KDown;
        case 'L': case 'l': return KLeft;
        case 'R': case 'r': return KRight;
        default: return KNone;
    }
}

// ---- drawing ----------------------------------------------------------------
int stalls = 0;
double worstStallMs = 0;

// This tick's length: the game's requested speed, or slower if compiles need it.
int tickLength(const Info& info) {
    return info.tickMs > opt.floorMs ? info.tickMs : opt.floorMs;
}

// At game over: the binary writes the updated best score (it decides the value).
void saveBest(const std::string& binDir) {
    if (opt.scripted) return;
    mkdirP(bestDir);
    std::string tmp = bestDir + "/best.hpp.new";
    if (runCapture({binDir + "/B", "best", tmp}, nullptr) != 0) die("B best failed in %s", binDir.c_str());
    if (std::rename(tmp.c_str(), (bestDir + "/best.hpp").c_str()) != 0)
        die("rename %s: %s", tmp.c_str(), std::strerror(errno));
}

int shownTickMs = 0;

// over: the game has ended; the status line offers restart instead of controls.
void draw(int tick, const std::string& frame, bool over = false) {
    if (opt.scripted) {
        std::printf("--- tick %d\n%s", tick, frame.c_str());
        std::fflush(stdout);
        return;
    }
    std::string s = "\x1b[H";
    s += frame;
    char status[160];
    if (over)
        std::snprintf(status, sizeof status, "  [r] play again   [q] quit\x1b[K\n\x1b[J");
    else
        std::snprintf(status, sizeof status,
                      " tick %d (%d ms)  stalls %d (worst %.0f ms)  [arrows/WASD, q quits]\x1b[K\n\x1b[J",
                      tick, shownTickMs, stalls, worstStallMs);
    s += status;
    // Frames use '\n'; raw mode keeps OPOST so the terminal still returns the carriage.
    (void)!write(STDOUT_FILENO, s.data(), s.size());
}

// At the game-over screen: block until r (true) or q / Ctrl-C (false).
bool waitForRestart() {
    decoder.restart = false;
    while (!decoder.restart && !decoder.quit) {
        pollfd p{STDIN_FILENO, POLLIN, 0};
        if (poll(&p, 1, 100) > 0 && (p.revents & POLLIN)) {
            unsigned char buf[64];
            ssize_t n = read(STDIN_FILENO, buf, sizeof buf);
            for (ssize_t i = 0; i < n; ++i) decoder.feed(buf[i]);
        }
    }
    return !decoder.quit;
}

// The finished game's binary writes the next game's first state (it picks the seed).
void writeRestartState(const std::string& binDir, const std::string& newDir) {
    mkdirP(newDir);
    if (runCapture({binDir + "/B", "restart", newDir + "/state.hpp"}, nullptr) != 0)
        die("B restart failed in %s", binDir.c_str());
}

std::string tickDir(int tick, int slot) {
    return workDir + "/t" + std::to_string(tick) + (slot < 0 ? std::string("") : "s" + std::to_string(slot));
}

// ---- naive loop: compile the chosen successor after the tick ends -----------
int runNaive() {
    std::string cur = tickDir(0, 0);
    mkdirP(cur);
    copyFile(opt.root + "/initial_state.hpp", cur + "/state.hpp");
    compileSync(cur, true);

    for (int tick = 0, gameStart = 0;; ++tick) {
        double tickStart = nowMs();
        Info info = readInfo(cur);
        shownTickMs = tickLength(info);
        draw(tick - gameStart, readFrame(cur), info.terminal && !opt.scripted);
        if (info.terminal) {
            saveBest(cur);
            if (opt.scripted || !waitForRestart()) return 0;
            std::string next = tickDir(tick + 1, 0);
            writeRestartState(cur, next);
            compileSync(next, false);
            removeDir(cur);
            cur = next;
            gameStart = tick + 1;
            continue;
        }
        if (opt.scripted && tick >= (int)opt.script.size()) return 0;

        std::string emitDir = tickDir(tick, -1);
        mkdirP(emitDir);
        emitSuccessors(cur, emitDir);

        int key;
        if (opt.scripted) {
            if (!opt.noDelay) collectInput(tickStart + tickLength(info), [] {});
            key = scriptKey(tick);
        } else {
            decoder.last = KNone;
            collectInput(tickStart + tickLength(info), [] {});
            if (decoder.quit) return 0;
            key = decoder.last;
        }
        int slot = info.keys[key];

        std::string next = tickDir(tick + 1, slot);
        mkdirP(next);
        copyFile(emitDir + "/succ" + std::to_string(slot) + ".hpp", next + "/state.hpp");
        double t0 = nowMs();
        compileSync(next, false);
        double stall = nowMs() - t0;
        if (!opt.noDelay) {
            ++stalls;
            if (stall > worstStallMs) worstStallMs = stall;
            logf("stall tick %d: %.1f ms (naive compile)", tick, stall);
        }
        removeDir(emitDir);
        removeDir(cur);
        cur = next;
    }
}

// ---- speculative loop: compile all 3 successors during the tick -------------
// One job per slot: a supervisor process (own process group) that compiles
// DIR/state.hpp into DIR/B and then runs B once with no arguments, so the
// first-exec cost (macOS scans new executables) is paid during the tick.
struct Job {
    pid_t pid = -1;
    std::string dir;
    double started = 0, finished = 0;
    bool done = false;
    int status = 0;
};

Job spawnJob(const std::string& dir, bool initial) {
    Job j;
    j.dir = dir;
    j.started = nowMs();
    auto cmd = compileCommand(dir, initial);
    std::string bin = dir + "/B";
    pid_t pid = fork();
    if (pid < 0) die("fork: %s", std::strerror(errno));
    if (pid == 0) {
        setpgid(0, 0);
        for (int s : {SIGINT, SIGTERM, SIGHUP, SIGQUIT}) signal(s, SIG_DFL);
        setenv("TMPDIR", dir.c_str(), 1);  // killed compiles leave temp files here
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) dup2(devnull, STDOUT_FILENO);
        pid_t c = fork();
        if (c < 0) _exit(126);
        if (c == 0) {
            auto v = cargv(cmd);
            execvp(v[0], v.data());
            _exit(127);
        }
        int st = 0;
        while (waitpid(c, &st, 0) < 0 && errno == EINTR) {}
        if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) _exit(1);
        // Record when the compile ended, for the timing log.
        if (FILE* f = std::fopen((dir + "/compiled_at").c_str(), "w")) {
            std::fprintf(f, "%.3f\n", nowMs());
            std::fclose(f);
        }
        execl(bin.c_str(), bin.c_str(), (char*)nullptr);  // warm run
        _exit(127);
    }
    setpgid(pid, pid);
    liveGroups.push_back(pid);
    j.pid = pid;
    return j;
}

void finishJob(Job& j, int st) {
    j.done = true;
    j.finished = nowMs();
    j.status = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + WTERMSIG(st);
    forgetGroup(j.pid);
}

void reapJobs(Job* jobs, int n) {
    for (int i = 0; i < n; ++i) {
        if (jobs[i].done || jobs[i].pid < 0) continue;
        int st;
        if (waitpid(jobs[i].pid, &st, WNOHANG) == jobs[i].pid) finishJob(jobs[i], st);
    }
}

void waitJob(Job& j) {
    if (j.done) return;
    int st = 0;
    while (waitpid(j.pid, &st, 0) < 0 && errno == EINTR) {}
    finishJob(j, st);
}

void cancelJob(Job& j) {
    if (j.done || j.pid < 0) return;
    kill(-j.pid, SIGKILL);
    waitJob(j);
}

// ---- tick calibration ---------------------------------------------------------
// The tick has to cover one tick's work: a few ms of B frame/info/emit, then
// three compiles in parallel, each followed by its first run. On macOS that
// first run is slow and serialized (new executables are scanned), so the cost
// differs a lot between machines and settings. Measure it instead of guessing.
constexpr int MaxTickMs = 1000;  // beyond this the game is unplayable anyway
constexpr int CalibrationRounds = 2;
constexpr double TickMargin = 1.1;
constexpr double PreWorkMs = 25;  // B frame + info + emit before compiles start

// Time one round: three jobs on the initial state, until the last is warm.
// The job in keepDir (if any) is left built for the game to start from.
double calibrationRound(int round, const std::string& keepDir) {
    std::string dirs[3];
    Job jobs[3];
    double t0 = nowMs();
    for (int i = 0; i < 3; ++i) {
        dirs[i] = (i == 0 && !keepDir.empty()) ? keepDir
                  : workDir + "/cal" + std::to_string(round) + "_" + std::to_string(i);
        mkdirP(dirs[i]);
        copyFile(opt.root + "/initial_state.hpp", dirs[i] + "/state.hpp");
        jobs[i] = spawnJob(dirs[i], true);
    }
    for (auto& j : jobs) {
        waitJob(j);
        if (j.status != 0) die("compile failed in %s (status %d)", j.dir.c_str(), j.status);
    }
    double t = nowMs() - t0;
    for (int i = 0; i < 3; ++i)
        if (dirs[i] != keepDir) removeDir(dirs[i]);
    logf("calibration round %d: 3 compiles + first runs in %.1f ms", round, t);
    return t;
}

// Returns the tick floor; leaves keepDir/B built from the initial state.
int calibrate(const std::string& keepDir, double* measured) {
    double worst = 0;
    for (int r = 0; r < CalibrationRounds; ++r) {
        double t = calibrationRound(r, r == 0 ? keepDir : "");
        if (t > worst) worst = t;
    }
    int tick = (int)(worst * TickMargin + PreWorkMs);
    tick = (tick + 9) / 10 * 10;  // round up to 10 ms
    if (tick > MaxTickMs) tick = MaxTickMs;
    if (measured) *measured = worst;
    logf("calibrated tick floor: %d ms (slowest round %.1f ms)", tick, worst);
    return tick;
}

int runSpeculative() {
    std::string cur = tickDir(0, 0);
    mkdirP(cur);
    copyFile(opt.root + "/initial_state.hpp", cur + "/state.hpp");
    if (opt.floorMs < 0) {
        if (!opt.scripted) {
            static const char msg[] = "\x1b[Hmeasuring compile speed...\x1b[K";
            (void)!write(STDOUT_FILENO, msg, sizeof msg - 1);
        }
        opt.floorMs = calibrate(cur, nullptr);
    } else {
        Job first = spawnJob(cur, true);
        waitJob(first);
        if (first.status != 0) die("initial compile failed (status %d)", first.status);
        logf("initial compile+warm: %.1f ms", first.finished - first.started);
    }

    for (int tick = 0, gameStart = 0;; ++tick) {
        double tickStart = nowMs();
        // 1-2. draw, read key table and terminal flag
        Info info = readInfo(cur);
        shownTickMs = tickLength(info);
        draw(tick - gameStart, readFrame(cur), info.terminal && !opt.scripted);
        if (info.terminal) {
            saveBest(cur);
            if (opt.scripted || !waitForRestart()) return 0;
            std::string next = tickDir(tick + 1, 0);
            writeRestartState(cur, next);
            Job j = spawnJob(next, false);
            waitJob(j);
            if (j.status != 0) die("compile failed in %s (status %d)", next.c_str(), j.status);
            removeDir(cur);
            cur = next;
            gameStart = tick + 1;
            logf("restart at tick %d", tick);
            continue;
        }
        if (opt.scripted && tick >= (int)opt.script.size()) return 0;

        // 3. successor headers
        std::string emitDir = tickDir(tick, -1);
        mkdirP(emitDir);
        emitSuccessors(cur, emitDir);

        // 4. compile all three in parallel
        Job jobs[3];
        for (int s = 0; s < 3; ++s) {
            std::string d = tickDir(tick + 1, s);
            mkdirP(d);
            copyFile(emitDir + "/succ" + std::to_string(s) + ".hpp", d + "/state.hpp");
            jobs[s] = spawnJob(d, false);
        }
        double spawned = nowMs();

        // 5. collect input until the tick ends
        int key;
        if (opt.scripted) {
            if (!opt.noDelay) collectInput(tickStart + tickLength(info), [&] { reapJobs(jobs, 3); });
            key = scriptKey(tick);
        } else {
            decoder.last = KNone;
            collectInput(tickStart + tickLength(info), [&] { reapJobs(jobs, 3); });
            if (decoder.quit) {
                for (auto& j : jobs) cancelJob(j);
                return 0;
            }
            key = decoder.last;
        }

        // 6. table lookup
        int slot = info.keys[key];

        // 7. wait for the chosen compile, drop the others
        Job& chosen = jobs[slot];
        double waitStart = nowMs();
        bool wasReady = chosen.done;
        waitJob(chosen);
        double stall = nowMs() - waitStart;
        for (int s = 0; s < 3; ++s)
            if (s != slot) cancelJob(jobs[s]);
        if (chosen.status != 0) die("compile failed in %s (status %d)", chosen.dir.c_str(), chosen.status);
        double compiledAt = chosen.finished;
        if (FILE* f = std::fopen((chosen.dir + "/compiled_at").c_str(), "r")) {
            if (std::fscanf(f, "%lf", &compiledAt) != 1) compiledAt = chosen.finished;
            std::fclose(f);
        }
        logf("tick %d (%d ms): pre %.1f ms, compile %.1f ms, warm %.1f ms, slot %d%s", tick,
             tickLength(info), spawned - tickStart, compiledAt - chosen.started,
             chosen.finished - compiledAt, slot,
             wasReady ? "" : " (waited)");
        if (!wasReady && !opt.noDelay) {
            ++stalls;
            if (stall > worstStallMs) worstStallMs = stall;
            logf("STALL tick %d: waited %.1f ms for slot %d", tick, stall, slot);
        }

        // 8. advance; remove everything from the previous tick
        removeDir(emitDir);
        removeDir(cur);
        for (int s = 0; s < 3; ++s)
            if (s != slot) removeDir(jobs[s].dir);
        cur = chosen.dir;
    }
}

// Remove run directories left by drivers that died without cleaning up
// (signals and crashes skip normal cleanup; the handler must stay async-safe).
void sweepStaleRuns() {
    DIR* d = opendir("tmp");
    if (!d) return;
    while (dirent* e = readdir(d)) {
        if (std::strncmp(e->d_name, "run-", 4) != 0) continue;
        pid_t pid = (pid_t)std::atoi(e->d_name + 4);
        if (pid <= 0 || (kill(pid, 0) != 0 && errno == ESRCH)) {
            std::string run = std::string("tmp/") + e->d_name;
            if (DIR* r = opendir(run.c_str())) {
                while (dirent* t = readdir(r))
                    if (t->d_name[0] != '.') removeDir(run + "/" + t->d_name);
                closedir(r);
            }
            rmdir(run.c_str());
            logf("swept stale %s", run.c_str());
        }
    }
    closedir(d);
}

void usage() {
    std::fputs("usage: driver [--tick MS] [--calibrate] [--seed N] [--script KEYS] [--no-delay] [--naive]\n"
               "              [--grid WxH] [--root DIR] [--log FILE]\n", stderr);
    std::exit(2);
}

} // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto val = [&]() -> std::string { if (i + 1 >= argc) usage(); return argv[++i]; };
        if (a == "--tick") opt.floorMs = std::atoi(val().c_str());
        else if (a == "--calibrate") opt.calibrateOnly = true;
        else if (a == "--seed") opt.seed = (unsigned)std::strtoul(val().c_str(), nullptr, 10);
        else if (a == "--script") { opt.scripted = true; opt.script = val(); }
        else if (a == "--no-delay") opt.noDelay = true;
        else if (a == "--naive") opt.naive = true;
        else if (a == "--root") opt.root = val();
        else if (a == "--grid") {
            if (std::sscanf(val().c_str(), "%dx%d", &opt.gridW, &opt.gridH) != 2) usage();
            if (opt.gridW < 3 || opt.gridH < 2 || opt.gridW > 63 || opt.gridH > 63) usage();
        }
        else if (a == "--log") opt.log = val();
        else usage();
    }
    if (opt.floorMs < -1) usage();
    // Timing doesn't matter for these; skip measuring.
    if (opt.floorMs < 0 && (opt.naive || opt.noDelay)) opt.floorMs = 0;

    std::string logDir = opt.log.substr(0, opt.log.find_last_of('/'));
    if (logDir != opt.log) mkdirP(logDir);
    logFile = std::fopen(opt.log.c_str(), "a");
    sweepStaleRuns();
    workDir = "tmp/run-" + std::to_string(getpid());
    mkdirP(workDir);
    logf("start pid %d tick %s seed %u %s", getpid(),
         opt.floorMs >= 0 ? (std::to_string(opt.floorMs) + " ms").c_str() : "auto", opt.seed,
         opt.naive ? "naive" : "speculative");

    if (opt.calibrateOnly) {
        std::string keep = tickDir(0, 0);
        mkdirP(keep);
        double measured = 0;
        int tick = calibrate(keep, &measured);
        removeDir(keep);
        rmdir(workDir.c_str());
        std::printf("one tick of work (3 parallel compiles + first runs): %.0f ms\n"
                    "tick floor: %d ms%s\n", measured, tick,
                    tick == MaxTickMs ? " (capped; expect stalls)" : "");
        return 0;
    }

    for (int s : {SIGINT, SIGTERM, SIGHUP, SIGQUIT, SIGSEGV, SIGBUS, SIGABRT}) signal(s, onSignal);
    std::atexit([] { killAllChildren(); restoreTerminal(); });
    if (!opt.scripted) enterRawMode();

    int rc = opt.naive ? runNaive() : runSpeculative();

    restoreTerminal();
    if (!opt.scripted && stalls) std::printf("stalls: %d (worst %.0f ms), see %s\n", stalls, worstStallMs, opt.log.c_str());
    // Clean up the whole run directory.
    if (DIR* d = opendir(workDir.c_str())) {
        while (dirent* e = readdir(d))
            if (e->d_name[0] != '.') removeDir(workDir + "/" + e->d_name);
        closedir(d);
    }
    rmdir(workDir.c_str());
    return rc;
}

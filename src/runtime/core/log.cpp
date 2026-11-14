#include <windows.h>

#include <atomic>
#include <cstring>
#include <mutex>

#include "runtime.h"

namespace vc2rt {

static FILE* g_logFile = nullptr;
static LogLevel g_level = LogLevel::Info;
static std::mutex g_logMtx;

// The last runs' logs are kept next to it: vc2recomp.log is this run, vc2recomp.1.log the one before,
// up to vc2recomp.3.log.
void logInit(const std::filesystem::path& file) {
    constexpr int kKeep = 3;
    std::error_code ec;
    auto older = [&](int n) {
        auto p = file;
        return p.replace_extension(std::to_string(n) + file.extension().string());
    };
    std::filesystem::remove(older(kKeep), ec);
    for (int n = kKeep - 1; n >= 1; n--) std::filesystem::rename(older(n), older(n + 1), ec);
    std::filesystem::rename(file, older(1), ec);
    g_logFile = _wfopen(file.c_str(), L"w");
}
void setLogLevel(LogLevel l) { g_level = l; }

static std::string g_watch;
static std::atomic<u32> g_watchHits{0};
void logWatch(const std::string& needle) { g_watch = needle; }
u32 logWatchHits() { return g_watchHits.load(); }

void logMsg(LogLevel lvl, const char* tag, const char* fmt, ...) {
    if (lvl < g_level) return;
    static const char* names[] = {"T", "D", "I", "W", "E"};
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (!g_watch.empty() && strstr(buf, g_watch.c_str())) g_watchHits++;
    std::lock_guard<std::mutex> lk(g_logMtx);
    double t = hostMicros() / 1e6;
    if (g_logFile) {
        fprintf(g_logFile, "[%9.3f] %s %-8s %s\n", t, names[(int)lvl], tag, buf);
        // Written out at least once a second, so a game that is killed (or hangs) keeps its log.
        static double lastFlush = 0;
        if (lvl >= LogLevel::Warn || t - lastFlush >= 1.0) {
            fflush(g_logFile);
            lastFlush = t;
        }
    }
    fprintf(stderr, "[%9.3f] %s %-8s %s\n", t, names[(int)lvl], tag, buf);
}

void fatal(const char* fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    logMsg(LogLevel::Error, "FATAL", "%s", buf);
    if (g_logFile) fflush(g_logFile);
    MessageBoxA(nullptr, buf, "Valkyria Chronicles Recomp - Fatal error", MB_ICONERROR | MB_OK);
    ExitProcess(1);
}

u64 hostMicros() {
    static LARGE_INTEGER freq, start;
    static bool init = false;
    if (!init) {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
        init = true;
    }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (u64)((now.QuadPart - start.QuadPart) * 1000000ull / freq.QuadPart);
}

}  // namespace vc2rt

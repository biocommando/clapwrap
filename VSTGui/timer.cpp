#include <windows.h>
#include <map>

static std::map<UINT_PTR, void *> _timers;

extern void clap_debug_logger(const char *ctx, const char *fmt, ...);
static void timer_callback(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    extern void time_interval_elapsed(void *);
    auto it = _timers.find(idEvent);
    if (it != _timers.end())
        time_interval_elapsed(it->second);
}

void timer_stop(void *state)
{
    for (auto &kv : _timers)
    {
        if (kv.second == state)
        {
            _timers.erase(kv.first);
            KillTimer(nullptr, kv.first);
        }
    }
}

void timer_start(unsigned int time, void *state)
{    
    auto timer = SetTimer(nullptr, 0, time, timer_callback);
    _timers.emplace(timer, state);
}

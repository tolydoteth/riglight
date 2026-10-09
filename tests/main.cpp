#include <cstdio>
#include <cstring>

#include "check.h"

int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int ran = 0, failed = 0;
    for (const check::Test& t : check::registry()) {
        if (filter && !std::strstr(t.name, filter)) continue;
        const int before = check::failures();
        t.fn();
        ++ran;
        const bool ok = check::failures() == before;
        if (!ok) ++failed;
        std::printf("%s  %s\n", ok ? "ok  " : "FAIL", t.name);
    }
    std::printf("\n%d tests, %d failed\n", ran, failed);
    return failed == 0 && ran > 0 ? 0 : 1;
}

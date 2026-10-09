#pragma once
#include <cstdio>
#include <string>
#include <vector>

namespace check {
struct Test {
    const char* name;
    void (*fn)();
};
inline std::vector<Test>& registry() {
    static std::vector<Test> r;
    return r;
}
inline int& failures() {
    static int f = 0;
    return f;
}
struct Reg {
    Reg(const char* n, void (*f)()) { registry().push_back({n, f}); }
};
}  // namespace check

#define TEST(name)                                   \
    static void name();                              \
    static const check::Reg reg_##name(#name, name); \
    static void name()

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            ++check::failures();                                            \
            std::printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                   \
    } while (0)

#define CHECK_EQ(a, b)                                                                  \
    do {                                                                                \
        const auto va_ = (a);                                                           \
        const auto vb_ = (b);                                                           \
        if (!(va_ == vb_)) {                                                            \
            ++check::failures();                                                        \
            std::printf("    FAIL %s:%d: %s == %s (%lld vs %lld)\n", __FILE__, __LINE__, \
                        #a, #b, (long long)va_, (long long)vb_);                        \
        }                                                                               \
    } while (0)

#define CHECK_STR(a, b)                                                                     \
    do {                                                                                    \
        const std::string va_(a);                                                           \
        const std::string vb_(b);                                                           \
        if (va_ != vb_) {                                                                   \
            ++check::failures();                                                            \
            std::printf("    FAIL %s:%d: %s == %s (\"%s\" vs \"%s\")\n", __FILE__, __LINE__, \
                        #a, #b, va_.c_str(), vb_.c_str());                                  \
        }                                                                                   \
    } while (0)

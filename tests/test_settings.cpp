#include "check.h"
#include "core/settings.h"

using namespace rl;

TEST(settings_defaults_when_empty) {
    const LightState s = parseSettings("");
    CHECK(s == LightState{});
    CHECK(s.on);
    CHECK(s.color == kWarmWhite);
    CHECK(s.effect == Effect::Static);
    CHECK_EQ(s.brightness, 60);
}

TEST(settings_round_trip) {
    LightState s;
    s.on = false;
    s.color = Rgb{0x12, 0xAB, 0xEF};
    s.effect = Effect::Rainbow;
    s.brightness = 35;
    CHECK(parseSettings(serializeSettings(s)) == s);
}

TEST(settings_serialized_format) {
    LightState s;
    CHECK_STR(serializeSettings(s),
              "[lights]\non=1\ncolor=FFB46B\neffect=static\nbrightness=60\n");
}

TEST(settings_garbage_falls_back_per_key) {
    const LightState s = parseSettings(
        "[lights]\r\non=maybe\r\ncolor=zzz\r\neffect=disco\r\nbrightness=abc\r\n");
    CHECK(s == LightState{});
}

TEST(settings_clamps_brightness) {
    CHECK_EQ(parseSettings("brightness=0").brightness, 5);
    CHECK_EQ(parseSettings("brightness=900").brightness, 100);
    CHECK_EQ(parseSettings("brightness=-4").brightness, 5);
    CHECK_EQ(parseSettings("brightness= 42 ").brightness, 42);
}

TEST(settings_handles_crlf_comments_and_case) {
    const LightState s = parseSettings(
        "; comment\r\n# other\r\n[lights]\r\nON=0\r\nColor=#00ff80\r\neffect=Breathing\r\n");
    CHECK(!s.on);
    CHECK(s.color == (Rgb{0x00, 0xFF, 0x80}));
    CHECK(s.effect == Effect::Breathing);
}

TEST(color_hex_parsing) {
    CHECK(parseColorHex("FF0000") == (Rgb{255, 0, 0}));
    CHECK(parseColorHex("#0a0B0c") == (Rgb{10, 11, 12}));
    CHECK(!parseColorHex("FF00"));
    CHECK(!parseColorHex("GG0000"));
    CHECK(!parseColorHex("FF00000"));
    CHECK_STR(colorHex(Rgb{1, 2, 255}), "0102FF");
}

TEST(settings_ram_lead_ms) {
    CHECK_EQ(parseRamLeadMs("[lights]\non=1\n"), kDefaultRamLeadMs);
    CHECK_EQ(parseRamLeadMs("ram_lead_ms=450\r\n"), 450);
    CHECK_EQ(parseRamLeadMs("ram_lead_ms=abc"), kDefaultRamLeadMs);
    CHECK_EQ(parseRamLeadMs("ram_lead_ms=-50"), 0);      // clamped
    CHECK_EQ(parseRamLeadMs("ram_lead_ms=99999"), 2000);  // clamped
    // the lights keys still parse, and serializing keeps a hand-set lead
    LightState s;
    CHECK(parseSettings("ram_lead_ms=450\non=0\n").on == false);
    CHECK_EQ(parseRamLeadMs(serializeSettings(s, 450)), 450);
    CHECK_EQ(parseRamLeadMs(serializeSettings(s)), kDefaultRamLeadMs);
}

TEST(state_spec_parsing) {
    const LightState base;
    auto off = parseStateSpec("off", base);
    CHECK(off && !off->on && off->color == base.color);
    auto on = parseStateSpec("on", *off);
    CHECK(on && on->on);
    auto st = parseStateSpec("static:FF0000:100", base);
    CHECK(st && st->on && st->effect == Effect::Static && st->color == (Rgb{255, 0, 0}) &&
          st->brightness == 100);
    auto br = parseStateSpec("breathing:00FF00", base);
    CHECK(br && br->effect == Effect::Breathing && br->color == (Rgb{0, 255, 0}) &&
          br->brightness == base.brightness);
    auto rb = parseStateSpec("rainbow:30", base);
    CHECK(rb && rb->effect == Effect::Rainbow && rb->brightness == 30 && rb->color == base.color);
    CHECK(!parseStateSpec("static:nothex", base));
    CHECK(!parseStateSpec("rainbow:abc", base));
    CHECK(!parseStateSpec("disco", base));
    CHECK(!parseStateSpec("", base));
}

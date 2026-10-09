#include "check.h"
#include "core/fusion2.h"

using namespace rl;
using namespace rl::fusion2;

static Packet bytes(std::initializer_list<int> head) {
    Packet p{};
    size_t i = 0;
    for (int b : head) p[i++] = (uint8_t)b;
    return p;
}

static LightState make(Effect e, Rgb c, int pct, bool on = true) {
    LightState s;
    s.on = on;
    s.effect = e;
    s.color = c;
    s.brightness = pct;
    return s;
}

TEST(fusion2_command_layout) {
    CHECK(command(0x28, 0xFF, 0x07) == bytes({0xCC, 0x28, 0xFF, 0x07}));
    CHECK(command(0x60) == bytes({0xCC, 0x60}));
}

TEST(fusion2_info_and_probe_steps_are_read_only) {
    const auto info = infoSteps();
    CHECK_EQ(info.size(), 1u);
    CHECK(info[0].bytes == bytes({0xCC, 0x60}));
    CHECK(info[0].readAfter);
    const auto probe = probeSteps();
    CHECK_EQ(probe.size(), 2u);
    CHECK(probe[0].bytes == bytes({0xCC, 0x60}));
    CHECK(probe[1].bytes == bytes({0xCC, 0x61}));
    CHECK(probe[0].readAfter && probe[1].readAfter);
}

static Packet sampleReport(uint8_t flag) {
    Packet p{};
    p[0] = 0xCC;
    p[2] = 0;                       // device_num
    p[3] = 1;                       // strip_detect
    p[4] = 0x01; p[5] = 0x02; p[6] = 0x03; p[7] = 0x04;  // fw
    p[8] = 0x22; p[9] = 0x02; p[10] = 0x00;              // argb count classes
    p[11] = flag;
    const char* name = "IT5711-GIGABYTE V3.5.14.0";
    for (int i = 0; name[i]; ++i) p[12 + i] = (uint8_t)name[i];
    p[56] = 0x78; p[57] = 0x56; p[58] = 0x34; p[59] = 0x12;  // chip id LE
    return p;
}

TEST(fusion2_parse_report) {
    const Report r = parseReport(sampleReport(0x03));
    CHECK(r.valid);
    CHECK_EQ(r.stripDetect, 1);
    CHECK_EQ(r.fw[3], 4);
    CHECK_EQ(r.argbCounts[0], 0x22);
    CHECK_EQ(r.supportCmdFlag, 3);
    CHECK(r.lampArray());
    CHECK_STR(r.product, "IT5711-GIGABYTE V3.5.14.0");
    CHECK_EQ(r.chipId, 0x12345678u);
    CHECK(!parseReport(sampleReport(0x01)).lampArray());
    Packet bad = sampleReport(0);
    bad[0] = 0x00;
    CHECK(!parseReport(bad).valid);
}

TEST(fusion2_parse_report_unterminated_name) {
    Packet p = sampleReport(0);
    for (int i = 12; i < 40; ++i) p[i] = 'A';
    CHECK_EQ(parseReport(p).product.size(), 28u);
}

TEST(fusion2_init_without_lamparray) {
    const auto steps = initSteps(parseReport(sampleReport(0x01)));
    CHECK_EQ(steps.size(), 14u);  // 8 + 3 resets, apply-all, beat off, headers to hw effects
    CHECK(steps[0].bytes == bytes({0xCC, 0x20}));
    CHECK(steps[7].bytes == bytes({0xCC, 0x27}));
    CHECK(steps[8].bytes == bytes({0xCC, 0x90}));
    CHECK(steps[10].bytes == bytes({0xCC, 0x92}));
    CHECK(steps[11].bytes == bytes({0xCC, 0x28, 0xFF, 0x07}));
    CHECK(steps[12].bytes == bytes({0xCC, 0x31, 0x00}));
    CHECK(steps[13].bytes == bytes({0xCC, 0x32, 0x00}));
    CHECK_EQ(steps[13].waitMs, 50);
    for (const Step& s : steps) CHECK(!s.readAfter);
}

TEST(fusion2_init_with_lamparray_first_disables_it) {
    const auto steps = initSteps(parseReport(sampleReport(0x02)));
    CHECK_EQ(steps.size(), 15u);
    CHECK(steps[0].bytes == bytes({0xCC, 0x48, 0x00}));
    CHECK(steps[1].bytes == bytes({0xCC, 0x20}));
}

TEST(fusion2_static_red_matches_protocol_doc) {
    // docs/fusion2-protocol.md §3 "Worked bytes": static red on ARGB_V2_1 at full brightness.
    const Packet p = effectPacket(kBoardZones[0], make(Effect::Static, Rgb{255, 0, 0}, 100));
    Packet want{};
    want[0] = 0xCC; want[1] = 0x25; want[2] = 0x20;
    want[11] = 0x01; want[12] = 0xFF;
    want[16] = 0xFF;  // color0 bytes 14..17 = B, G, R, 0
    CHECK(p == want);
}

TEST(fusion2_static_brightness_uses_max_field_not_color) {
    const Packet p = effectPacket(kBoardZones[0], make(Effect::Static, Rgb{10, 20, 30}, 60));
    CHECK_EQ(p[12], 153);  // round(60 * 255 / 100)
    CHECK_EQ(p[14], 30);
    CHECK_EQ(p[15], 20);
    CHECK_EQ(p[16], 10);
    CHECK_EQ(p[22], 0);  // period0 must be 0 for static
    CHECK_EQ(effectPacket(kBoardZones[0], make(Effect::Static, Rgb{1, 1, 1}, 5))[12], 13);
}

TEST(fusion2_breathing_packet) {
    const Packet p = effectPacket(kBoardZones[1], make(Effect::Breathing, Rgb{0, 0, 255}, 60));
    CHECK_EQ(p[1], 0x26);
    CHECK_EQ(p[2], 0x40);
    CHECK_EQ(p[11], 0x02);
    CHECK_EQ(p[12], 60);  // pulse brightness is a percent (controller quirk)
    CHECK_EQ(p[13], 0);
    CHECK_EQ(p[14], 0xFF);
    CHECK_EQ(p[22], 0xE8); CHECK_EQ(p[23], 0x03);  // 1000 ms
    CHECK_EQ(p[24], 0xE8); CHECK_EQ(p[25], 0x03);  // 1000 ms
    CHECK_EQ(p[26], 0xC8); CHECK_EQ(p[27], 0x00);  // 200 ms
    CHECK_EQ(p[30], 0);
}

TEST(fusion2_rainbow_packet) {
    const Packet p = effectPacket(kBoardZones[3], make(Effect::Rainbow, Rgb{9, 9, 9}, 100));
    CHECK_EQ(p[1], 0x22);
    CHECK_EQ(p[2], 0x04);
    CHECK_EQ(p[11], 0x04);
    CHECK_EQ(p[12], 0xFF);
    CHECK_EQ(p[14], 0); CHECK_EQ(p[15], 0); CHECK_EQ(p[16], 0);  // color unused
    CHECK_EQ(p[22], 0xBC); CHECK_EQ(p[23], 0x02);  // 700 ms
    CHECK_EQ(p[24], 0xF4); CHECK_EQ(p[25], 0x01);  // 500 ms
    CHECK_EQ(p[30], 7);
}

TEST(fusion2_off_packet) {
    const Packet p = effectPacket(kBoardZones[4], make(Effect::Rainbow, Rgb{255, 255, 255}, 100, false));
    Packet want{};
    want[0] = 0xCC; want[1] = 0x24; want[2] = 0x10;
    want[11] = 0x01;  // static, max_brightness 0, color 0, periods 0
    CHECK(p == want);
}

TEST(fusion2_apply_script) {
    const auto steps = applySteps(make(Effect::Static, Rgb{1, 2, 3}, 100));
    CHECK_EQ(steps.size(), 6u);
    const uint8_t regs[5] = {0x25, 0x26, 0x27, 0x22, 0x24};
    for (int i = 0; i < 5; ++i) CHECK_EQ(steps[i].bytes[1], regs[i]);
    CHECK(steps[5].bytes == bytes({0xCC, 0x28, 0xF4, 0x00, 0x00, 0x00}));
}

TEST(fusion2_frame_script_is_direct_static) {
    // Animated effects are driven frame by frame from RigLight's clock (same clock as the RAM):
    // one static packet per zone with the frame color, period0 = 0 ("direct"), then commit.
    const auto steps = frameSteps(Rgb{10, 20, 30});
    CHECK_EQ(steps.size(), 6u);
    const uint8_t regs[5] = {0x25, 0x26, 0x27, 0x22, 0x24};
    for (int i = 0; i < 5; ++i) {
        const Packet& p = steps[i].bytes;
        CHECK_EQ(p[1], regs[i]);
        CHECK_EQ(p[2], kBoardZones[i].mask);
        CHECK_EQ(p[11], kEffectStatic);
        CHECK_EQ(p[12], 0xFF);
        CHECK_EQ(p[14], 30); CHECK_EQ(p[15], 20); CHECK_EQ(p[16], 10);
        CHECK_EQ(p[22], 0); CHECK_EQ(p[23], 0);
        CHECK(!isForbidden(p));
        CHECK_EQ(steps[i].waitMs, 0);
    }
    CHECK(steps[5].bytes == bytes({0xCC, 0x28, 0xF4, 0x00, 0x00, 0x00}));
}

TEST(fusion2_zone_masks_or_to_commit_mask) {
    uint32_t m = 0;
    for (const Zone& z : kBoardZones) m |= z.mask;
    CHECK_EQ(m, kBoardZoneMask);
}

TEST(fusion2_never_emits_flash_commands) {
    CHECK(isForbidden(command(0x5E)));
    CHECK(isForbidden(command(0x47, 1)));
    CHECK(isForbidden(command(0x33)));
    CHECK(isForbidden(command(0x34)));
    CHECK(!isForbidden(command(0x28, 0xFF, 0x07)));
    std::vector<Step> all = probeSteps();
    for (uint8_t flag : {0x00, 0x01, 0x02, 0x03}) {
        auto v = initSteps(parseReport(sampleReport(flag)));
        all.insert(all.end(), v.begin(), v.end());
    }
    for (Effect e : {Effect::Static, Effect::Breathing, Effect::Rainbow})
        for (bool on : {true, false})
            for (int pct : {5, 60, 100}) {
                auto v = applySteps(make(e, Rgb{255, 128, 0}, pct, on));
                all.insert(all.end(), v.begin(), v.end());
            }
    for (const Step& s : all) {
        CHECK(!isForbidden(s.bytes));
        CHECK_EQ(s.bytes[0], 0xCC);
    }
}

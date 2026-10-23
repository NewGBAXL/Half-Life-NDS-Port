// The head-up display's places (shared/HlHudLayout.hpp): the game's own sums
// at 640 by 480, brought to 256 by 192, checked against the same sums done by
// hand from its cl_dll; the pictures of the kinds of hurt, stacked and timed
// as the game has them; and the kinds of hurt kept with the player without
// being any part of a save.

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "HlHudLayout.hpp"
#include "HlSaveState.hpp"
#include "TestBspBuilder.hpp"

using namespace hldata;
using namespace hldstest;

namespace
{
    void testPlaces()
    {
        // health.cpp: y = 480 - 24 - 12 = 444; the cross at x = 16; the
        // number at x = 32 + 10 = 42, y = 444 + 4; the bar at 42 + 60 + 10.
        check(hudDs(kHud640CrossX) == 6 && hudDs(kHud640RowY) == 178, "the cross is at (6, 178)");
        check(hudDs(kHud640HealthX) == 17 && hudDs(kHud640HealthX + kHud640DigitStep) == 25 &&
                  hudDs(kHud640HealthX + 2 * kHud640DigitStep) == 33 && hudDs(kHud640NumberY) == 179,
              "... health's three digits at 17, 25, 33, a row lower");
        check(hudDs(kHud640HealthBarX) == 45 && hudDs(kHud640BarWidth) == 1 && hudDs(kHud640FontHeight) == 10,
              "... and its bar at 45, one wide and a digit high");
        // battery.cpp: x = 3 * 40, y = 444 - 40 / 6; the number at 160.
        check(hudDs(kHud640SuitX) == 48 && hudDs(kHud640SuitY) == 175 && hudDs(kHud640ArmorX) == 64,
              "the suit is at (48, 175), armour's number at 64");
        // ammo.cpp: 640 - 160 - 24 = 456 for the clip; its bar at 526; what
        // is carried at 538; the icon at 598, 445. Without a clip, 536 and
        // the icon at 596; the grenades at y 448 - 30.
        check(hudDs(kHud640ClipX) == 182 && hudDs(kHud640ClipBarX) == 210 && hudDs(kHud640SpareX) == 215 &&
                  hudDs(hud640AmmoIconX(kHud640SpareX)) == 239 && hudDs(kHud640NumberY - kHud640AmmoIconUp) == 178,
              "a clip at 182, its bar at 210, the rest at 215, the icon at (239, 178)");
        check(hudDs(kHud640CountX) == 214 && hudDs(hud640AmmoIconX(kHud640CountX)) == 238 &&
                  hudDs(kHud640SecondY) == 167 && hudDs(kHud640SecondY - kHud640AmmoIconUp) == 166,
              "a count without a clip at 214 (icon 238); the second kind a row up, at 167");
        check(hud640AmmoIconX(kHud640SpareX) + kHud640AmmoSide <= kHud640Width - 16,
              "... the icon in from the right edge, as the game has it");
        // flashlight.cpp: 640 - 32 - 16 = 592, y = 32 / 2; the beam at 624.
        check(hudDs(kHud640LampX) == 237 && hudDs(kHud640LampY) == 6 && hudDs(kHud640BeamX) == 250 &&
                  hudDs(kHud640BeamX) + kHud640LampBeam.w == 256,
              "the lamp at (237, 6), its beam at 250 to the edge");
        // health.cpp's DrawPain: (256, 96), (416, 176), (256, 336), (176, 176).
        check(hudDs(kHud640PainAt[0].x) == 102 && hudDs(kHud640PainAt[0].y) == 38 &&
                  hudDs(kHud640PainAt[1].x) == 166 && hudDs(kHud640PainAt[1].y) == 70 &&
                  hudDs(kHud640PainAt[2].x) == 102 && hudDs(kHud640PainAt[2].y) == 134 &&
                  hudDs(kHud640PainAt[3].x) == 70 && hudDs(kHud640PainAt[3].y) == 70,
              "the marks of where a hurt came from, round the middle");
        // UpdateTiles: (64 / 8, 480 - 128), each older 64 up.
        check(hudDs(hud640HurtAt(0).x) == 3 && hudDs(hud640HurtAt(0).y) == 141 && hudDs(hud640HurtAt(1).y) == 115 &&
                  hudDs(hud640HurtAt(2).y) == 90,
              "the pictures of kinds of hurt at the left, stacked up from 141");
        check(hudDs(kHud640CrosshairX) == 123 && hudDs(kHud640CrosshairY) == 91 &&
                  hudDs(kHud640CrosshairX) + kHud640Crosshair.w / 2 == 128,
              "the crosshair in the middle");
        // What is drawn of a number, and of the suit and the lamp.
        check(hudFieldFirst(0) == 2 && hudFieldFirst(9) == 2 && hudFieldFirst(10) == 1 && hudFieldFirst(100) == 0,
              "a number leaves out its leading zeros, and shows 0");
        check(hudSuitEmptyRows(100) == 0 && hudSuitEmptyRows(0) == kHud640SuitFull.h && hudSuitEmptyRows(50) == 8 &&
                  hudSuitEmptyRows(200) == 0,
              "the suit fills from the bottom as far as there is armour");
        check(hudLampEmptyColumns(100) == 0 && hudLampEmptyColumns(0) == kHud640LampFull.w &&
                  hudLampEmptyColumns(50) == 6,
              "the lamp empties from its left");
        // The 640 pictures, 0.4 of their size.
        check(kHud640DigitWidth == hudDs(kHud640DigitStep) && kHud640DigitHeight == hudDs(kHud640FontHeight) &&
                  kHud640Cross.w == hudDs(kHud640CrossSide) && kHud640SuitFull.w == hudDs(kHud640SuitSide) &&
                  hud640Ammo(0).w == hudDs(kHud640AmmoSide) && hud640Hurt(0).w == hudDs(kHud640HurtSide) &&
                  kHud640Pain[0].w == hudDs(kHud640PainLong) && kHud640Pain[0].h == hudDs(kHud640PainShort) &&
                  kHud640Crosshair.w == hudDs(kHud640CrosshairSide) && kHud640LampFull.w == hudDs(kHud640LampSide),
              "the pictures are 0.4 of the game's 640 set");
    }

    void testHurts()
    {
        check(HudHurts::pulse(0) == 0 && HudHurts::pulse(785) > 250 && HudHurts::pulse(1571) < 8,
              "the pictures pulse as |sin 2t|: dark, bright at a quarter turn, dark again");
        HudHurts hurts;
        check(!hurts.any(), "none shown at first");
        hurts.add(kHurtRadiation, 0);
        hurts.add(kHurtDrown, 100);
        check(hurts.stacked[6] == 1 && hurts.stacked[3] == 0 && hurts.any(),
              "the newest kind at the bottom, the one before moved up");
        hurts.add(kHurtRadiation, 500);
        check(hurts.stacked[6] == 1 && hurts.stacked[3] == 0, "hurt again by a kind shown: it stays where it is");
        hurts.tick(2050, 10);
        check(hurts.stacked[6] == 1 && hurts.stacked[3] == 0, "... and stays two seconds past its last");
        hurts.tick(2150, 200);
        check(hurts.stacked[3] == 0, "one whose time is up waits for a dark moment");
        hurts.tick(2150, 10);
        check(hurts.stacked[3] == -1 && hurts.stacked[6] == 0, "... then goes, and the one above moves down");
        hurts.tick(2600, 10);
        check(!hurts.any(), "... until none are left");
        hurts.add(kHurtBurn | kHurtShock, 3000);
        check(hurts.stacked[4] >= 0 && hurts.stacked[7] >= 0 && hurts.stacked[4] != hurts.stacked[7],
              "two kinds at once each have a place");
    }

    void testKinds()
    {
        // The game's DMG_* (cbase.h).
        check(hurtKindsOf(1u << 18) == kHurtRadiation && hurtKindsOf(1u << 17) == kHurtPoison &&
                  hurtKindsOf(1u << 20) == kHurtAcid && hurtKindsOf(1u << 22) == kHurtFreeze &&
                  hurtKindsOf(1u << 4) == kHurtFreeze && hurtKindsOf(1u << 3) == kHurtBurn &&
                  hurtKindsOf(1u << 21) == kHurtBurn && hurtKindsOf(1u << 14) == kHurtDrown &&
                  hurtKindsOf(1u << 16) == kHurtGas && hurtKindsOf(1u << 8) == kHurtShock,
              "each of the game's kinds the display shows has its picture");
        check(hurtKindsOf(0) == 0 && hurtKindsOf(1u << 1) == 0 && hurtKindsOf(1u << 2) == 0,
              "... and the rest (a blow, a bullet) none");
        PlayerState player;
        logicHurt(player, 5, kHurtShock);
        check(player.health == 95 && player.hurtKinds.value == kHurtShock, "a hurt keeps its kind for the display");
        logicHurt(player, -5, kHurtBurn);
        check(player.hurtKinds.value == kHurtShock, "... healing is no hurt");
        // ... and no save has it: a player written with it and without is
        // the same bytes.
        PlayerState plain = player;
        plain.hurtKinds.value = 0;
        std::vector<std::uint8_t> with, without;
        appendState(with, player);
        appendState(without, plain);
        check(with == without && stateBytes(player) == stateBytes(plain), "the kinds are no part of a save");
    }
} // namespace

int main()
{
    testPlaces();
    testHurts();
    testKinds();
    if (g_failures == 0)
        std::printf("test_hud: ok\n");
    return g_failures == 0 ? 0 : 1;
}

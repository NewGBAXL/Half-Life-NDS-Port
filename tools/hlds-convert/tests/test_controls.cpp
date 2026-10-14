// What the buttons and the stylus mean (shared/HlControls.hpp): the classic
// layout is, to the frame, what the game did before there was a choice; the
// modern one gives each button its work; the hand changes sides; the pad and
// the stylus can each take the other's axis.

#include <cstdio>
#include <cstdlib>

#include "HlControls.hpp"
#include "HlMonsters.hpp"
#include "HlSettings.hpp"
#include "TestBspBuilder.hpp"

using namespace hldata;
using namespace hldstest;

namespace
{
    /// The controls as they were when there was one layout: the game's own
    /// code of that time, kept here to be compared with.
    struct Before
    {
        int moveForward = 0, moveSide = 0;
        bool jump = false, duck = false, usePressed = false, attack = false, second = false;
        int weaponPick = 0;
        bool menuPressed = false, reloadPressed = false;
        int touchOn = kTouchAim;
        bool lampPressed = false, startPressed = false, tapped = false;
        int tapX = 0, tapY = 0, lookDx = 0, lookDy = 0;

        int touch_ = kTouchAim;
        bool touching_ = false, jumping_ = false;
        std::uint32_t lastTapBlank_ = 0;
        int lastTapX_ = -1000, lastTapY_ = -1000;
        std::uint32_t lastHeld_ = 0;
        int lastX_ = 0, lastY_ = 0;

        void poll(const ControlsIn &in, bool leftHanded)
        {
            const std::uint32_t held = in.held;
            moveForward = ((held & (kButtonUp | kButtonX)) ? 1 : 0) - ((held & (kButtonDown | kButtonB)) ? 1 : 0);
            moveSide = ((held & (kButtonRight | kButtonA)) ? 1 : 0) - ((held & (kButtonLeft | kButtonY)) ? 1 : 0);
            lampPressed = (held & ~lastHeld_ & kButtonSelect) != 0;
            startPressed = (held & ~lastHeld_ & kButtonStart) != 0;
            tapped = false;
            usePressed = false;
            weaponPick = 0;
            menuPressed = false;
            reloadPressed = false;
            lastHeld_ = held;
            lookDx = 0;
            lookDy = 0;
            if (in.touching)
            {
                if (!touching_)
                {
                    tapped = true;
                    tapX = in.touchX;
                    tapY = in.touchY;
                    touch_ = touchHit(touchLayout(leftHanded), in.touchX, in.touchY);
                    usePressed = touch_ == kTouchUse;
                    menuPressed = touch_ == kTouchMenu;
                    reloadPressed = touch_ == kTouchReload;
                    lampPressed = lampPressed || touch_ == kTouchLamp;
                    if (touch_ >= kTouchSlot)
                        weaponPick = touch_ - kTouchSlot + 1;
                    jumping_ = false;
                    if (touch_ == kTouchAim)
                    {
                        const std::uint32_t now = in.blank;
                        const int dx = in.touchX - lastTapX_, dy = in.touchY - lastTapY_;
                        jumping_ = now - lastTapBlank_ <= 20 && dx > -40 && dx < 40 && dy > -40 && dy < 40;
                        lastTapBlank_ = jumping_ ? now - 1000 : now;
                        lastTapX_ = in.touchX;
                        lastTapY_ = in.touchY;
                    }
                }
                else if (touch_ == kTouchAim)
                {
                    lookDx = in.touchX - lastX_;
                    lookDy = in.touchY - lastY_;
                }
                lastX_ = in.touchX;
                lastY_ = in.touchY;
                touching_ = true;
            }
            else
            {
                touching_ = false;
                touch_ = kTouchAim;
                jumping_ = false;
            }
            jump = jumping_ || touch_ == kTouchJump;
            duck = touch_ == kTouchDuck;
            touchOn = touching_ ? touch_ : kTouchAim;
            attack = (held & kButtonL) != 0;
            second = (held & kButtonR) != 0;
        }
    };

    std::uint32_t g_seed = 12345;
    std::uint32_t rnd()
    {
        g_seed = g_seed * 1664525u + 1013904223u;
        return g_seed >> 8;
    }

    /// Fifty thousand frames of someone leaning on everything at once.
    void testClassicIsAsBefore()
    {
        Before before;
        ControlsState state;
        ControlSettings set; // classic, right-handed, nothing swapped
        ControlsIn in;
        int differ = 0;
        for (int frame = 0; frame < 50000; ++frame)
        {
            in.blank += 1 + rnd() % 2;
            // (Buttons change now and then; the stylus comes down, drags,
            // jumps about and lifts.)
            if (rnd() % 4 == 0)
                in.held ^= 1u << (rnd() % 12);
            if (rnd() % 6 == 0)
                in.touching = !in.touching;
            if (in.touching && rnd() % 3 == 0)
            {
                in.touchX = static_cast<int>(rnd() % 256);
                in.touchY = static_cast<int>(rnd() % 192);
            }
            else if (in.touching)
            {
                in.touchX += static_cast<int>(rnd() % 7) - 3;
                in.touchY += static_cast<int>(rnd() % 7) - 3;
            }
            before.poll(in, false);
            const ControlsOut now = controlsStep(set, in, state);
            differ += now.moveForward != before.moveForward || now.moveSide != before.moveSide ||
                      now.jump != before.jump || now.duck != before.duck || now.usePressed != before.usePressed ||
                      now.attack != before.attack || now.second != before.second ||
                      now.weaponPick != before.weaponPick || now.weaponStep != 0 ||
                      now.menuPressed != before.menuPressed || now.reloadPressed != before.reloadPressed ||
                      now.touchOn != before.touchOn || now.lampPressed != before.lampPressed ||
                      now.startPressed != before.startPressed || now.tapped != before.tapped ||
                      (now.tapped && (now.tapX != before.tapX || now.tapY != before.tapY)) ||
                      now.lookDx != before.lookDx || now.lookDy != before.lookDy;
        }
        check(differ == 0, "the classic layout, right-handed, is what the controls were: every frame of 50000");
    }

    /// One frame with `held` down and no stylus.
    ControlsOut press(const ControlSettings &set, ControlsState &state, std::uint32_t held, std::uint32_t &blank)
    {
        ControlsIn in;
        in.held = held;
        in.blank = ++blank;
        return controlsStep(set, in, state);
    }
    ControlsOut touch(const ControlSettings &set, ControlsState &state, int x, int y, std::uint32_t &blank,
                      std::uint32_t held = 0)
    {
        ControlsIn in;
        in.held = held;
        in.touching = true;
        in.touchX = x;
        in.touchY = y;
        in.blank = ++blank;
        return controlsStep(set, in, state);
    }

    void testModern()
    {
        ControlSettings set;
        set.scheme = kControlsModern;
        ControlsState state;
        std::uint32_t blank = 100;

        ControlsOut out = press(set, state, kButtonUp | kButtonRight, blank);
        check(out.moveForward == 1 && out.moveSide == 1, "modern: the D-pad moves");
        out = press(set, state, kButtonL, blank);
        check(out.attack && !out.second, "modern: L fires");
        out = press(set, state, kButtonR, blank);
        check(out.second && !out.attack, "modern: R is the second trigger");
        out = press(set, state, 0, blank);

        out = press(set, state, kButtonX, blank);
        check(out.weaponStep == -1 && out.moveForward == 0, "modern: X is the weapon before, and no step forward");
        out = press(set, state, kButtonX, blank);
        check(out.weaponStep == 0, "... once, however long it is held");
        out = press(set, state, kButtonY, blank);
        check(out.weaponStep == 1 && out.moveSide == 0, "modern: Y is the weapon after");
        out = press(set, state, kButtonB, blank);
        check(out.reloadPressed && out.moveForward == 0, "modern: B reloads");
        out = press(set, state, kButtonB, blank);
        check(!out.reloadPressed, "... once");
        out = press(set, state, kButtonA, blank);
        check(out.jump && out.moveSide == 0, "modern: A jumps");
        out = press(set, state, kButtonA, blank);
        check(out.jump, "... for as long as it is held");
        out = press(set, state, 0, blank);
        check(!out.jump && !out.duck, "... and no longer");

        // The second shoulder with the pad: how the player stands.
        out = press(set, state, kButtonR, blank);
        out = press(set, state, kButtonR | kButtonDown, blank);
        check(out.duck && out.moveForward == 0, "modern: R and down crouches, and is not a step back");
        out = press(set, state, 0, blank);
        check(out.duck, "... and stays crouched with both let go");
        out = press(set, state, kButtonDown, blank);
        check(out.duck && out.moveForward == -1, "... walking, crouched");
        out = press(set, state, kButtonR | kButtonLeft, blank);
        check(out.duck && out.moveSide == -1, "the pad's left and right sidestep whatever R is doing");
        out = press(set, state, kButtonR | kButtonUp, blank);
        check(!out.duck && out.moveForward == 0, "modern: R and up stands");
        out = press(set, state, kButtonSelect | kButtonStart, blank);
        check(out.lampPressed && out.startPressed, "Select is the lamp and Start the menu, as ever");

        // The button on the screen stands someone who crouched by R.
        press(set, state, 0, blank);
        press(set, state, kButtonR, blank);
        press(set, state, kButtonR | kButtonDown, blank);
        const TouchLayout at = touchLayout(false);
        out = touch(set, state, at.duck.x + 5, at.duck.y + 5, blank);
        check(out.duck, "the screen's DUCK, held, crouches");
        out = press(set, state, 0, blank);
        check(!out.duck, "... and let go, stands -- also someone who had crouched with R");
    }

    void testTaps()
    {
        ControlSettings modern;
        modern.scheme = kControlsModern;
        ControlSettings classic;
        const TouchLayout at = touchLayout(false);
        const int x = at.pad.x + 80, y = at.pad.y + 60;

        for (int which = 0; which < 2; ++which)
        {
            const ControlSettings &set = which ? modern : classic;
            ControlsState state;
            std::uint32_t blank = 1000;
            // A tap: down three frames, up.
            ControlsOut out = touch(set, state, x, y, blank);
            check(!out.usePressed && out.tapped, "a touch of the aim pad is not USE while it is down");
            touch(set, state, x + 1, y, blank);
            touch(set, state, x + 1, y + 1, blank);
            out = press(set, state, 0, blank);
            check(out.usePressed == (which == 1), which ? "modern: a short tap is USE, as it lifts"
                                                         : "classic: a tap is nothing");
            out = press(set, state, 0, blank);
            check(!out.usePressed, "... the once");
            // Straight after, a second: a jump, and not a second USE.
            out = touch(set, state, x + 3, y - 2, blank);
            check(out.jump, "two quick taps jump");
            out = touch(set, state, x + 3, y - 2, blank);
            check(out.jump, "... for as long as the second is held");
            out = press(set, state, 0, blank);
            check(!out.jump && !out.usePressed, "... and its lifting is not a USE");
        }

        ControlsState state;
        std::uint32_t blank = 5000;
        // A drag is not a tap.
        touch(modern, state, x, y, blank);
        ControlsOut out = touch(modern, state, x + 12, y, blank);
        check(out.lookDx == 12 && out.lookDy == 0, "a drag turns the view by as far as it went");
        out = press(modern, state, 0, blank);
        check(!out.usePressed, "... and is not a tap, though it was short");
        // Nor is a long press.
        blank += 100;
        touch(modern, state, x, y, blank);
        for (int n = 0; n < 20; ++n)
            touch(modern, state, x, y, blank);
        out = press(modern, state, 0, blank);
        check(!out.usePressed, "a touch held a third of a second is not a tap");
        // Nor a touch that started on a control.
        blank += 100;
        out = touch(modern, state, at.jump.x + 4, at.jump.y + 4, blank);
        check(out.jump && out.touchOn == kTouchJump, "the screen's JUMP jumps");
        out = press(modern, state, 0, blank);
        check(!out.usePressed, "... and lifting from it is not USE");
        blank += 100;
        out = touch(modern, state, at.use.x + 4, at.use.y + 4, blank);
        check(out.usePressed, "the screen's USE is still USE, as the stylus lands");
        out = touch(modern, state, at.slot[2].x + 4, at.slot[2].y + 4, blank);
        check(out.weaponPick == 0, "(sliding on from it to a weapon's slot takes no weapon)");
        press(modern, state, 0, blank);
        out = touch(modern, state, at.slot[2].x + 4, at.slot[2].y + 4, blank);
        check(out.weaponPick == 3, "a slot touched takes its weapon");
    }

    void testHand()
    {
        ControlSettings set;
        set.scheme = kControlsModern;
        set.leftHanded = true;
        ControlsState state;
        std::uint32_t blank = 100;
        ControlsOut out = press(set, state, kButtonX | kButtonA, blank);
        check(out.moveForward == 1 && out.moveSide == 1 && !out.jump && out.weaponStep == 0,
              "left-handed: A B X Y are the pad, by where they are");
        out = press(set, state, kButtonB | kButtonY, blank);
        check(out.moveForward == -1 && out.moveSide == -1 && !out.reloadPressed, "... all four");
        out = press(set, state, kButtonR, blank);
        check(out.attack && !out.second, "left-handed: R fires");
        out = press(set, state, kButtonL, blank);
        check(out.second && !out.attack, "... and L is the second trigger");
        press(set, state, 0, blank);
        out = press(set, state, kButtonUp, blank);
        check(out.weaponStep == -1 && out.moveForward == 0, "left-handed: the D-pad has the face buttons' work");
        out = press(set, state, kButtonLeft, blank);
        check(out.weaponStep == 1, "... the weapon after");
        out = press(set, state, kButtonDown, blank);
        check(out.reloadPressed, "... a reload");
        out = press(set, state, kButtonRight, blank);
        check(out.jump, "... a jump");
        press(set, state, 0, blank);
        press(set, state, kButtonL, blank);
        out = press(set, state, kButtonL | kButtonB, blank);
        check(out.duck, "left-handed: L and the pad's down (B) crouches");
        out = press(set, state, kButtonL | kButtonX, blank);
        check(!out.duck, "... and its up (X) stands");
        const TouchLayout mirrored = touchLayout(true);
        out = touch(set, state, mirrored.jump.x + 4, mirrored.jump.y + 4, blank);
        check(out.jump, "left-handed: the touch screen is the other way round");

        ControlSettings classic;
        classic.leftHanded = true;
        ControlsState other;
        out = press(classic, other, kButtonR, blank);
        check(out.attack && !out.second, "classic, left-handed: R fires too");
        out = press(classic, other, kButtonUp | kButtonA, blank);
        check(out.moveForward == 1 && out.moveSide == 1, "... and both sets of four still move");
    }

    void testSticks()
    {
        const TouchLayout at = touchLayout(false);
        const int x = at.pad.x + 100, y = at.pad.y + 60;
        {
            ControlSettings set;
            set.padTurns = true;
            ControlsState state;
            std::uint32_t blank = 100;
            press(set, state, 0, blank); // (the first frame is as long as it likes)
            ControlsOut out = press(set, state, kButtonRight, blank);
            check(out.moveSide == 0 && out.lookDx == kPadTurnPixels, "pad turns: right turns right, and is no sidestep");
            out = press(set, state, kButtonLeft | kButtonUp, blank);
            check(out.lookDx == -kPadTurnPixels && out.moveForward == 1, "... left turns left; up still walks");
            blank += 1; // (a frame that took two)
            out = press(set, state, kButtonRight, blank);
            check(out.lookDx == 2 * kPadTurnPixels, "... twice as far in a frame twice as long");
            press(set, state, 0, blank);
            touch(set, state, x, y, blank);
            out = touch(set, state, x + 4, y - 9, blank);
            check(out.moveSide == 0 && out.lookDx == 0 && out.lookDy == -9,
                  "pad turns: the stylus still looks up and down, and near where it landed does not sidestep");
            out = touch(set, state, x + 30, y - 9, blank);
            check(out.moveSide == 1 && out.lookDx == 0, "... pushed right, it sidesteps right");
            out = touch(set, state, x + 30, y - 9, blank);
            check(out.moveSide == 1, "... for as long as it is held there");
            out = touch(set, state, x - 30, y, blank);
            check(out.moveSide == -1, "... and left");
            out = press(set, state, 0, blank);
            check(out.moveSide == 0, "... and lifted, stops");
        }
        {
            ControlSettings set;
            set.padLooks = true;
            ControlsState state;
            std::uint32_t blank = 100;
            press(set, state, 0, blank); // (the first frame is as long as it likes)
            ControlsOut out = press(set, state, kButtonUp, blank);
            check(out.moveForward == 0 && out.lookDy == -kPadLookPixels, "pad looks: up looks up, and is no step");
            out = press(set, state, kButtonDown | kButtonRight, blank);
            check(out.lookDy == kPadLookPixels && out.moveSide == 1, "... down looks down; right still sidesteps");
            press(set, state, 0, blank);
            touch(set, state, x, y, blank);
            out = touch(set, state, x + 7, y - 25, blank);
            check(out.moveForward == 1 && out.lookDx == 7 && out.lookDy == 0,
                  "pad looks: the stylus pushed up walks forward, and still turns");
            out = touch(set, state, x + 7, y + 25, blank);
            check(out.moveForward == -1, "... pulled down, back");
        }
        {
            ControlSettings set;
            set.padTurns = set.padLooks = true;
            ControlsState state;
            std::uint32_t blank = 100;
            press(set, state, 0, blank); // (the first frame is as long as it likes)
            ControlsOut out = press(set, state, kButtonUp | kButtonLeft, blank);
            check(out.moveForward == 0 && out.moveSide == 0 && out.lookDx == -kPadTurnPixels &&
                      out.lookDy == -kPadLookPixels,
                  "both: the pad is the view");
            press(set, state, 0, blank);
            touch(set, state, x, y, blank);
            out = touch(set, state, x - 20, y - 20, blank);
            check(out.moveForward == 1 && out.moveSide == -1 && out.lookDx == 0 && out.lookDy == 0,
                  "... and the stylus the legs");
        }
    }

    void testWeaponStep()
    {
        Inventory inv;
        inv.have = static_cast<std::uint8_t>((1u << kWeaponCrowbar) | (1u << kWeaponShotgun) | (1u << kWeaponPython));
        inv.current = kWeaponShotgun;
        check(weaponStepped(inv, 0) == kWeaponNone, "no step: no weapon asked for");
        check(weaponStepped(inv, 1) == kWeaponPython, "the weapon after, of those held");
        check(weaponStepped(inv, -1) == kWeaponCrowbar, "the weapon before");
        inv.current = kWeaponPython;
        check(weaponStepped(inv, 1) == kWeaponCrowbar, "... round from the last to the first");
        inv.current = kWeaponCrowbar;
        check(weaponStepped(inv, -1) == kWeaponPython, "... and from the first to the last");
        inv.have = static_cast<std::uint8_t>(1u << kWeaponCrowbar);
        check(weaponStepped(inv, 1) == kWeaponNone && weaponStepped(inv, -1) == kWeaponNone,
              "with one weapon there is none to step to");
        inv.have = 1; // (the suit, which is no weapon)
        inv.current = kWeaponNone;
        check(weaponStepped(inv, 1) == kWeaponNone, "... nor with none");
    }

    void testSettings()
    {
        SettingsData data;
        data.controlScheme = kControlsModern;
        data.padTurns = 1;
        data.leftHanded = 1;
        std::vector<std::uint8_t> bytes = writeSettings(data);
        SettingsData back;
        check(bytes.size() <= 64 && readSettings(bytes.data(), bytes.size(), back) &&
                  back.controlScheme == kControlsModern && back.padTurns == 1 && back.padLooks == 0 &&
                  back.leftHanded == 1,
              "the layout and the swaps are kept with the other settings");
        check(SettingsData().controlScheme == kControlsClassic && SettingsData().padTurns == 0 &&
                  SettingsData().padLooks == 0,
              "... and are the classic layout, nothing swapped, unless it is said");
        data.controlScheme = 2;
        bytes = writeSettings(data);
        SettingsData untouched;
        check(!readSettings(bytes.data(), bytes.size(), untouched) && untouched.controlScheme == kControlsClassic,
              "a layout there is not is not believed");

        // A file of the version before: all it had is kept.
        SettingsData old;
        old.leftHanded = 1;
        old.viewBob = 1;
        old.volume[kVolumeMusic] = 3;
        old.renderMode = 3;
        constexpr std::size_t flags = yas::mem | yas::binary | yas::no_header;
        yas::mem_ostream os;
        yas::binary_oarchive<yas::mem_ostream, flags> oa(os);
        old.serializeEight(oa);
        const auto buf = os.get_intrusive_buffer();
        SettingsHeader header{};
        std::memcpy(header.magic, kSettingsMagic, sizeof(header.magic));
        header.version = kSettingsVersionBefore;
        header.payloadBytes = static_cast<std::uint16_t>(buf.size);
        header.crc32 = crc32(reinterpret_cast<const std::uint8_t *>(buf.data), static_cast<std::uint32_t>(buf.size));
        std::vector<std::uint8_t> file(sizeof(header) + buf.size);
        std::memcpy(file.data(), &header, sizeof(header));
        std::memcpy(file.data() + sizeof(header), buf.data, buf.size);
        SettingsData read;
        check(readSettings(file.data(), file.size(), read) && read.leftHanded == 1 && read.viewBob == 1 &&
                  read.volume[kVolumeMusic] == 3 && read.renderMode == 3 &&
                  read.controlScheme == kControlsClassic && read.padTurns == 0 && read.padLooks == 0,
              "settings saved before there were layouts are read as they were, with the classic layout");
    }
} // namespace

int main()
{
    testClassicIsAsBefore();
    testModern();
    testTaps();
    testHand();
    testSticks();
    testWeaponStep();
    testSettings();
    if (g_failures == 0)
        std::printf("test_controls: ok\n");
    return g_failures == 0 ? 0 : 1;
}

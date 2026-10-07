#include "game/Player.hpp"

#include "Global.hpp"

#include <nds.h>

#include "DebugLog.hpp"
#include "data/LevelFile.hpp"
#include "game/Game.hpp"
#include "game/Input.hpp"
#include "render/Camera.hpp"

// ARM code (the file name says so), for the movement's 64-bit multiplies; NOT
// in fast memory. The movement runs once a frame, and its ten thousand
// instructions are mostly branches not taken. What it calls thousands of
// times -- the hull trace -- is in fast memory, in Trace.itcm.arm.cpp, and is
// reached through MoveWorld::trace.

namespace hlrt
{
    Player player;

    namespace
    {
        using namespace hldata;

        /// One sixtieth of a second in Q16, to the nearest unit; four vblanks
        /// are 1/15 s to within one part in 4000.
        constexpr std::int32_t kVblankTime = 1092;
        /// The longest step taken in one go. A frame that took longer than this
        /// (a load, a hitch) is played in slow motion rather than letting the
        /// player cover a quarter of a room between two collision checks.
        constexpr int kMaxVblanks = 4;

        constexpr int kFlySpeed = 320; // units a second
    } // namespace

    void Player::spawn()
    {
        const LevelView &view = level.view;
        state = PlayerState();
        camera.yaw = 0;
        camera.pitch = 0;
        for (std::uint32_t i = 0; i < view.entityCount; ++i)
        {
            const EntityRecord &entity = view.entities[i];
            if (entity.classId != static_cast<std::uint16_t>(EntityClass::InfoPlayerStart))
                continue;
            for (int a = 0; a < 3; ++a)
                state.origin[a] = entity.origin[a];
            camera.yaw = entity.angles[1];
            break;
        }

        // A development aid: "x y z yaw" in debug_start.txt, in whole units
        // and degrees, starts there instead (tools/emu/play.sh writes it).
        if (std::FILE *file = std::fopen(global.dataPath("debug_start.txt").c_str(), "r"))
        {
            int x, y, z, yaw, track = -1;
            if (std::fscanf(file, "%d %d %d %d %d", &x, &y, &z, &yaw, &track) >= 4)
            {
                // (A fifth number is a track of music to start with.)
                debugTrack = track;
                state.origin[0] = fxFromInt(x);
                state.origin[1] = fxFromInt(y);
                state.origin[2] = fxFromInt(z);
                camera.yaw = static_cast<std::uint16_t>((yaw * 65536) / 360);
                // ... and armed, wherever that is.
                state.inv.have = (1u << kWeaponCrowbar) | (1u << kWeaponGlock) | (1u << kWeaponShotgun) |
                                 (1u << kWeaponGrenade);
                state.inv.grenades = 5;
                state.inv.current = kWeaponCrowbar;
                state.inv.clipGlock = 17;
                state.inv.clipShotgun = 8;
                state.inv.ammo9mm = 68;
                state.inv.ammoBuck = 32;
            }
            std::fclose(file);
        }
    }

    void Player::turn()
    {
        // One pixel of stylus travel is 1/512 of a turn: the width of the
        // screen is half a turn.
        camera.yaw = static_cast<std::uint16_t>(camera.yaw - input.lookDx * 128);
        camera.pitch = static_cast<std::int16_t>(camera.pitch - input.lookDy * 128);
        camera.clampPitch();
    }

    void Player::fly(int vblanks)
    {
        camera.update(); // for the view vectors
        const fx step = (fxFromInt(kFlySpeed) / 60) * vblanks;
        for (int i = 0; i < 3; ++i)
        {
            const std::int64_t along =
                static_cast<std::int64_t>(camera.forward[i]) * input.moveForward +
                static_cast<std::int64_t>(camera.right[i]) * input.moveSide;
            state.origin[i] += static_cast<fx>((along * step) >> kFxShift);
            state.velocity[i] = 0;
        }
        state.origin[2] += (input.jump ? step : 0) - (input.duck ? step : 0);
    }

    void Player::frame(int vblanks)
    {
        if (vblanks < 1)
            vblanks = 1;
        if (vblanks > kMaxVblanks)
            vblanks = kMaxVblanks;

        turn();
        if (input.noclipPressed)
            noclip = !noclip;

        if (noclip)
        {
            fly(vblanks);
        }
        else
        {
            const DebugLog::Perf::Scope scope(DebugLog::Section::Move);
            MoveCommand cmd;
            cmd.forward = input.moveForward;
            cmd.side = input.moveSide;
            cmd.jump = input.jump;
            cmd.duck = input.duck;
            // libnds angles are 32768 to the circle; ours are 65536.
            const fx s = sinLerp(static_cast<s16>(camera.yaw >> 1));
            const fx c = cosLerp(static_cast<s16>(camera.yaw >> 1));
            cmd.forwardXY[0] = c;
            cmd.forwardXY[1] = s;
            cmd.rightXY[0] = s;
            cmd.rightXY[1] = -c;
            // The view's own axes, with pitch, for ladders. The camera's are
            // from the last update, so bring them up to this frame's turn.
            camera.update();
            for (int a = 0; a < 3; ++a)
            {
                cmd.viewForward[a] = camera.forward[a];
                cmd.viewRight[a] = camera.right[a];
            }
            playerMove(state, cmd, game.world, kVblankTime * vblanks);
        }

        camera.pos[0] = state.origin[0];
        camera.pos[1] = state.origin[1];
        camera.pos[2] = state.origin[2] + state.viewHeight;
        camera.update();
    }
} // namespace hlrt

#pragma once

// The player's movement: walking, falling, jumping, crouching, climbing steps.
//
// This follows the structure and the constants of Half-Life's own player
// movement (pm_shared.c: friction, accelerate, the four-bump slide move, the
// 18-unit step, the split gravity step) so that the game feels like itself --
// the speed, the jump height and the way the player slides along a wall are
// part of what the maps were built around. It is a fixed-point rewrite, not a
// translation: positions and velocities are Q12, time is Q16 seconds, and
// every product goes through 64 bits (HlFixed.hpp).
//
// Brush entities are collided with through HlMovers.hpp: the trace used here
// covers the world and every solid brush.
//
// Being carried and pushed by moving brushes is the entity logic's doing
// (HlLogic.hpp).
//
// WHAT IS NOT HERE YET: water, fall damage.
//
// std-only and inline, like HlHullTrace.hpp, so the host tests drive the very
// code the console runs. Sine and cosine are not available here (the DS's
// tables live in libnds), so the caller hands in the view's horizontal axes.

#include "HlMovers.hpp"

namespace hldata
{
    // --- Half-Life's movement variables (the game's defaults).
    inline constexpr fx kGravity = fxFromInt(800);
    inline constexpr fx kMaxSpeed = fxFromInt(320);
    inline constexpr fx kStopSpeed = fxFromInt(100);
    inline constexpr int kAccelerate = 10;
    inline constexpr int kAirAccelerate = 10;
    inline constexpr int kFriction = 4;
    inline constexpr int kEdgeFriction = 2;
    inline constexpr fx kStepSize = fxFromInt(18);
    inline constexpr fx kMaxVelocity = fxFromInt(2000);
    inline constexpr fx kAirSpeedCap = fxFromInt(30);
    /// The speed the movement keys ask for; clamped to kMaxSpeed.
    inline constexpr fx kWishSpeed = fxFromInt(400);
    /// sqrt(2 * 800 * 45): the take-off speed that reaches 45 units.
    inline constexpr fx kJumpSpeed = 1099072;
    /// Movement keys count for a third while crouched.
    inline constexpr fx kDuckSpeedScale = 1364;
    /// A floor is a plane whose normal's z is at least 0.7.
    inline constexpr std::int32_t kFloorNormalZ = 751619277; // 0.7 in Q30
    inline constexpr fx kStopEpsilon = 410;                  // 0.1 unit/s
    /// Above this upward speed the player is not on the ground whatever is
    /// under them.
    inline constexpr fx kLeaveGroundSpeed = fxFromInt(180);
    /// Bunny-hop cap: 1.7 times the top speed.
    inline constexpr fx kBunnyCap = (kMaxSpeed * 17) / 10;

    inline constexpr fx kViewHeight = fxFromInt(28);
    inline constexpr fx kDuckViewHeight = fxFromInt(12);
    /// How much lower the crouched hull's centre is than the standing one's.
    inline constexpr fx kDuckDrop = fxFromInt(18);
    inline constexpr fx kStandingFeet = fxFromInt(36); ///< origin to feet
    inline constexpr fx kCrouchedFeet = fxFromInt(18);
    inline constexpr int kDuckTimeMs = 400;
    /// On a ladder: how fast the keys climb, and how hard a jump kicks off it.
    inline constexpr fx kClimbSpeed = fxFromInt(200);
    inline constexpr fx kLadderJumpSpeed = fxFromInt(270);

    struct MoveCommand
    {
        int forward = 0; ///< -1, 0, 1
        int side = 0;    ///< -1 left, 1 right
        bool jump = false;
        bool duck = false;
        /// The view's horizontal axes, unit length, Q12: forward is
        /// (cos yaw, sin yaw) and right is (sin yaw, -cos yaw).
        fx forwardXY[2] = {kFxOne, 0};
        fx rightXY[2] = {0, -kFxOne};
        /// The view's own axes, pitch included, unit length, Q12. Only a
        /// ladder uses them: looking up and pressing forward climbs, looking
        /// down and pressing forward descends.
        fx viewForward[3] = {kFxOne, 0, 0};
        fx viewRight[3] = {0, -kFxOne, 0};
    };

    // What the player carries.
    inline constexpr std::uint8_t kWeaponNone = 0;
    inline constexpr std::uint8_t kWeaponCrowbar = 1;
    inline constexpr std::uint8_t kWeaponGlock = 2;
    inline constexpr std::uint8_t kWeaponShotgun = 3;
    inline constexpr std::uint8_t kWeaponGrenade = 4;
    inline constexpr std::uint8_t kWeaponMp5 = 5;    ///< with its grenade launcher
    inline constexpr std::uint8_t kWeaponPython = 6; ///< the .357
    // ... and those that came after the first six (Arsenal):
    inline constexpr std::uint8_t kWeaponCrossbow = 7;
    inline constexpr std::uint8_t kWeaponSatchel = 8;
    inline constexpr std::uint8_t kWeaponTripmine = 9;
    inline constexpr std::uint8_t kWeaponSnark = 10;
    inline constexpr std::uint8_t kWeaponCount = 11;
    /// The first weapon Inventory::have has no bit for: from it on, they are
    /// the Arsenal's.
    inline constexpr std::uint8_t kWeaponFirstMore = 7;

    struct Inventory
    {
        /// A bit per weapon (1 << kWeapon*); bit 0, which is no weapon's, is
        /// the suit.
        std::uint8_t have = 0;
        std::uint8_t current = kWeaponNone;
        std::int16_t clipGlock = 0;   ///< rounds in the pistol
        std::int16_t clipShotgun = 0; ///< shells in the shotgun
        std::int16_t ammo9mm = 0;     ///< carried besides
        std::int16_t ammoBuck = 0;
        std::int16_t armor = 0;       ///< 0..100
        std::int16_t grenades = 0;    ///< 0..10
        std::int16_t clipMp5 = 0;     ///< rounds in the MP5 (it takes the pistol's 9 mm)
        std::int16_t clipPython = 0;  ///< ... in the .357
        std::int16_t ammo357 = 0;     ///< carried besides, 0..36
        std::int16_t ammoAr = 0;      ///< grenades for the MP5's launcher, 0..10
    };

    /// What the player carries of the weapons that came after the first
    /// six (kWeaponFirstMore on): kept apart from Inventory, which is in
    /// every save and every level change as it is, so that a save made
    /// before these were is read as it was written. (It goes in a save after
    /// all the rest: HlSave.hpp.) Inventory::current may be any weapon.
    struct Arsenal
    {
        std::uint16_t have = 0;       ///< a bit per weapon: 1 << (kWeapon* - kWeaponFirstMore)
        std::int16_t boltsClip = 0;   ///< in the crossbow
        std::int16_t bolts = 0;       ///< carried besides, 0..50
        std::int16_t satchels = 0;    ///< 0..5
        std::int16_t tripmines = 0;   ///< 0..5
        std::int16_t snarks = 0;      ///< 0..15
        std::int16_t rockets = 0;     ///< (the weapons still to come)
        std::int16_t uranium = 0;
        std::int16_t hornets = 0;
        std::uint8_t zoomed = 0;      ///< the crossbow's sight is up

        template <class Ar>
        void serialize(Ar &ar)
        {
            ar &have &boltsClip &bolts &satchels &tripmines &snarks &rockets &uranium &hornets &zoomed;
        }
    };

    struct PlayerState
    {
        fx origin[3] = {0, 0, 0};
        fx velocity[3] = {0, 0, 0};
        fx viewHeight = kViewHeight; ///< eyes above the origin
        bool onGround = false;
        /// The brush entity stood on, as a mover index; -1 on the world or in
        /// the air. A lift carries whoever has it here.
        std::int16_t groundMover = -1;
        bool ducked = false;   ///< using the crouched hull
        bool ducking = false;  ///< on the way down, still in the standing hull
        bool onLadder = false;
        /// The way the player is facing, horizontal, unit, Q12: what a door
        /// that opens away from the player turns away from.
        fx facingXY[2] = {kFxOne, 0};
        bool jumpHeld = false; ///< jump has not been released since the last jump
        bool duckHeld = false;
        int duckMs = 0;        ///< time since the crouch began
        bool stuck = false;    ///< the last move started inside solid and stayed there
        int stuckMs = 0;       ///< ... and for how long now
        /// Set by the move that lands: how fast the player was falling
        /// (Q12 units a second); 0 otherwise. Whoever hurts for a fall reads
        /// it.
        fx landed = 0;
        /// What has hurt the player since the caller last looked: how much
        /// in all, and where the last of it came from (`hurtKnown`: it came
        /// from somewhere).
        int hurt = 0;
        fx hurtFrom[3] = {0, 0, 0};
        bool hurtKnown = false;
        /// The brush entity the last move ran into sideways, as a mover
        /// index; -1 if none. What a box that can be pushed is pushed by.
        std::int16_t touchedMover = -1;
        /// ... and how fast the player was going when they ran into it: by
        /// the time the move is over, that speed has been taken off them.
        fx touchVelocity[2] = {0, 0};
        std::int32_t health = 100;
        Inventory inv;
        /// The rest of what they carry (owned by the game, not saved with
        /// this: a pointer is no part of the state). Null: none of it.
        Arsenal *arsenal = nullptr;
        /// How far there is still to walk before the next footstep is heard
        /// (playerFootsteps).
        fx stepLeft = 0;
        /// How deep in liquid: 0 not at all, 1 feet, 2 waist, 3 over the
        /// eyes. And what the liquid is (kContents*).
        std::uint8_t waterLevel = 0;
        std::int8_t waterType = kContentsEmpty;
        int waterJumpMs = 0;   ///< left of a jump out of the water onto a ledge
        fx waterJumpDir[2] = {0, 0};
        int underMs = 0;       ///< how long the eyes have been under
        int hurtMs = 0;        ///< until the liquid (or the lack of air) next hurts
        int drownStep = 2;     ///< what the next breath not taken costs
    };

    /// Whether the player has weapon `which`.
    inline bool hasWeapon(const PlayerState &player, int which)
    {
        if (which <= kWeaponNone || which >= kWeaponCount)
            return false;
        if (which < kWeaponFirstMore)
            return (player.inv.have & (1u << which)) != 0;
        return player.arsenal != nullptr && (player.arsenal->have & (1u << (which - kWeaponFirstMore))) != 0;
    }
    /// The game's five kinds of weapon, as its slots 1 to 5 have them: the
    /// crowbar; the pistols; the rifles; the heavy ones (still to come); what
    /// is thrown or put down. Each kind's weapons in the game's order; `count`
    /// is how many.
    inline constexpr int kWeaponCategories = 5;
    inline const std::uint8_t *weaponsOfCategory(int category, int &count)
    {
        static const std::uint8_t kLists[kWeaponCategories][4] = {
            {kWeaponCrowbar},
            {kWeaponGlock, kWeaponPython},
            {kWeaponMp5, kWeaponShotgun, kWeaponCrossbow},
            {},
            {kWeaponGrenade, kWeaponSatchel, kWeaponTripmine, kWeaponSnark}};
        static const int kCounts[kWeaponCategories] = {1, 2, 3, 0, 4};
        if (category < 1 || category > kWeaponCategories)
        {
            count = 0;
            return nullptr;
        }
        count = kCounts[category - 1];
        return kLists[category - 1];
    }
    /// Which of them weapon `which` is (1..5), or 0.
    inline int weaponCategory(int which)
    {
        for (int c = 1; c <= kWeaponCategories; ++c)
        {
            int count = 0;
            const std::uint8_t *list = weaponsOfCategory(c, count);
            for (int k = 0; k < count; ++k)
            {
                if (list[k] == which)
                    return c;
            }
        }
        return 0;
    }

    /// ... given it, or taken from them.
    inline void giveWeapon(PlayerState &player, int which)
    {
        if (which > kWeaponNone && which < kWeaponFirstMore)
            player.inv.have = static_cast<std::uint8_t>(player.inv.have | (1u << which));
        else if (which >= kWeaponFirstMore && which < kWeaponCount && player.arsenal != nullptr)
            player.arsenal->have = static_cast<std::uint16_t>(player.arsenal->have | (1u << (which - kWeaponFirstMore)));
    }
    inline void takeWeapon(PlayerState &player, int which)
    {
        if (which > kWeaponNone && which < kWeaponFirstMore)
            player.inv.have = static_cast<std::uint8_t>(player.inv.have & ~(1u << which));
        else if (which >= kWeaponFirstMore && which < kWeaponCount && player.arsenal != nullptr)
            player.arsenal->have = static_cast<std::uint16_t>(player.arsenal->have & ~(1u << (which - kWeaponFirstMore)));
    }

    /// What a touch of category `category`'s slot asks for: with one of its
    /// weapons in hand, the next of them the player has (round again); else
    /// the first they have. The one in hand if it is the only one (the
    /// caller makes that a reload); kWeaponNone if they have none of them.
    inline std::uint8_t weaponOfCategory(const PlayerState &player, int category)
    {
        int count = 0;
        const std::uint8_t *list = weaponsOfCategory(category, count);
        int at = -1;
        for (int k = 0; k < count; ++k)
        {
            if (list[k] == player.inv.current)
                at = k;
        }
        for (int n = 1; n <= count; ++n)
        {
            const std::uint8_t which = list[(at + n + count) % count];
            if (hasWeapon(player, which))
                return which;
        }
        return kWeaponNone;
    }

    namespace detail
    {
        inline fx scaleTime(fx value, std::int32_t dt)
        {
            return static_cast<fx>((static_cast<std::int64_t>(value) * dt) >> kTimeShift);
        }

        inline fx dotQ12(const fx *a, const fx *b)
        {
            return static_cast<fx>((static_cast<std::int64_t>(a[0]) * b[0] +
                                    static_cast<std::int64_t>(a[1]) * b[1] +
                                    static_cast<std::int64_t>(a[2]) * b[2]) >>
                                   kFxShift);
        }

        /// dot(v, n) with v Q12 and n Q30: Q12.
        inline fx dotNormal(const fx *v, const std::int32_t *n)
        {
            return static_cast<fx>((static_cast<std::int64_t>(v[0]) * n[0] +
                                    static_cast<std::int64_t>(v[1]) * n[1] +
                                    static_cast<std::int64_t>(v[2]) * n[2]) >>
                                   kNormalShift);
        }

        inline fx clampFx(fx v, fx lo, fx hi)
        {
            return v < lo ? lo : (v > hi ? hi : v);
        }

        /// Remove from `in` its component into the plane `normal`.
        inline void clipVelocity(const fx *in, const std::int32_t *normal, fx *out)
        {
            const fx backoff = dotNormal(in, normal);
            for (int i = 0; i < 3; ++i)
            {
                const fx change =
                    static_cast<fx>((static_cast<std::int64_t>(normal[i]) * backoff) >> kNormalShift);
                out[i] = in[i] - change;
                if (out[i] > -kStopEpsilon && out[i] < kStopEpsilon)
                    out[i] = 0;
            }
        }

        struct Mover
        {
            PlayerState &s;
            const MoveWorld &world;
            std::int32_t dt; ///< Q16 seconds

            /// The WORLD's hull for the player's current stance: what "stuck in
            /// solid" is judged against (HlMovers.hpp says why brushes are
            /// left out of that question).
            const Hull &hull() const { return s.ducked ? world.crouched : world.standing; }
            /// A move, against the world and every solid brush.
            Trace trace(const fx *from, const fx *to) const
            {
                const Trace tr = traceAll(world, s.ducked, from, to);
                // Walked into, not stood on: its face is nearer upright than
                // flat.
                if (tr.mover >= 0 && tr.fraction < kFractionOne && tr.normal[2] < kFloorNormalZ &&
                    tr.normal[2] > -kFloorNormalZ)
                {
                    s.touchedMover = tr.mover;
                    s.touchVelocity[0] = s.velocity[0];
                    s.touchVelocity[1] = s.velocity[1];
                }
                return tr;
            }

            void checkVelocity()
            {
                for (int i = 0; i < 3; ++i)
                    s.velocity[i] = clampFx(s.velocity[i], -kMaxVelocity, kMaxVelocity);
            }

            /// Gravity is applied in two halves, before and after the move,
            /// which makes the position exact for constant acceleration.
            void halfGravity()
            {
                s.velocity[2] -= scaleTime(kGravity, dt) / 2;
                checkVelocity();
            }

            void categorizePosition()
            {
                if (s.velocity[2] > kLeaveGroundSpeed)
                {
                    s.onGround = false;
                    s.groundMover = -1;
                    return;
                }
                fx below[3] = {s.origin[0], s.origin[1], s.origin[2] - fxFromInt(2)};
                const Trace tr = trace(s.origin, below);
                // No hit leaves the normal at zero, which is "too steep" too.
                s.onGround = tr.normal[2] >= kFloorNormalZ;
                s.groundMover = s.onGround ? tr.mover : static_cast<std::int16_t>(-1);
                if (s.onGround && !tr.startSolid && !tr.allSolid)
                {
                    for (int i = 0; i < 3; ++i)
                        s.origin[i] = tr.endPos[i];
                }
            }

            /// How deep in liquid the player is: sampled at the feet, the
            /// waist and the eyes, as the game does.
            void checkWater()
            {
                const fx feet = s.ducked ? kCrouchedFeet : kStandingFeet;
                fx p[3] = {s.origin[0], s.origin[1], s.origin[2] - feet + fxFromInt(1)};
                s.waterLevel = 0;
                s.waterType = kContentsEmpty;
                const int atFeet = pointContents(world, p);
                if (!isLiquid(atFeet))
                    return;
                s.waterType = static_cast<std::int8_t>(atFeet);
                s.waterLevel = 1;
                p[2] = s.origin[2];
                if (!isLiquid(pointContents(world, p)))
                    return;
                s.waterLevel = 2;
                p[2] = s.origin[2] + s.viewHeight;
                if (isLiquid(pointContents(world, p)))
                    s.waterLevel = 3;
            }

            /// What being in it costs: breath, after twelve seconds under;
            /// and slime and lava, by the second and by how deep.
            void waterHurts(int ms)
            {
                if (s.hurtMs > 0)
                    s.hurtMs -= ms;
                if (s.waterLevel == 3)
                {
                    s.underMs += ms;
                    if (s.underMs > 12000 && s.hurtMs <= 0)
                    {
                        s.health -= s.drownStep;
                        if (s.drownStep < 5)
                            ++s.drownStep;
                        s.hurtMs = 1000;
                    }
                }
                else
                {
                    s.underMs = 0;
                    s.drownStep = 2;
                }
                if (s.waterLevel > 0 && s.hurtMs <= 0 &&
                    (s.waterType == kContentsLava || s.waterType == kContentsSlime))
                {
                    s.health -= (s.waterType == kContentsLava ? 10 : 4) * s.waterLevel;
                    s.hurtMs = 1000;
                }
            }

            /// Swimming: the keys move along where the player LOOKS, there is
            /// no gravity, and left alone they sink slowly.
            void waterMove(const MoveCommand &cmd)
            {
                fx wishVel[3];
                for (int i = 0; i < 3; ++i)
                    wishVel[i] = fxMul(cmd.viewForward[i], kWishSpeed * cmd.forward) +
                                 fxMul(cmd.viewRight[i], kWishSpeed * cmd.side);
                if (cmd.forward == 0 && cmd.side == 0 && !cmd.jump)
                    wishVel[2] -= fxFromInt(60);
                fx wishSpeed = fxLength(wishVel);
                fx wishDir[3] = {0, 0, 0};
                if (wishSpeed > 0)
                {
                    for (int i = 0; i < 3; ++i)
                        wishDir[i] = static_cast<fx>(
                            (static_cast<std::int64_t>(wishVel[i]) << kFxShift) / wishSpeed);
                }
                if (wishSpeed > kMaxSpeed)
                    wishSpeed = kMaxSpeed;
                wishSpeed = (wishSpeed * 8) / 10;

                // The water's drag, on all three axes.
                const fx speed = fxLength(s.velocity);
                fx newSpeed = 0;
                if (speed > 0)
                {
                    newSpeed = speed - scaleTime(speed * kFriction, dt);
                    if (newSpeed < 0)
                        newSpeed = 0;
                    for (int i = 0; i < 3; ++i)
                        s.velocity[i] = static_cast<fx>(
                            (static_cast<std::int64_t>(s.velocity[i]) * newSpeed) / speed);
                }
                if (wishSpeed >= kStopEpsilon)
                {
                    const fx addSpeed = wishSpeed - newSpeed;
                    if (addSpeed > 0)
                    {
                        fx accelSpeed = scaleTime(wishSpeed * kAccelerate, dt);
                        if (accelSpeed > addSpeed)
                            accelSpeed = addSpeed;
                        for (int i = 0; i < 3; ++i)
                            s.velocity[i] += fxMul(accelSpeed, wishDir[i]);
                    }
                }
                flyMove();
            }

            /// At the surface, facing a ledge with room above it: up and over.
            void checkWaterJump(const MoveCommand &cmd)
            {
                if (s.waterJumpMs > 0 || s.velocity[2] < -fxFromInt(180))
                    return;
                // Not while backing away from what is ahead.
                const fx flat[3] = {s.velocity[0], s.velocity[1], 0};
                if (fxLength(flat) > 0 &&
                    fxMul(s.velocity[0], cmd.forwardXY[0]) + fxMul(s.velocity[1], cmd.forwardXY[1]) < 0)
                    return;
                fx from[3] = {s.origin[0], s.origin[1], s.origin[2] + fxFromInt(8)};
                fx to[3] = {from[0] + cmd.forwardXY[0] * 24, from[1] + cmd.forwardXY[1] * 24, from[2]};
                const Trace wall = trace(from, to);
                if (wall.fraction >= kFractionOne || wall.normal[2] > (1 << 27) ||
                    wall.normal[2] < -(1 << 27))
                    return; // nothing ahead, or not a wall
                from[2] = to[2] = s.origin[2] + (s.ducked ? kCrouchedFeet : kStandingFeet);
                const Trace above = trace(from, to);
                if (above.fraction < kFractionOne)
                    return; // no room over it
                s.waterJumpMs = 2000;
                s.velocity[2] = fxFromInt(225);
                s.waterJumpDir[0] = static_cast<fx>(
                    -(static_cast<std::int64_t>(wall.normal[0]) * 50) >> (kNormalShift - kFxShift));
                s.waterJumpDir[1] = static_cast<fx>(
                    -(static_cast<std::int64_t>(wall.normal[1]) * 50) >> (kNormalShift - kFxShift));
            }

            void friction()
            {
                const fx speed = fxLength(s.velocity);
                if (speed < kStopEpsilon)
                    return;
                fx drop = 0;
                if (s.onGround)
                {
                    // Standing at the lip of a drop doubles the friction, so
                    // the player does not slide off ledges: look 16 units ahead
                    // for ground within 34 units of the feet.
                    fx start[3];
                    fx stop[3];
                    for (int i = 0; i < 2; ++i)
                    {
                        start[i] = s.origin[i] +
                                   static_cast<fx>((static_cast<std::int64_t>(s.velocity[i]) * 16
                                                    << kFxShift) /
                                                   speed);
                        stop[i] = start[i];
                    }
                    start[2] = s.origin[2] - (s.ducked ? kCrouchedFeet : kStandingFeet);
                    stop[2] = start[2] - fxFromInt(34);
                    const Trace tr = trace(start, stop);
                    const int friction =
                        tr.fraction == kFractionOne ? kFriction * kEdgeFriction : kFriction;
                    const fx control = speed < kStopSpeed ? kStopSpeed : speed;
                    drop = scaleTime(control * friction, dt);
                }
                fx newSpeed = speed - drop;
                if (newSpeed < 0)
                    newSpeed = 0;
                for (int i = 0; i < 3; ++i)
                {
                    s.velocity[i] = static_cast<fx>(
                        (static_cast<std::int64_t>(s.velocity[i]) * newSpeed) / speed);
                }
            }

            void accelerate(const fx *wishDir, fx wishSpeed, fx cappedSpeed, int accel)
            {
                const fx addSpeed = cappedSpeed - dotQ12(s.velocity, wishDir);
                if (addSpeed <= 0)
                    return;
                fx accelSpeed = scaleTime(
                    static_cast<fx>(clampFx(wishSpeed, 0, kMaxVelocity)) * accel, dt);
                if (accelSpeed > addSpeed)
                    accelSpeed = addSpeed;
                for (int i = 0; i < 3; ++i)
                    s.velocity[i] += fxMul(accelSpeed, wishDir[i]);
            }

            /// Slide along whatever is in the way, up to four times.
            void flyMove()
            {
                constexpr int kMaxPlanes = 5;
                std::int32_t planes[kMaxPlanes][3];
                int planeCount = 0;
                fx original[3];
                fx primal[3];
                for (int i = 0; i < 3; ++i)
                    original[i] = primal[i] = s.velocity[i];

                std::int64_t allFraction = 0;
                std::int32_t timeLeft = dt;
                for (int bump = 0; bump < 4; ++bump)
                {
                    if (s.velocity[0] == 0 && s.velocity[1] == 0 && s.velocity[2] == 0)
                        break;
                    fx end[3];
                    for (int i = 0; i < 3; ++i)
                        end[i] = s.origin[i] + scaleTime(s.velocity[i], timeLeft);

                    const Trace tr = trace(s.origin, end);
                    allFraction += tr.fraction;
                    if (tr.allSolid)
                    {
                        s.velocity[0] = s.velocity[1] = s.velocity[2] = 0;
                        return;
                    }
                    if (tr.fraction > 0)
                    {
                        for (int i = 0; i < 3; ++i)
                        {
                            s.origin[i] = tr.endPos[i];
                            original[i] = s.velocity[i];
                        }
                        planeCount = 0;
                    }
                    if (tr.fraction == kFractionOne)
                        break;

                    timeLeft -= static_cast<std::int32_t>(
                        (static_cast<std::int64_t>(timeLeft) * tr.fraction) >> kFractionShift);

                    if (planeCount >= kMaxPlanes)
                    {
                        s.velocity[0] = s.velocity[1] = s.velocity[2] = 0;
                        break;
                    }
                    for (int i = 0; i < 3; ++i)
                        planes[planeCount][i] = tr.normal[i];
                    ++planeCount;

                    if (planeCount == 1 && !s.onGround)
                    {
                        // In the air, off the first thing hit: just slide.
                        fx clipped[3];
                        clipVelocity(original, planes[0], clipped);
                        for (int i = 0; i < 3; ++i)
                            s.velocity[i] = original[i] = clipped[i];
                    }
                    else
                    {
                        // Find a plane to slide along that does not push the
                        // move back into any of the others.
                        int i = 0;
                        for (; i < planeCount; ++i)
                        {
                            clipVelocity(original, planes[i], s.velocity);
                            int j = 0;
                            for (; j < planeCount; ++j)
                            {
                                if (j != i && dotNormal(s.velocity, planes[j]) < 0)
                                    break;
                            }
                            if (j == planeCount)
                                break;
                        }
                        if (i == planeCount)
                        {
                            // None will do: go along the crease between two
                            // planes, or stop in a corner of three.
                            if (planeCount != 2)
                            {
                                s.velocity[0] = s.velocity[1] = s.velocity[2] = 0;
                                break;
                            }
                            const std::int32_t *a = planes[0];
                            const std::int32_t *b = planes[1];
                            auto cross = [](std::int64_t x, std::int64_t y, std::int64_t z,
                                            std::int64_t w)
                            {
                                return static_cast<std::int32_t>((x * y - z * w) >> kNormalShift);
                            };
                            const std::int32_t dir[3] = {cross(a[1], b[2], a[2], b[1]),
                                                         cross(a[2], b[0], a[0], b[2]),
                                                         cross(a[0], b[1], a[1], b[0])};
                            const fx d = dotNormal(s.velocity, dir);
                            for (int k = 0; k < 3; ++k)
                            {
                                s.velocity[k] = static_cast<fx>(
                                    (static_cast<std::int64_t>(dir[k]) * d) >> kNormalShift);
                            }
                        }
                        // Turned right round: stop, rather than jitter in a
                        // sloping corner.
                        if (dotQ12(s.velocity, primal) <= 0)
                        {
                            s.velocity[0] = s.velocity[1] = s.velocity[2] = 0;
                            break;
                        }
                    }
                }
                if (allFraction == 0)
                    s.velocity[0] = s.velocity[1] = s.velocity[2] = 0;
            }

            /// The direction and speed the keys ask for.
            fx wish(const MoveCommand &cmd, fx *wishDir) const
            {
                fx scale = kWishSpeed;
                if (s.ducked)
                    scale = fxMul(scale, kDuckSpeedScale);
                fx wishVel[3];
                for (int i = 0; i < 2; ++i)
                {
                    wishVel[i] = fxMul(cmd.forwardXY[i], scale * cmd.forward) +
                                 fxMul(cmd.rightXY[i], scale * cmd.side);
                }
                wishVel[2] = 0;
                fx speed = fxLength(wishVel);
                wishDir[0] = wishDir[1] = wishDir[2] = 0;
                if (speed == 0)
                    return 0;
                for (int i = 0; i < 2; ++i)
                {
                    wishDir[i] = static_cast<fx>(
                        (static_cast<std::int64_t>(wishVel[i]) << kFxShift) / speed);
                }
                return speed > kMaxSpeed ? kMaxSpeed : speed;
            }

            void walkMove(const MoveCommand &cmd)
            {
                fx wishDir[3];
                const fx wishSpeed = wish(cmd, wishDir);

                s.velocity[2] = 0;
                accelerate(wishDir, wishSpeed, wishSpeed, kAccelerate);
                s.velocity[2] = 0;

                if (fxLength(s.velocity) < kFxOne)
                {
                    s.velocity[0] = s.velocity[1] = s.velocity[2] = 0;
                    return;
                }

                // Straight ahead first: most steps of a walk are just that.
                fx dest[3] = {s.origin[0] + scaleTime(s.velocity[0], dt),
                              s.origin[1] + scaleTime(s.velocity[1], dt), s.origin[2]};
                Trace tr = trace(s.origin, dest);
                if (tr.fraction == kFractionOne)
                {
                    for (int i = 0; i < 3; ++i)
                        s.origin[i] = tr.endPos[i];
                    return;
                }

                // Blocked. Try it two ways -- sliding along the obstacle, and
                // stepping up onto it -- and keep whichever got further.
                fx original[3], originalVel[3];
                for (int i = 0; i < 3; ++i)
                {
                    original[i] = s.origin[i];
                    originalVel[i] = s.velocity[i];
                }
                flyMove();
                fx down[3], downVel[3];
                for (int i = 0; i < 3; ++i)
                {
                    down[i] = s.origin[i];
                    downVel[i] = s.velocity[i];
                    s.origin[i] = original[i];
                    s.velocity[i] = originalVel[i];
                }

                // Up a step's height, forward, and back down.
                fx up[3] = {s.origin[0], s.origin[1], s.origin[2] + kStepSize};
                tr = trace(s.origin, up);
                if (!tr.startSolid && !tr.allSolid)
                {
                    for (int i = 0; i < 3; ++i)
                        s.origin[i] = tr.endPos[i];
                }
                flyMove();
                fx downTo[3] = {s.origin[0], s.origin[1], s.origin[2] - kStepSize};
                tr = trace(s.origin, downTo);
                bool useDown = tr.normal[2] < kFloorNormalZ; // landed on a slope too steep to stand on
                if (!useDown)
                {
                    if (!tr.startSolid && !tr.allSolid)
                    {
                        for (int i = 0; i < 3; ++i)
                            s.origin[i] = tr.endPos[i];
                    }
                    auto dist2 = [&](const fx *p)
                    {
                        const std::int64_t dx = p[0] - original[0];
                        const std::int64_t dy = p[1] - original[1];
                        return dx * dx + dy * dy;
                    };
                    useDown = dist2(down) > dist2(s.origin);
                }
                if (useDown)
                {
                    for (int i = 0; i < 3; ++i)
                    {
                        s.origin[i] = down[i];
                        s.velocity[i] = downVel[i];
                    }
                }
                else
                {
                    s.velocity[2] = downVel[2];
                }
            }

            void airMove(const MoveCommand &cmd)
            {
                fx wishDir[3];
                const fx wishSpeed = wish(cmd, wishDir);
                // In the air the keys can only add up to 30 units/s in their
                // direction, however fast the player is going: that is what
                // makes air control weak and strafing matter.
                accelerate(wishDir, wishSpeed, wishSpeed > kAirSpeedCap ? kAirSpeedCap : wishSpeed,
                           kAirAccelerate);
                flyMove();
            }

            void jump(const MoveCommand &cmd)
            {
                if (!cmd.jump)
                {
                    s.jumpHeld = false;
                    return;
                }
                if (!s.onGround || s.jumpHeld)
                {
                    s.jumpHeld = true; // holding jump does not hop on landing
                    return;
                }
                s.onGround = false;

                const fx speed = fxLength(s.velocity);
                if (speed > kBunnyCap)
                {
                    // 65% of the cap, as the game does it.
                    const std::int64_t scale =
                        (static_cast<std::int64_t>(kBunnyCap) * 65 / 100 << kFxShift) / speed;
                    for (int i = 0; i < 3; ++i)
                        s.velocity[i] = static_cast<fx>((s.velocity[i] * scale) >> kFxShift);
                }
                s.velocity[2] = kJumpSpeed;
                halfGravity();
                s.jumpHeld = true;
            }

            /// The ladder the player is touching, as an index; -1 if none.
            int findLadder() const
            {
                for (int i = 0; i < world.ladderCount; ++i)
                {
                    const LadderVolume &ladder = world.ladders[i];
                    if (!ladder.active)
                        continue;
                    bool touching = true;
                    for (int a = 0; a < 3 && touching; ++a)
                    {
                        const fx half = (a == 2 ? detail::kPlayerHalfHeight[s.ducked ? 1 : 0]
                                                : detail::kPlayerHalfWidth);
                        touching = s.origin[a] + half >= ladder.mins[a] &&
                                   s.origin[a] - half <= ladder.maxs[a];
                    }
                    if (touching)
                        return i;
                }
                return -1;
            }

            /// Which way the ladder faces, as seen from the player: the side of
            /// its volume the player is on. Unit, Q30, horizontal.
            void ladderNormal(const LadderVolume &ladder, std::int32_t *normal) const
            {
                // How far outside the volume the player's centre is along each
                // horizontal axis (negative: inside). The face they are in
                // front of is the one they are furthest outside of.
                fx best = -0x7FFFFFFF;
                int axis = 0;
                int sign = 1;
                for (int a = 0; a < 2; ++a)
                {
                    const fx below = ladder.mins[a] - s.origin[a];
                    const fx above = s.origin[a] - ladder.maxs[a];
                    if (below > best)
                    {
                        best = below;
                        axis = a;
                        sign = -1;
                    }
                    if (above > best)
                    {
                        best = above;
                        axis = a;
                        sign = 1;
                    }
                }
                normal[0] = normal[1] = normal[2] = 0;
                normal[axis] = sign * kNormalOne;
            }

            /// On a ladder there is no gravity and no friction: the keys set
            /// the velocity outright. What would carry the player INTO the
            /// ladder carries them UP it instead. Returns false if the player
            /// jumped off, and so is no longer climbing.
            bool ladderMove(const MoveCommand &cmd, const LadderVolume &ladder)
            {
                std::int32_t normal[3];
                ladderNormal(ladder, normal);
                if (cmd.jump)
                {
                    for (int a = 0; a < 3; ++a)
                    {
                        s.velocity[a] = static_cast<fx>(
                            (static_cast<std::int64_t>(normal[a]) * kLadderJumpSpeed) >>
                            kNormalShift);
                    }
                    s.jumpHeld = true;
                    return false;
                }
                s.jumpHeld = false;
                if (cmd.forward == 0 && cmd.side == 0)
                {
                    s.velocity[0] = s.velocity[1] = s.velocity[2] = 0;
                    return true;
                }
                fx wish[3];
                for (int a = 0; a < 3; ++a)
                {
                    wish[a] = fxMul(cmd.viewForward[a], kClimbSpeed * cmd.forward) +
                              fxMul(cmd.viewRight[a], kClimbSpeed * cmd.side);
                }
                // Split into the part along the ladder's normal and the rest.
                // The normal is horizontal, so "up the ladder" is straight up.
                const fx into = dotNormal(wish, normal);
                for (int a = 0; a < 3; ++a)
                {
                    const fx along = static_cast<fx>(
                        (static_cast<std::int64_t>(normal[a]) * into) >> kNormalShift);
                    s.velocity[a] = wish[a] - along;
                }
                s.velocity[2] -= into;
                // Standing on the floor and pushing away from the ladder: step
                // off it, rather than hang on by the fingertips.
                if (s.onGround && into > 0)
                {
                    for (int a = 0; a < 3; ++a)
                    {
                        s.velocity[a] += static_cast<fx>(
                            (static_cast<std::int64_t>(normal[a]) * kClimbSpeed) >> kNormalShift);
                    }
                }
                return true;
            }

            void finishDuck()
            {
                s.ducked = true;
                s.ducking = false;
                s.viewHeight = kDuckViewHeight;
                if (s.onGround)
                {
                    // The crouched hull's centre is lower: keep the feet where
                    // they are.
                    s.origin[2] -= kDuckDrop;
                    // If that is somehow inside something, come back up.
                    for (int i = 0; i < 36 && !hullPositionFree(hull(), s.origin); ++i)
                        s.origin[2] += kFxOne;
                    categorizePosition();
                }
            }

            void unDuck()
            {
                fx stand[3] = {s.origin[0], s.origin[1], s.origin[2]};
                if (s.onGround)
                    stand[2] += kDuckDrop;
                // Room to stand? If not, stay down: this is what keeps the
                // player crouched inside a vent, or under a grate.
                if (!positionFree(world, false, stand))
                    return;
                s.ducked = false;
                s.ducking = false;
                s.duckMs = 0;
                s.viewHeight = kViewHeight;
                for (int i = 0; i < 3; ++i)
                    s.origin[i] = stand[i];
                categorizePosition();
            }

            void duck(const MoveCommand &cmd, int ms)
            {
                const bool pressed = cmd.duck && !s.duckHeld;
                s.duckHeld = cmd.duck;

                if (!cmd.duck)
                {
                    if (s.ducked || s.ducking)
                        unDuck();
                    if (!s.ducked)
                        s.ducking = false;
                    return;
                }
                if (s.ducked)
                    return;
                if (pressed || !s.ducking)
                {
                    s.ducking = true;
                    s.duckMs = 0;
                }
                s.duckMs += ms;
                if (s.duckMs >= kDuckTimeMs || !s.onGround)
                {
                    finishDuck(); // in the air the legs come up at once
                    return;
                }
                // On the way down the eyes follow a smooth-step curve from
                // standing height to crouched (measured from the same origin,
                // so 18 below the crouched view height).
                const fx t = (s.duckMs << kFxShift) / kDuckTimeMs;
                const fx t2 = fxMul(t, t);
                const fx smooth = 3 * t2 - 2 * fxMul(t2, t);
                s.viewHeight =
                    kViewHeight + fxMul(kDuckViewHeight - kDuckDrop - kViewHeight, smooth);
            }

            /// The move starts inside solid: look for somewhere close that is
            /// not. False if there is nowhere.
            /// Just inside a brush entity -- by the last place of a number,
            /// in a corner of one that turns: edged back out, by the
            /// smallest step in any direction that does it. This matters
            /// more than it looks: a move that STARTS inside a brush does not
            /// collide with it at all, so a player a hair inside a tram's
            /// wall would walk through it, and one a hair inside its floor
            /// would fall. (Deeper than a unit is not this function's
            /// business: that is a door closing on someone.)
            void nudgeOutOfMovers()
            {
                if (positionFree(world, s.ducked, s.origin))
                    return;
                static constexpr std::int8_t kWays[10][3] = {
                    {0, 0, 1},  {1, 0, 0},  {-1, 0, 0}, {0, 1, 0},  {0, -1, 0},
                    {1, 1, 0},  {1, -1, 0}, {-1, 1, 0}, {-1, -1, 0}, {0, 0, -1}};
                for (fx reach = kFxOne / 64; reach <= kFxOne; reach *= 2)
                {
                    for (const auto &way : kWays)
                    {
                        const fx test[3] = {s.origin[0] + way[0] * reach, s.origin[1] + way[1] * reach,
                                            s.origin[2] + way[2] * reach};
                        if (positionFree(world, s.ducked, test))
                        {
                            for (int i = 0; i < 3; ++i)
                                s.origin[i] = test[i];
                            return;
                        }
                    }
                }
            }

            bool unstick()
            {
                static constexpr int kOffsets[][3] = {
                    {0, 0, 1},  {0, 0, -1}, {1, 0, 0},  {-1, 0, 0}, {0, 1, 0},  {0, -1, 0},
                    {0, 0, 2},  {2, 0, 0},  {-2, 0, 0}, {0, 2, 0},  {0, -2, 0}, {0, 0, 4},
                    {4, 0, 0},  {-4, 0, 0}, {0, 4, 0},  {0, -4, 0}, {0, 0, 8},  {0, 0, 16},
                    {8, 0, 0},  {-8, 0, 0}, {0, 8, 0},  {0, -8, 0}, {0, 0, 24}, {0, 0, 36}};
                // Somewhere clear of the brushes too, if there is such a place:
                // out of a wall and into the side of the tram one is riding
                // is out of one trouble into another. Failing that, clear of
                // the world.
                for (int pass = 0; pass < 2; ++pass)
                {
                    for (const auto &offset : kOffsets)
                    {
                        const fx test[3] = {s.origin[0] + fxFromInt(offset[0]),
                                            s.origin[1] + fxFromInt(offset[1]),
                                            s.origin[2] + fxFromInt(offset[2])};
                        if (pass == 0 ? positionFree(world, s.ducked, test)
                                      : hullPositionFree(hull(), test))
                        {
                            for (int i = 0; i < 3; ++i)
                                s.origin[i] = test[i];
                            return true;
                        }
                    }
                }
                return false;
            }

            /// Stuck fast, and for a while: somewhere clear is looked for
            /// much farther off -- every way, a body's width at a time, out
            /// to 128 units. Being put a few steps away is a small price
            /// for not being held in a wall for good (which nothing in the
            /// game gets anyone out of).
            bool unstickFar()
            {
                static constexpr int kReach[] = {16, 32, 48, 64, 96, 128};
                for (int reach : kReach)
                {
                    // (Up first: most of what one is stuck in is underfoot.)
                    for (int z = 1; z >= -1; --z)
                    {
                        for (int y = -1; y <= 1; ++y)
                        {
                            for (int x = -1; x <= 1; ++x)
                            {
                                if (x == 0 && y == 0 && z == 0)
                                    continue;
                                const fx test[3] = {s.origin[0] + fxFromInt(x * reach), s.origin[1] + fxFromInt(y * reach),
                                                    s.origin[2] + fxFromInt(z * reach)};
                                if (positionFree(world, s.ducked, test))
                                {
                                    for (int i = 0; i < 3; ++i)
                                        s.origin[i] = test[i];
                                    return true;
                                }
                            }
                        }
                    }
                }
                return false;
            }
        };
    } // namespace detail

    /// Advance the player by one time step. `dt` is Q16 seconds; the game runs
    /// it at a fixed 30 Hz (kPlayerTick).
    inline void playerMove(PlayerState &state, const MoveCommand &cmd, const MoveWorld &world,
                           std::int32_t dt)
    {
        detail::Mover m{state, world, dt};
        // (A landing is known by the end of the move: in the air before,
        // on the ground after, and how fast it was coming down.)
        struct Landing
        {
            PlayerState &state;
            const bool inAir;
            const fx falling;
            ~Landing()
            {
                state.landed = inAir && state.onGround && state.waterLevel == 0 && !state.onLadder && falling < 0
                                   ? -falling
                                   : 0;
            }
        } landing{state, !state.onGround, state.velocity[2]};
        state.facingXY[0] = cmd.forwardXY[0];
        state.facingXY[1] = cmd.forwardXY[1];
        const int ms = static_cast<int>((static_cast<std::int64_t>(dt) * 1000) >> kTimeShift);

        state.stuck = false;
        state.touchedMover = -1;
        if (!hullPositionFree(m.hull(), state.origin) && !m.unstick())
        {
            state.stuckMs += ms;
            if (state.stuckMs < 500 || !m.unstickFar())
            {
                state.stuck = true;
                state.velocity[0] = state.velocity[1] = state.velocity[2] = 0;
                return;
            }
        }
        state.stuckMs = 0;

        m.nudgeOutOfMovers();
        m.categorizePosition();
        m.duck(cmd, ms);

        const int ladder = m.findLadder();
        state.onLadder = ladder >= 0 && m.ladderMove(cmd, world.ladders[ladder]);
        if (state.onLadder)
        {
            m.checkVelocity();
            m.flyMove();
            m.categorizePosition();
            return;
        }

        m.checkWater();
        m.waterHurts(ms);

        // On the way out of the water onto a ledge: carried up and forwards,
        // deaf to the keys, until landed, clear of the water or out of time.
        if (state.waterJumpMs > 0)
        {
            state.waterJumpMs -= ms;
            if (state.waterJumpMs <= 0 || state.waterLevel == 0 || state.onGround)
            {
                state.waterJumpMs = 0;
            }
            else
            {
                state.velocity[0] = state.waterJumpDir[0];
                state.velocity[1] = state.waterJumpDir[1];
                m.halfGravity();
                m.flyMove();
                m.categorizePosition();
                m.halfGravity();
                return;
            }
        }

        if (state.waterLevel >= 2)
        {
            if (state.waterLevel == 2)
                m.checkWaterJump(cmd);
            // Jump is "swim up": faster in water than in what burns.
            if (cmd.jump && state.waterJumpMs == 0)
                state.velocity[2] = state.waterType == kContentsWater
                                        ? fxFromInt(100)
                                        : (state.waterType == kContentsSlime ? fxFromInt(80)
                                                                             : fxFromInt(50));
            state.onGround = false;
            state.groundMover = -1;
            m.waterMove(cmd);
            m.categorizePosition();
            return;
        }

        m.halfGravity();
        m.jump(cmd);
        if (state.onGround)
        {
            state.velocity[2] = 0;
            m.friction();
        }
        m.checkVelocity();

        if (state.onGround)
            m.walkMove(cmd);
        else
            m.airMove(cmd);

        m.categorizePosition();
        m.checkVelocity();
        m.halfGravity();
        if (state.onGround)
            state.velocity[2] = 0;
    }

    /// What the start of every move does about a player found a hair inside a
    /// brush entity: on its own, for whoever has to judge where the player is
    /// between moves (the tests).
    inline void playerSettle(PlayerState &state, const MoveWorld &world)
    {
        detail::Mover m{state, world, 0};
        m.nudgeOutOfMovers();
    }

    /// One thirtieth of a second, Q16.
    inline constexpr std::int32_t kPlayerTick = 2185;
    /// How many footsteps were heard in a move that took the player from
    /// `before` to where they are now: one for every ninety units walked on
    /// the ground at more than a creep (crouched, nobody hears them).
    inline int playerFootsteps(PlayerState &s, const fx *before)
    {
        const fx d[3] = {s.origin[0] - before[0], s.origin[1] - before[1], 0};
        const fx moved = fxLength(d);
        const fx speed[3] = {s.velocity[0], s.velocity[1], 0};
        if (!s.onGround || s.ducked || s.waterLevel >= 2 || fxLength(speed) < fxFromInt(120) ||
            moved > fxFromInt(64))
        {
            // (A jump in place over 64 units is a teleport, not a stride.)
            return 0;
        }
        int steps = 0;
        s.stepLeft -= moved;
        while (s.stepLeft <= 0)
        {
            s.stepLeft += fxFromInt(90);
            ++steps;
        }
        return steps;
    }

    /// What the player's going sounded like over a move from `before`: how
    /// many steps, and of which kind. The game's own rules: a rung for every
    /// so far up or down a ladder, a wade for every stride through water to
    /// the waist or deeper (swum or walked), a slosh where it is only round
    /// the feet, and otherwise a footstep.
    enum class StepKind
    {
        Floor,
        Ladder,
        Slosh,
        Wade
    };
    struct StepsHeard
    {
        int count = 0;
        StepKind kind = StepKind::Floor;
    };
    inline StepsHeard playerSteps(PlayerState &s, const fx *before)
    {
        StepsHeard heard;
        const fx all[3] = {s.origin[0] - before[0], s.origin[1] - before[1], s.origin[2] - before[2]};
        if (fxLength(all) > fxFromInt(64))
            return heard; // (a teleport, not a stride)
        fx moved = 0, stride = fxFromInt(90);
        if (s.onLadder)
        {
            // (Up and down count on a ladder, and nothing else does.)
            heard.kind = StepKind::Ladder;
            moved = all[2] < 0 ? -all[2] : all[2];
            stride = fxFromInt(48);
        }
        else if (s.waterLevel >= 2)
        {
            heard.kind = StepKind::Wade;
            moved = fxLength(all);
            stride = fxFromInt(120);
            const fx speed[3] = {s.velocity[0], s.velocity[1], s.velocity[2]};
            if (fxLength(speed) < fxFromInt(60))
                moved = 0;
        }
        else
        {
            heard.kind = s.waterLevel == 1 ? StepKind::Slosh : StepKind::Floor;
            heard.count = playerFootsteps(s, before);
            return heard;
        }
        s.stepLeft -= moved;
        while (s.stepLeft <= 0)
        {
            s.stepLeft += stride;
            ++heard.count;
        }
        return heard;
    }
} // namespace hldata

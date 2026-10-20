// Soldiers and tripmines (shared/HlGrunt.hpp, and the mines of
// shared/HlMonsters.hpp), on the game's own maps: HLDS_VALVE_DIR says where.
//
// The mines of c1a3 arm, are tripped by walking through their beams, by a
// shot, and by each other. A soldier is cooked with the head, gun and skin
// that go with what he carries; he sees, fires in threes, reloads, kicks,
// is saved by his helmet, dies, comes apart. The squads of c1a3a are the
// map's; no more than two of a squad fire at once; cover is somewhere the
// player cannot see; grenades keep to the game's rules.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "HlThink.hpp"
#include "TestGame.hpp"

using namespace hlds;
using namespace hldata;
using namespace hldstest;

namespace
{
    std::unique_ptr<Game> load(const char *dir, const char *map)
    {
        auto game = std::make_unique<Game>();
        std::string error;
        if (!game->bsp.load(std::string(dir) + "/maps/" + map + ".bsp", error))
            return nullptr;
        finish(*game, nullptr, dir);
        if (!game->ok)
            return nullptr;
        return game;
    }

    /// The game by itself, the player held where they are put.
    void tickStill(Game &g, int ms = 33)
    {
        logicTick(g.logic, g.player, ms);
        g.logic.viewTurn = 0;
        monstersTick(g.logic, g.player, ms);
        g.drain();
    }

    bool isKind(const ActorState &actor, ActorKind kind)
    {
        return actor.active && actor.kind == static_cast<std::uint8_t>(kind);
    }

    int findKind(const Game &g, ActorKind kind, int nth = 0)
    {
        for (int i = 0; i < g.actors.count; ++i)
        {
            if (isKind(g.actors.states[i], kind) && nth-- == 0)
                return i;
        }
        return -1;
    }

    /// Everything that fights but `keep` (and what is of `also`) is taken
    /// away: one thing at a time is looked at.
    void only(Game &g, int keep, ActorKind also = ActorKind::Count)
    {
        for (int i = 0; i < g.actors.count; ++i)
        {
            ActorState &actor = g.actors.states[i];
            if (i == keep || !fights(actor.kind) || actor.kind == static_cast<std::uint8_t>(also))
                continue;
            actor.active = false;
            actorPlaceBox(g.actors, i);
        }
    }

    /// Stand the player on the floor about `distance` units from the
    /// soldier, where each can see the other; and turn him to face them.
    bool standBefore(Game &g, int index, int distance)
    {
        ActorState &me = g.actors.states[index];
        const fx gun[3] = {me.pos[0], me.pos[1], me.pos[2] + fxFromInt(60)};
        for (int turn = 0; turn < 32; ++turn)
        {
            fx sine, cosine;
            fxSinCos(static_cast<std::uint16_t>(turn * 2048), sine, cosine);
            ActorState probe;
            probe.pos[0] = me.pos[0] + cosine * distance;
            probe.pos[1] = me.pos[1] + sine * distance;
            probe.pos[2] = me.pos[2] + fxFromInt(8);
            if (!actorToFloor(g.move, probe, fxFromInt(8), fxFromInt(64)))
                continue;
            const fx at[3] = {probe.pos[0], probe.pos[1], probe.pos[2] + kStandingFeet};
            if (!lineClear(g.move, gun, at) || probe.pos[2] - me.pos[2] > fxFromInt(24) ||
                me.pos[2] - probe.pos[2] > fxFromInt(24))
                continue;
            for (int k = 0; k < 3; ++k)
            {
                g.player.origin[k] = at[k];
                g.player.velocity[k] = 0;
            }
            g.player.onGround = true;
            me.yaw = fxAtan2Turns(at[1] - me.pos[1], at[0] - me.pos[0]);
            return true;
        }
        return false;
    }

    int flat(const fx *a, const fx *b)
    {
        const fx d[3] = {a[0] - b[0], a[1] - b[1], 0};
        return fxToInt(fxLength(d));
    }

    // ------------------------------------------------------------- tripmines

    void testMines(const char *dir)
    {
        auto game = load(dir, "c1a3");
        check(game != nullptr, "c1a3 cooks");
        if (!game)
            return;
        Game &g = *game;
        only(g, -1, ActorKind::Tripmine);
        int mines[8], count = 0;
        for (int i = 0; i < g.actors.count && count < 8; ++i)
        {
            if (isKind(g.actors.states[i], ActorKind::Tripmine))
                mines[count++] = i;
        }
        check(count == 3, "c1a3 has its three tripmines");
        if (count != 3)
            return;
        const fx home[3] = {g.player.origin[0], g.player.origin[1], g.player.origin[2]};
        for (int ms = 0; ms < 900; ms += 33)
            tickStill(g);
        bool early = false;
        for (int n = 0; n < count; ++n)
            early = early || g.actors.states[mines[n]].ai == kMineArmed;
        for (int ms = 900; ms < 3000; ms += 33)
            tickStill(g);
        bool armed = true, sane = true;
        for (int n = 0; n < count; ++n)
        {
            const ActorState &mine = g.actors.states[mines[n]];
            armed = armed && mine.ai == kMineArmed;
            const int length = flat(mine.goal, mine.pos);
            std::printf("test_soldiers: tripmine at %d %d %d: beam of %d units\n", fxToInt(mine.pos[0]),
                        fxToInt(mine.pos[1]), fxToInt(mine.pos[2]), length);
            sane = sane && length > 16 && length < kMineRange;
        }
        check(!early && armed && g.times(CommonSound::MineActivate) == 3,
              "they are armed two and a half seconds after the map starts, not before");
        check(sane, "each beam ends at the wall across from it");
        for (int ms = 0; ms < 3000; ms += 33)
            tickStill(g);
        bool quiet = true;
        for (int n = 0; n < count; ++n)
            quiet = quiet && g.actors.states[mines[n]].ai == kMineArmed;
        check(quiet && g.times(CommonSound::Explode) == 0, "left alone they stay as they are");

        // Through the first one's beam.
        {
            const ActorState &mine = g.actors.states[mines[0]];
            for (int k = 0; k < 3; ++k)
                g.player.origin[k] = (mine.pos[k] + mine.goal[k]) / 2;
            g.player.health = 1000;
            const fx d[3] = {g.player.origin[0] - mine.pos[0], g.player.origin[1] - mine.pos[1],
                             g.player.origin[2] - mine.pos[2]};
            const int far = fxToInt(fxLength(d));
            int took = -1;
            for (int ms = 0; ms < 1000 && took < 0; ms += 33)
            {
                tickStill(g);
                if (!mine.active)
                    took = ms;
            }
            std::printf("test_soldiers: walked into a beam %d units from its mine: off after %d ms, %d taken\n", far,
                        took, 1000 - g.player.health);
            check(took >= 100 && took <= 500 && g.times(CommonSound::Explode) == 1,
                  "a mine whose beam is walked into goes off a moment later");
            check(far >= 375 ? g.player.health == 1000 : g.player.health < 1000,
                  "... and hurts whoever is within 375 units of it");
        }
        for (int k = 0; k < 3; ++k)
            g.player.origin[k] = home[k];
        g.player.health = 100;
        // (The blast may have taken the others with it: they stand close.)
        int left = 0;
        for (int ms = 0; ms < 1000; ms += 33)
            tickStill(g);
        for (int n = 1; n < count; ++n)
            left += g.actors.states[mines[n]].active ? 1 : 0;
        std::printf("test_soldiers: %d of the other two are left after the first went off\n", left);
        const int heard = g.times(CommonSound::Explode);
        for (int n = 1; n < count; ++n)
        {
            const ActorState &mine = g.actors.states[mines[n]];
            if (!mine.active)
                continue;
            // A shot at it, from a little way along its own beam.
            fx forward[3];
            hldata::detail::turretForward(mine, forward);
            const fx eye[3] = {mine.pos[0] + forward[0] * 12, mine.pos[1] + forward[1] * 12,
                               mine.pos[2] + forward[2] * 12};
            const fx back[3] = {-forward[0], -forward[1], -forward[2]};
            const bool hit = fireLine(g.logic, g.player, eye, back, 64, 8, true);
            for (int ms = 0; ms < 500; ms += 33)
                tickStill(g);
            check(hit && !mine.active, "a mine that is shot goes off");
            break;
        }
        // And what is left, by a blast near it.
        bool chained = true;
        for (int n = 1; n < count; ++n)
        {
            const ActorState &mine = g.actors.states[mines[n]];
            if (!mine.active)
                continue;
            fx forward[3];
            hldata::detail::turretForward(mine, forward);
            const fx at[3] = {mine.pos[0] + forward[0] * 40, mine.pos[1] + forward[1] * 40,
                              mine.pos[2] + forward[2] * 40};
            radiusDamage(g.logic, g.player, at, 50, 125, -1);
            for (int ms = 0; ms < 500; ms += 33)
                tickStill(g);
            chained = chained && !mine.active;
        }
        check(chained && g.times(CommonSound::Explode) > heard, "... and so does one caught in a blast");
    }

    // ------------------------------------------------------------- one soldier

    void testCooked(const char *dir)
    {
        // What each carries, by the map's own count of them.
        struct Map
        {
            const char *name;
            int soldiers, shotguns, launchers, leaders;
        };
        for (const Map &map : {Map{"c1a3a", 10, -1, -1, 3}, Map{"c2a2b1", 14, -1, -1, 0}})
        {
            auto game = load(dir, map.name);
            check(game != nullptr, "a map of soldiers cooks");
            if (!game)
                continue;
            Game &g = *game;
            int soldiers = 0, shotguns = 0, launchers = 0, leaders = 0, wrong = 0, triggers = 0;
            for (std::uint32_t i = 0; i < g.view.actorCount; ++i)
            {
                const ActorRecord &record = g.view.actors[i];
                if (record.kind != static_cast<std::uint8_t>(ActorKind::Grunt))
                    continue;
                ++soldiers;
                const std::uint32_t weapons = record.item & 15;
                triggers += record.target != 0 && ((record.item >> kGruntTriggerShift) & 15) != 0;
                const int head = record.body % 4, gun = record.body / 4;
                const int skin = static_cast<int>((record.item >> kActorSkinShift) & 0xFF);
                shotguns += (weapons & kGruntShotgun) ? 1 : 0;
                launchers += (weapons & kGruntLauncher) ? 1 : 0;
                leaders += (record.flags & kActorLeader) ? 1 : 0;
                if (weapons & kGruntShotgun)
                    wrong += head != 2 || gun != 1;
                else if (weapons & kGruntLauncher)
                    wrong += head != 3 || gun != 0 || skin != 1;
                else
                    wrong += head != 0 || gun != 0;
            }
            std::printf("test_soldiers: %s: %d soldiers (%d with shotguns, %d with launchers), %d leaders, "
                        "%d that set something off; %u lines to call out\n",
                        map.name, soldiers, shotguns, launchers, leaders, triggers, g.view.speechCount);
            check(soldiers == map.soldiers && leaders == map.leaders && wrong == 0,
                  "its soldiers are cooked, each with the head and gun of what he carries");
            check(g.view.speechCount == static_cast<std::uint32_t>(kSpeechGroups * kSpeechLines) &&
                      g.view.speech[kSpeakAlert * kSpeechLines] != 0xFFFF &&
                      g.view.speech[kSpeakGrenade * kSpeechLines] != 0xFFFF,
                  "... and with what soldiers call out");
            const int one = findKind(g, ActorKind::Grunt);
            const ModelView *model = one >= 0 ? actorModel(g.actors, g.actors.states[one]) : nullptr;
            check(model != nullptr && model->findClip(nameHash("standing_mp5")) >= 0 &&
                      model->findClip(nameHash("reload_mp5")) >= 0 && model->findClip(nameHash("frontkick")) >= 0 &&
                      model->findClip(nameHash("dieheadshot")) >= 0 && model->header->hitboxCount != 0,
                  "his model has the clips he fights with, and his hit boxes");
            check(g.actors.humanGibs >= 0, "... and the map has the pieces a person comes apart into");
        }
    }

    void testSoldier(const char *dir)
    {
        auto game = load(dir, "c1a3d");
        check(game != nullptr, "c1a3d cooks");
        if (!game)
            return;
        Game &g = *game;
        const int index = findKind(g, ActorKind::Grunt);
        check(index >= 0, "c1a3d has its soldier");
        if (index < 0)
            return;
        only(g, index);
        ActorState &me = g.actors.states[index];
        g.player.origin[2] += fxFromInt(30000);
        tickStill(g);
        check(me.health == kGruntHealth && me.ammo == kGruntClip && me.squad < 0 && me.ai == kAiIdle,
              "a soldier by himself: fifty health, thirty-six rounds, no squad, minding his business");

        // His helmet, his head, and the rest of him.
        gruntHurt(g.logic, index, 15, true, kHitHelmet);
        check(me.health == kGruntHealth, "his helmet stops what is not twenty");
        me.ai = kAiIdle;
        me.mood &= static_cast<std::uint8_t>(~kMoodHurt);
        gruntHurt(g.logic, index, 25, true, kHitHelmet);
        check(me.health == kGruntHealth - 15, "... and takes twenty off what is more (the rest counts three times)");
        me.health = kGruntHealth;
        gruntHurt(g.logic, index, 5, true, kHitHead);
        gruntHurt(g.logic, index, 5, true, kHitBody);
        check(me.health == kGruntHealth - 20, "a round in the head counts three times, one anywhere else once");
        check(g.times(CommonSound::GruntPain1) + g.times(CommonSound::GruntPain2) <= 1, "he cries out once a second at most");
        g.drain();
        me.health = kGruntHealth;
        me.ai = kAiIdle;
        me.mood &= static_cast<std::uint8_t>(~(kMoodHurt | kMoodNew));
        me.cooldownMs = 0;

        // Seen: not from behind him, from in front.
        int range = 0;
        for (int distance : {400, 320, 240, 160})
        {
            if (range == 0 && standBefore(g, index, distance))
                range = distance;
        }
        check(range != 0, "somewhere in front of him to stand");
        me.yaw = static_cast<std::uint16_t>(me.yaw + 32768);
        for (int ms = 0; ms < 1000; ms += 33)
            tickStill(g);
        check(me.ai == kAiIdle, "someone behind him, and quiet, is not seen");
        me.yaw = static_cast<std::uint16_t>(me.yaw + 32768);
        int saw = -1;
        for (int ms = 0; ms < 1000 && saw < 0; ms += 33)
        {
            tickStill(g);
            if (me.ai != kAiIdle)
                saw = ms;
        }
        check(saw >= 0, "someone in front of him is");

        // He fires: in threes, a tenth of a second apart; four to a round
        // that lands; and when the clip is out he reloads.
        g.player.health = 100000;
        int last = me.ammo, rounds = 0, lastAt = -1000, threes = 0, inBurst = 0, wrongGap = 0;
        int health = g.player.health, hits = 0, badHit = 0, most = 0;
        bool reloaded = false, ranDry = false;
        int ms = 0;
        for (; ms < 40000 && !reloaded; ms += 33)
        {
            tickStill(g);
            // (Held where he is: this is about his gun.)
            if (me.ai == kAiChase && me.attack != kGoReload && me.attack != kGoCover)
                hldata::detail::gruntWait(g.actors, me, 200);
            if (me.ammo < last)
            {
                rounds += last - me.ammo;
                if (ms - lastAt > 250)
                    inBurst = 0;
                else if (ms - lastAt < 60 || ms - lastAt > 140)
                    ++wrongGap;
                inBurst += last - me.ammo;
                threes += inBurst == 3 ? 1 : 0;
                lastAt = ms;
            }
            else if (me.ammo > last)
            {
                reloaded = true;
            }
            ranDry = ranDry || me.ammo == 0;
            last = me.ammo;
            if (g.player.health < health)
            {
                const int took = health - g.player.health;
                hits += took / kGruntRound;
                badHit += took % kGruntRound != 0;
                most = took > most ? took : most;
                health = g.player.health;
            }
        }
        std::printf("test_soldiers: %d rounds in %d bursts of three in %d ms; %d landed at %d units; then %s\n", rounds,
                    threes, ms, hits, range, reloaded ? "reloaded" : "did NOT reload");
        check(rounds == kGruntClip && threes == kGruntClip / 3 && wrongGap == 0,
              "he fires his clip in bursts of three, a tenth of a second between rounds");
        check(hits > 0 && hits < rounds && badHit == 0 && most <= 3 * kGruntRound,
              "some land, not all: four each (nothing worn)");
        check(ranDry && reloaded && me.ammo == kGruntClip && g.times(CommonSound::GruntReload) == 1,
              "with the clip out he reloads");
        check(g.times(CommonSound::GruntFire1) + g.times(CommonSound::GruntFire2) == threes,
              "... his gun heard once a burst");

        // Close to: a kick.
        check(standBefore(g, index, 44), "somewhere right in front of him to stand");
        g.player.health = 100;
        g.player.velocity[0] = g.player.velocity[1] = g.player.velocity[2] = 0;
        hldata::detail::gruntWait(g.actors, me, 0);
        bool kicked = false;
        for (ms = 0; ms < 1500 && g.player.health == 100; ms += 33)
        {
            tickStill(g);
            kicked = kicked || (me.ai == kAiAttack && me.attack == kActKick);
        }
        const fx push[3] = {g.player.velocity[0], g.player.velocity[1], 0};
        check(kicked && g.player.health == 100 - kGruntKick && fxLength(push) > fxFromInt(50),
              "someone right in front of him is kicked: ten, and back they go");

        // Hurt badly, he limps.
        me.health = kGruntLimp;
        const fx away[3] = {me.pos[0], me.pos[1], me.pos[2]};
        hldata::detail::gruntGo(g.actors, me, away, kGoCover);
        const ModelView *model = actorModel(g.actors, me);
        check(model != nullptr && me.clip == model->findClip(nameHash("limpingrun")), "at twenty health he limps");

        // And dies, and stays dead. (What the map has him set off by it is
        // set off: here, something made up.)
        g.player.origin[2] += fxFromInt(30000);
        me.health = 5;
        me.fires = 1;
        me.item = static_cast<std::uint8_t>((me.item & 15) | (4 << kGruntTriggerShift));
        int queued = 0;
        for (const LogicEvent &event : g.logic.events)
            queued += event.active ? 1 : 0;
        const int cries = g.times(CommonSound::GruntDie1) + g.times(CommonSound::GruntDie2);
        gruntHurt(g.logic, index, 2, true, kHitHead);
        int after = 0;
        for (const LogicEvent &event : g.logic.events)
            after += event.active && event.target == 1 ? 1 : 0;
        check(after == 1 && queued + 1 >= after && me.fires == 0, "what he is to set off by dying is set off, once");
        check(me.ai == kAiDying && me.clip == model->findClip(nameHash("dieheadshot")) &&
                  !(me.flags & kActorSolid),
              "shot in the head he dies of it, as someone shot in the head");
        for (ms = 0; ms < 4000; ms += 33)
            tickStill(g);
        check(me.ai == kAiDead && me.active &&
                  g.times(CommonSound::GruntDie1) + g.times(CommonSound::GruntDie2) == cries + 1,
              "... and lies where he fell");
        // A blast by his body: it comes apart.
        const fx by[3] = {me.pos[0], me.pos[1], me.pos[2] + fxFromInt(16)};
        radiusDamage(g.logic, g.player, by, 100, 250, -1);
        int pieces = 0;
        for (const Gib &gib : g.actors.gibs)
            pieces += gib.active && gib.model == static_cast<std::uint16_t>(g.actors.humanGibs);
        check(!me.active && pieces == 5 && g.times(CommonSound::BodySplat) == 0,
              "a blast by his body takes it apart: the skull and four more");
        tickStill(g);
        check(g.times(CommonSound::BodySplat) == 1, "... heard");
    }

    // ------------------------------------------------------------- squads

    /// `deferred`: their long thinking (cover, ways somewhere) asked for and
    /// worked out between ticks, as the console's worker does it (HlThink.hpp)
    /// -- the same fight, by the same rules.
    void testSquads(const char *dir, bool deferred = false)
    {
        auto game = load(dir, "c1a3a");
        if (!game)
            return;
        Game &g = *game;
        std::vector<ThinkSlot> slots(static_cast<std::size_t>(g.actors.count));
        ThinkQueue queue;
        queue.slots = slots.data();
        queue.count = g.actors.count;
        queue.deferred = deferred;
        int answered = 0;
        if (deferred)
            g.actors.think = &queue;
        g.player.origin[2] += fxFromInt(30000);
        tickStill(g);
        // The map's three squads of three, each under the one it flags.
        int squads = 0, members = 0, alone = 0, berets = 0;
        for (int i = 0; i < g.actors.count; ++i)
        {
            const ActorState &actor = g.actors.states[i];
            if (!isKind(actor, ActorKind::Grunt))
                continue;
            if (actor.squad < 0)
            {
                ++alone;
                continue;
            }
            ++members;
            if (actor.squad == i)
            {
                ++squads;
                berets += actor.body % 4 == 1 && (g.view.actors[i].flags & kActorLeader);
            }
        }
        std::printf("test_soldiers: c1a3a: %d squads, %d in them, %d alone\n", squads, members, alone);
        check(squads == 3 && members == 9 && alone == 1 && berets == 3,
              "c1a3a: three squads of three, each led by the one the map flags (in his beret); one alone");

        // A fight with one of them. Watched throughout: never more than two
        // of a squad firing; whoever runs for cover runs to somewhere the
        // player cannot see; no grenade from within 256 units, or onto a
        // squad-mate, or within six seconds of his last.
        int leader = -1;
        for (int i = 0; i < g.actors.count && leader < 0; ++i)
        {
            if (isKind(g.actors.states[i], ActorKind::Grunt) && g.actors.states[i].squad == i &&
                standBefore(g, i, 500))
                leader = i;
        }
        check(leader >= 0, "somewhere to stand in front of a squad's leader");
        if (leader < 0)
            return;
        only(g, -1, ActorKind::Grunt);
        for (int i = 0; i < g.actors.count; ++i)
        {
            ActorState &actor = g.actors.states[i];
            if (isKind(actor, ActorKind::Grunt) && actor.squad != leader)
            {
                actor.active = false;
                actorPlaceBox(g.actors, i);
            }
        }
        const fx stand[3] = {g.player.origin[0], g.player.origin[1], g.player.origin[2]};
        std::vector<std::uint8_t> wasAi(g.actors.count, 0), wasAct(g.actors.count, 0);
        std::vector<int> lastToss(g.actors.count, -100000);
        std::vector<std::uint8_t> first(g.actors.count, 0);
        int mostFiring = 0, covers = 0, badCover = 0, tosses = 0, badToss = 0, signals = 0, woke = 0;
        bool leaderFirst = false, othersHid = false;
        for (int ms = 0; ms < 30000; ms += 33)
        {
            g.frame();
            // (The worker's turn: what was asked this tick, worked out
            // before the next.)
            while (deferred && thinkRunOne(g.logic))
                ++answered;
            g.player.health = 100;
            for (int k = 0; k < 3; ++k)
                g.player.origin[k] = stand[k];
            g.player.velocity[0] = g.player.velocity[1] = g.player.velocity[2] = 0;
            int firing = 0;
            const fx eye[3] = {stand[0], stand[1], stand[2] + g.player.viewHeight};
            for (int i = 0; i < g.actors.count; ++i)
            {
                const ActorState &actor = g.actors.states[i];
                if (!isKind(actor, ActorKind::Grunt) || actor.ai >= kAiDying)
                    continue;
                const bool changed = actor.ai != wasAi[i] || actor.attack != wasAct[i];
                if (changed && std::getenv("HL_DEBUG") != nullptr)
                    std::printf("  %5d ms: #%d ai %d act %d ammo %d health %d at %d %d (%d from the player)%s\n", ms, i,
                                actor.ai, actor.attack, actor.ammo, actor.health, fxToInt(actor.pos[0]),
                                fxToInt(actor.pos[1]), flat(actor.pos, stand), i == leader ? " leader" : "");
                if (actor.ai == kAiAttack && (actor.attack == kActBurst || actor.attack == kActSignal))
                    ++firing;
                if (changed && wasAi[i] == kAiIdle && ms < 3000)
                    ++woke;
                // (What each does first, once it has turned to look.)
                if (changed && wasAi[i] == kGruntWait && !(first[i]))
                {
                    first[i] = 1;
                    if (i == leader)
                        leaderFirst = actor.ai == kAiAttack && (actor.attack == kActSignal || actor.attack == kActBurst);
                    else
                        othersHid = othersHid || (actor.ai == kAiChase && actor.attack == kGoCover);
                }
                if (changed && actor.ai == kAiAttack && actor.attack == kActSignal)
                    ++signals;
                if (changed && actor.ai == kAiChase && actor.attack == kGoCover)
                {
                    ++covers;
                    const fx there[3] = {actor.target[0], actor.target[1], actor.target[2] + fxFromInt(50)};
                    badCover += lineClear(g.move, eye, there) ? 1 : 0;
                }
                if (changed && actor.ai == kAiAttack && actor.attack == kActToss)
                {
                    ++tosses;
                    bool bad = flat(actor.pos, stand) <= 200 || ms - lastToss[i] < 6000;
                    lastToss[i] = ms;
                    badToss += bad ? 1 : 0;
                }
                wasAi[i] = actor.ai;
                wasAct[i] = actor.attack;
            }
            mostFiring = firing > mostFiring ? firing : mostFiring;
        }
        std::printf("test_soldiers: c1a3a, thirty seconds with a squad%s: %d woke; at most %d firing at once; "
                    "%d runs for cover (%d to somewhere in sight); %d grenades thrown (%d against the rules); "
                    "%d lines spoken; %d questions answered outside the frame\n",
                    deferred ? " (thinking outside the frame)" : "", woke, mostFiring, covers, badCover, tosses,
                    badToss, g.sentencesHeard, answered);
        if (deferred)
            check(answered > 0, "their thinking is done outside the frame");
        check(woke == 3 && leaderFirst, "the whole squad learns of the player at once, and its leader opens fire");
        check(othersHid, "... while the others make for cover");
        check(signals == 1, "... with a wave of the hand first, the once");
        check(mostFiring >= 1 && mostFiring <= 2, "never more than two of a squad fire at once");
        check(covers > 0 && badCover == 0, "whoever runs for cover runs to somewhere the player cannot see");
        check(badToss == 0, "grenades keep to the rules: not from close to, not twice in six seconds");
        check(g.sentencesHeard > 0, "they call out");
    }

    /// A hand grenade, by the game's rules: thrown in an arc that comes
    /// down by the player, three and a half seconds on the fuse; not from
    /// within 256 units, not onto one of his own, not twice in six seconds.
    void testGrenades(const char *dir)
    {
        auto game = load(dir, "c1a3a");
        if (!game)
            return;
        Game &g = *game;
        g.player.origin[2] += fxFromInt(30000);
        tickStill(g);
        int index = -1;
        for (int i = 0; i < g.actors.count && index < 0; ++i)
        {
            ActorState &one = g.actors.states[i];
            if (!isKind(one, ActorKind::Grunt) || !(one.item & kGruntGrenades))
                continue;
            // (Somewhere a throw can be made from: no beam overhead.)
            for (int distance : {500, 420, 600})
            {
                if (index >= 0 || !standBefore(g, i, distance))
                    continue;
                for (int k = 0; k < 3; ++k)
                    one.seen[k] = g.player.origin[k];
                for (int n = 0; n < 8 && index < 0; ++n)
                {
                    one.grenadeMs = 0;
                    if (hldata::detail::gruntGrenade(g.logic, g.player, i, true))
                        index = i;
                }
            }
        }
        check(index >= 0, "a soldier with grenades, and somewhere in the open in front of him");
        if (index < 0)
            return;
        ActorState &me = g.actors.states[index];
        // One of his squad is kept (moved about below); the rest go.
        int mate = -1;
        for (int i = 0; i < g.actors.count; ++i)
        {
            ActorState &actor = g.actors.states[i];
            if (i == index || !fights(actor.kind))
                continue;
            if (mate < 0 && isKind(actor, ActorKind::Grunt) && actor.squad == me.squad && me.squad >= 0)
            {
                mate = i;
                continue;
            }
            actor.active = false;
            actorPlaceBox(g.actors, i);
        }
        for (int k = 0; k < 3; ++k)
            me.seen[k] = g.player.origin[k];
        const fx stand[3] = {g.player.origin[0], g.player.origin[1], g.player.origin[2]};
        fx home[3] = {0, 0, 0};
        if (mate >= 0)
        {
            for (int k = 0; k < 3; ++k)
                home[k] = g.actors.states[mate].pos[k];
            // By the player: no grenade.
            for (int k = 0; k < 3; ++k)
                g.actors.states[mate].pos[k] = stand[k] + (k == 0 ? fxFromInt(60) : 0);
            me.grenadeMs = 0;
            check(!hldata::detail::gruntGrenade(g.logic, g.player, index, true) && me.grenadeMs > 0,
                  "no grenade onto one of his own squad (and he does not think of it again for a second)");
            // (Out of the way, and of the fight.)
            g.actors.states[mate].active = false;
            actorPlaceBox(g.actors, mate);
        }
        check(mate >= 0, "one of his squad to stand by the player");
        // Too close: no grenade.
        check(standBefore(g, index, 200), "somewhere nearer to stand");
        me.grenadeMs = 0;
        check(!hldata::detail::gruntGrenade(g.logic, g.player, index, true), "no grenade from within 256 units");
        for (int k = 0; k < 3; ++k)
            g.player.origin[k] = me.seen[k] = stand[k];
        me.grenadeMs = 0;
        bool can = false;
        for (int n = 0; n < 8 && !can; ++n)
        {
            me.grenadeMs = 0;
            can = hldata::detail::gruntGrenade(g.logic, g.player, index, true);
        }
        check(can, "from 500 units in the open there is a throw to be had");
        if (!can)
            return;
        // He throws it; it comes down by them, and goes off on its fuse.
        hldata::detail::gruntAct(g.actors, me, kActToss, "throwgrenade");
        g.player.health = 1000;
        int thrownAt = -1, offAt = -1, nearest = 100000;
        const fx feet[3] = {stand[0], stand[1], stand[2] - kStandingFeet};
        for (int ms = 0; ms < 8000 && offAt < 0; ms += 33)
        {
            tickStill(g);
            // (Held to this: what he does next is not the point.)
            if (thrownAt >= 0 && me.ai != kGruntWait)
                hldata::detail::gruntWait(g.actors, me, 10000);
            bool flying = false;
            for (const Projectile &shot : g.actors.shots)
            {
                if (!shot.active || shot.kind != kShotGrenade)
                    continue;
                flying = true;
                const fx d[3] = {shot.pos[0] - feet[0], shot.pos[1] - feet[1], shot.pos[2] - feet[2]};
                const int far = fxToInt(fxLength(d));
                nearest = far < nearest ? far : nearest;
            }
            if (flying && thrownAt < 0)
                thrownAt = ms;
            if (!flying && thrownAt >= 0)
                offAt = ms;
        }
        std::printf("test_soldiers: grenade: thrown at %d ms, comes within %d units of their feet, off %d ms later, "
                    "%d taken\n",
                    thrownAt, nearest, offAt - thrownAt, 1000 - g.player.health);
        check(thrownAt >= 1100 && thrownAt <= 1300, "the grenade leaves his hand on the throw's own frame");
        check(nearest < 80, "... and comes down by them");
        check(offAt - thrownAt >= 3400 && offAt - thrownAt <= 3600 && g.player.health < 1000,
              "... to go off three and a half seconds after");
        check(me.grenadeMs > 2000 && !hldata::detail::gruntGrenade(g.logic, g.player, index, true),
              "and he has not another for them inside six seconds");
    }

    /// A grenade by him: he says so and runs from it (or, with nowhere to
    /// run, gets down). Soldiers with nothing to do talk. And the dead ones
    /// the maps lay out are laid out.
    void testOddments(const char *dir)
    {
        {
            auto game = load(dir, "c1a3d");
            if (!game)
                return;
            Game &g = *game;
            const int index = findKind(g, ActorKind::Grunt);
            if (index < 0)
                return;
            only(g, index);
            ActorState &me = g.actors.states[index];
            g.player.origin[2] += fxFromInt(30000);
            tickStill(g);
            hldata::detail::gruntAlert(g.logic, index, me.pos);
            const fx still[3] = {0, 0, 0};
            const fx by[3] = {me.pos[0] + fxFromInt(40), me.pos[1], me.pos[2] + fxFromInt(4)};
            g.player.health = 100;
            projectileAdd(g.actors, kShotGrenade, by, still, 3000);
            const int spoken = g.sentencesHeard;
            bool ran = false, cowered = false;
            for (int ms = 0; ms < 1500 && !ran && !cowered; ms += 33)
            {
                tickStill(g);
                ran = me.ai == kAiChase && me.attack == kGoFlee;
                cowered = me.ai == kAiAttack && me.attack == kActCower;
            }
            std::printf("test_soldiers: a grenade at his feet: he %s\n", ran ? "runs" : (cowered ? "cowers" : "stays"));
            check(ran || cowered, "a grenade at his feet: he runs from it, or gets down");
            check(g.sentencesHeard == spoken + 1, "... and says so");
        }
        {
            auto game = load(dir, "c1a3a");
            if (!game)
                return;
            Game &g = *game;
            g.player.origin[2] += fxFromInt(30000);
            for (int ms = 0; ms < 60000; ms += 33)
                tickStill(g);
            bool idle = true;
            for (int i = 0; i < g.actors.count; ++i)
                idle = idle && (!isKind(g.actors.states[i], ActorKind::Grunt) || g.actors.states[i].ai == kAiIdle);
            std::printf("test_soldiers: c1a3a, a minute with nobody about: %d lines of talk\n", g.sentencesHeard);
            check(idle && g.sentencesHeard >= 2 && g.sentencesHeard <= 40,
                  "soldiers with nothing to do pass the time of day, one at a time");
        }
        {
            auto game = load(dir, "c1a3");
            if (!game)
                return;
            Game &g = *game;
            int laid = 0;
            for (int i = 0; i < g.actors.count; ++i)
            {
                const ActorState &actor = g.actors.states[i];
                if (isKind(actor, ActorKind::Pose) && actor.model < g.view.modelRefCount &&
                    std::string(g.view.modelRefs[actor.model].name) == "hgrunt" && actor.clip >= 0 && actor.frozen)
                    ++laid;
            }
            check(laid == 1, "c1a3's dead soldier lies where the map has him");
        }
    }

    /// The launcher, on the map that first has one: a grenade at where the
    /// player stands, off at the first thing it meets.
    void testLauncher(const char *dir)
    {
        auto game = load(dir, "c2a2b1");
        if (!game)
            return;
        Game &g = *game;
        g.player.origin[2] += fxFromInt(30000);
        tickStill(g);
        int index = -1;
        for (int i = 0; i < g.actors.count && index < 0; ++i)
        {
            const ActorState &actor = g.actors.states[i];
            if (isKind(actor, ActorKind::Grunt) && (actor.item & kGruntLauncher) && standBefore(g, i, 600))
                index = i;
        }
        check(index >= 0, "c2a2b1 has a soldier with a launcher, and somewhere to stand in front of him");
        if (index < 0)
            return;
        only(g, index);
        ActorState &me = g.actors.states[index];
        me.squad = -1;
        check(((g.view.actors[index].item >> kActorSkinShift) & 0xFF) == 1 && g.view.actors[index].body % 4 == 3,
              "he is the one in the mask");
        g.player.health = 100;
        bool launched = false, flew = false;
        int ms = 0;
        for (; ms < 6000 && g.player.health == 100; ms += 33)
        {
            tickStill(g);
            launched = launched || (me.ai == kAiAttack && me.attack == kActLaunch);
            for (const Projectile &shot : g.actors.shots)
                flew = flew || (shot.active && shot.kind == kShotContact && shot.sprite == 1);
        }
        std::printf("test_soldiers: c2a2b1: launcher: %s, %d taken after %d ms\n", launched ? "launched" : "NOT launched",
                    100 - g.player.health, ms);
        check(launched && flew && g.times(CommonSound::Launcher) == 1 && g.times(CommonSound::Explode) >= 1 &&
                  g.player.health < 100,
              "he launches a grenade at them before anything else, and it goes off where they stand");
        check(me.grenadeMs > 0, "... and not another for six seconds");
    }

    /// The shotgun: eight shells, one a "burst", five pellets of five.
    void testShotgun(const char *dir)
    {
        auto game = load(dir, "c2a2b2");
        if (!game)
            return;
        Game &g = *game;
        g.player.origin[2] += fxFromInt(30000);
        tickStill(g);
        int index = -1;
        for (int i = 0; i < g.actors.count && index < 0; ++i)
        {
            const ActorState &actor = g.actors.states[i];
            if (isKind(actor, ActorKind::Grunt) && (actor.item & kGruntShotgun) && standBefore(g, i, 200))
                index = i;
        }
        check(index >= 0, "c2a2b2 has a soldier with a shotgun, and somewhere to stand in front of him");
        if (index < 0)
            return;
        only(g, index);
        ActorState &me = g.actors.states[index];
        me.item &= static_cast<std::uint8_t>(~(kGruntGrenades | kGruntLauncher));
        check(me.ammo == kGruntShells && me.body % 4 == 2 && me.body / 4 == 1, "eight shells, and the gun to be seen");
        g.player.health = 100000;
        int health = g.player.health, shells = 0, last = me.ammo, most = 0, odd = 0;
        for (int ms = 0; ms < 8000 && me.ammo > 0; ms += 33)
        {
            tickStill(g);
            if (me.ai == kAiChase)
                hldata::detail::gruntWait(g.actors, me, 200);
            shells += me.ammo < last ? last - me.ammo : 0;
            last = me.ammo;
            const int took = health - g.player.health;
            most = took > most ? took : most;
            odd += took % kGruntPellet != 0;
            health = g.player.health;
        }
        std::printf("test_soldiers: c2a2b2: shotgun: %d shells, at most %d a shell at 200 units\n", shells, most);
        check(shells == kGruntShells && g.times(CommonSound::Shotgun) == kGruntShells, "he fires them one at a time");
        check(most > 0 && most <= kGruntPellets * kGruntPellet && odd == 0, "... five pellets of five a shell");
    }

    /// What a monstermaker makes: soldiers, with what the game gives one
    /// that is not told what to carry.
    void testMade(const char *dir)
    {
        auto game = load(dir, "c2a1a");
        if (!game)
            return;
        Game &g = *game;
        int places = 0;
        for (std::uint32_t i = 0; i < g.view.actorCount; ++i)
            places += g.view.actors[i].kind == static_cast<std::uint8_t>(ActorKind::Grunt) &&
                      (g.view.actors[i].flags & kActorDormant);
        check(places >= 5, "c2a1a keeps places for the soldiers its makers make");
        g.player.origin[2] += fxFromInt(30000);
        int maker = -1;
        for (int n = 0; maker < 0; ++n)
        {
            const int record = g.recordOfKind(LogicKind::Maker, n);
            if (record < 0)
                break;
            const std::uint32_t first = g.view.logic[record].extra[0];
            if (g.view.logic[record].extra[1] != 0 && first < g.view.actorCount &&
                g.view.actors[first].kind == static_cast<std::uint8_t>(ActorKind::Grunt))
                maker = record;
        }
        check(maker >= 0, "a maker of soldiers");
        if (maker < 0)
            return;
        const int slot = static_cast<int>(g.view.logic[maker].extra[0]);
        logicUse(g.logic, g.player, maker, -1, kUseToggle);
        for (int ms = 0; ms < 3000; ms += 33)
            tickStill(g);
        const ActorState &made = g.actors.states[slot];
        check(made.active && made.health == kGruntHealth && made.item == (kGruntMp5 | kGruntGrenades) &&
                  made.ammo == kGruntClip,
              "used, it makes one: an MP5 and grenades, as the game gives");
    }
} // namespace

int main()
{
    const char *dir = std::getenv("HLDS_VALVE_DIR");
    if (dir != nullptr && dir[0] != '\0')
    {
        testMines(dir);
        testCooked(dir);
        testSoldier(dir);
        testSquads(dir);
        testSquads(dir, true);
        testGrenades(dir);
        testOddments(dir);
        testLauncher(dir);
        testShotgun(dir);
        testMade(dir);
    }
    else
    {
        std::printf("test_soldiers: HLDS_VALVE_DIR not set, the maps were not played\n");
    }
    if (g_failures == 0)
        std::printf("test_soldiers: ok\n");
    return g_failures == 0 ? 0 : 1;
}

// A game between consoles (shared/HlNet.hpp, HlNetGame.hpp), played here by
// several games in one process joined by a link that loses frames: what is
// said once arrives once, in order, whatever is lost on the way.

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <memory>
#include <vector>

#include "HlNet.hpp"
#include "HlNetGame.hpp"
#include "TestGame.hpp"

using namespace hldata;
using namespace hldstest;

namespace
{
    std::uint32_t g_seed = 2024;
    std::uint32_t rnd()
    {
        g_seed = g_seed * 1664525u + 1013904223u;
        return g_seed >> 8;
    }

    /// The air: a frame put in comes out some frames later, or not at all.
    struct Air
    {
        int lossPercent = 20;
        int mostDelay = 5;
        struct Frame
        {
            int due;
            std::vector<std::uint8_t> bytes;
        };
        std::deque<Frame> flying;
        int now = 0;
        void send(const std::uint8_t *bytes, int size)
        {
            if (size <= 0 || static_cast<int>(rnd() % 100) < lossPercent)
                return;
            flying.push_back({now + 1 + static_cast<int>(rnd() % (mostDelay + 1)),
                              std::vector<std::uint8_t>(bytes, bytes + size)});
        }
        /// The frames that land this tick, oldest first (a later one may
        /// overtake an earlier: the radio does not do that, but nothing
        /// here may depend on it).
        std::vector<std::vector<std::uint8_t>> land()
        {
            std::vector<std::vector<std::uint8_t>> out;
            for (auto it = flying.begin(); it != flying.end();)
            {
                if (it->due <= now)
                {
                    out.push_back(std::move(it->bytes));
                    it = flying.erase(it);
                }
                else
                    ++it;
            }
            return out;
        }
    };

    NetEvent numbered(NetEventType type, int from, int n)
    {
        NetEvent e;
        e.type = type;
        e.from = static_cast<std::uint8_t>(from);
        e.v[0] = static_cast<std::int16_t>(n);
        e.v[5] = static_cast<std::int16_t>(n * 7);
        return e;
    }

    void testPacking()
    {
        NetPlayer p;
        p.pos[0] = -12345;
        p.pos[1] = 321;
        p.pos[2] = 16000;
        p.yaw = 54321;
        p.pitch = -100;
        p.velocity[0] = -128;
        p.velocity[2] = 127;
        p.weapon = 5;
        p.flags = kNetHere | kNetAlive | kNetFiring;
        p.health = 87;
        p.spawns = 200;
        std::uint8_t bytes[64];
        hldata::detail::NetOut out{bytes, bytes + sizeof(bytes)};
        hldata::detail::put(out, p);
        check(out.ok && out.at - bytes == kNetPlayerBytes, "a player's state is sixteen bytes");
        NetPlayer q;
        hldata::detail::NetIn in{bytes, out.at};
        hldata::detail::get(in, q);
        check(in.ok && q.pos[0] == p.pos[0] && q.pos[2] == p.pos[2] && q.yaw == p.yaw && q.pitch == p.pitch &&
                  q.velocity[0] == p.velocity[0] && q.velocity[2] == p.velocity[2] && q.weapon == p.weapon &&
                  q.flags == p.flags && q.health == p.health && q.spawns == p.spawns,
              "... and reads back as it was written");

        NetEvent e;
        e.type = NetEventType::Hit;
        e.from = 2;
        e.a = 1;
        e.v[0] = 40;
        e.v[5] = -32768;
        hldata::detail::NetOut eo{bytes, bytes + sizeof(bytes)};
        hldata::detail::put(eo, e);
        check(eo.ok && eo.at - bytes == kNetEventBytes, "an event is fifteen bytes");
        NetEvent f;
        hldata::detail::NetIn ei{bytes, eo.at};
        hldata::detail::get(ei, f);
        check(f.type == e.type && f.from == 2 && f.a == 1 && f.v[0] == 40 && f.v[5] == -32768, "... and reads back");
        NetEvent hello;
        hello.type = NetEventType::Hello;
        std::strncpy(hello.text, "A name far too long for it", kNetTextBytes - 1);
        hldata::detail::NetOut ho{bytes, bytes + sizeof(bytes)};
        hldata::detail::put(ho, hello);
        hldata::detail::NetIn hi{bytes, ho.at};
        hldata::detail::get(hi, f);
        check(ho.at - bytes == kNetEventBytes && std::strcmp(f.text, "A name far ") == 0,
              "a name is eleven letters of it, and ends");

        // A frame cut short, or of something else, is not read.
        NetHost host;
        netHostJoin(host, 1);
        std::uint8_t frame[kNetCmdBytes];
        const int size = netHostWrite(host, p, frame);
        NetClient client;
        client.token = 77;
        host.clients[1].token = 77;
        const int resized = netHostWrite(host, p, frame);
        check(resized == size, "(the host's frame is as long whoever is in it)");
        check(size > 0 && size <= kNetCmdBytes && netClientRead(client, frame, size), "a client reads the host's frame");
        check(!netClientRead(client, frame, 20), "... not one cut short");
        frame[0] ^= 0xFF;
        check(!netClientRead(client, frame, size), "... nor one of another game");
        frame[0] ^= 0xFF;
        frame[2] = kNetVersion + 1;
        check(!netClientRead(client, frame, size), "... nor of another version of this one");
    }

    /// A host and three clients say three hundred things each through bad
    /// air; a fourth of the way in, one client leaves and another comes.
    void testEvents()
    {
        auto host = std::make_unique<NetHost>();
        std::unique_ptr<NetClient> clients[kNetPlayers];
        Air down[kNetPlayers], up[kNetPlayers];
        int hostSaid = 0, clientSaid[kNetPlayers] = {};
        int hostHeardFrom[kNetPlayers] = {};   // the next number the host expects of each
        int clientHeard[kNetPlayers] = {};     // ... and each client of the host
        int clientFirst[kNetPlayers] = {};     // the host's number each began at
        bool wrong = false;
        for (int c = 1; c < kNetPlayers; ++c)
        {
            clients[c] = std::make_unique<NetClient>();
            clients[c]->token = static_cast<std::uint16_t>(100 + c);
            netHostJoin(*host, c);
        }
        const int kSay = 300;
        int quiet = 0;
        for (int tick = 0; tick < 20000 && quiet < 200; ++tick)
        {
            // Client 2 goes at tick 150; someone new takes its place at 400.
            if (tick == 150)
            {
                netHostLeave(*host, 2);
                clients[2].reset();
            }
            if (tick == 400)
            {
                clients[2] = std::make_unique<NetClient>();
                clients[2]->token = 999;
                netHostJoin(*host, 2);
                clientSaid[2] = 0;
                hostHeardFrom[2] = 0;
                clientFirst[2] = clientHeard[2] = hostSaid;
                up[2].flying.clear();
                down[2].flying.clear();
            }
            // Everyone says something now and then.
            if (hostSaid < kSay && rnd() % 3 == 0 && host->out.count < NetOutbox::kMost)
                host->out.add(numbered(NetEventType::Score, 0, hostSaid++));
            for (int c = 1; c < kNetPlayers; ++c)
                if (clients[c] && clientSaid[c] < kSay && rnd() % 3 == 0 && clients[c]->out.count < NetOutbox::kMost)
                    clients[c]->out.add(numbered(NetEventType::Shot, c, clientSaid[c]++));

            std::uint8_t frame[kNetCmdBytes];
            NetPlayer mine;
            mine.pos[0] = static_cast<std::int16_t>(tick);
            const int size = netHostWrite(*host, mine, frame);
            wrong = wrong || size <= 0 || size > kNetCmdBytes;
            for (int c = 1; c < kNetPlayers; ++c)
            {
                down[c].now = up[c].now = tick;
                if (!clients[c])
                    continue;
                down[c].send(frame, size);
                for (const auto &bytes : down[c].land())
                    netClientRead(*clients[c], bytes.data(), static_cast<int>(bytes.size()));
                NetEvent e;
                while (clients[c]->in.take(e))
                {
                    wrong = wrong || e.type != NetEventType::Score || e.v[0] != clientHeard[c] ||
                            e.v[5] != static_cast<std::int16_t>(clientHeard[c] * 7);
                    ++clientHeard[c];
                }
                std::uint8_t reply[kNetReplyBytes];
                NetPlayer theirs;
                theirs.pos[1] = static_cast<std::int16_t>(c * 100 + tick % 50);
                const int length = netClientWrite(*clients[c], theirs, reply);
                wrong = wrong || length <= 0 || length > kNetReplyBytes;
                up[c].send(reply, length);
                for (const auto &bytes : up[c].land())
                    netHostRead(*host, c, bytes.data(), static_cast<int>(bytes.size()));
                while (host->clients[c].in.take(e))
                {
                    wrong = wrong || e.type != NetEventType::Shot || e.from != c || e.v[0] != hostHeardFrom[c];
                    ++hostHeardFrom[c];
                }
            }
            bool all = hostSaid == kSay && tick > 400;
            for (int c = 1; c < kNetPlayers; ++c)
                all = all && clients[c] && clientSaid[c] == kSay && hostHeardFrom[c] == kSay && clientHeard[c] == kSay;
            quiet = all ? quiet + 1 : 0;
            if (all && host->out.count == 0)
                break;
        }
        check(!wrong, "every event arrives in its order, once, through air that loses a fifth of the frames");
        bool whole = hostSaid == kSay;
        for (int c = 1; c < kNetPlayers; ++c)
            whole = whole && hostHeardFrom[c] == kSay && clientHeard[c] == kSay;
        check(whole, "... all of them: three hundred from each to the host and from the host to each");
        check(clientFirst[2] > 0 && clientFirst[2] < kSay,
              "(the one who came late began at what the host was saying then, not at the beginning)");
        check(host->out.count == 0 && host->out.lost == 0, "... and the host keeps nothing that everyone has");
        check(host->players[1].pos[1] / 100 == 1 && host->players[3].pos[1] / 100 == 3 &&
                  (host->players[1].flags & kNetHere),
              "the host knows where each client is");
        check(clients[1]->players[0].pos[0] > 0 && clients[1]->players[3].pos[1] / 100 == 3,
              "... and each client where the host and the other clients are");
    }

    /// A file of 300 KB sent to a client through air of a kind.
    int sendFile(const std::vector<std::uint8_t> &file, int loss, int delay, std::vector<std::uint8_t> &got,
                 FileReceiver &receiver, FileSender &sender)
    {
        Air down, up;
        down.lossPercent = up.lossPercent = loss;
        down.mostDelay = up.mostDelay = delay;
        receiver.open("map:/lvl/crossfire.lvl");
        got.clear();
        int ticks = 0;
        std::uint8_t frame[kNetCmdBytes];
        for (; ticks < 100000 && !receiver.done(); ++ticks)
        {
            down.now = up.now = ticks;
            const int ask = receiver.ask(frame);
            if (ask > 0)
                up.send(frame, ask);
            for (const auto &bytes : up.land())
                sender.asked(2, bytes.data(), static_cast<int>(bytes.size()));
            // The host sends two pieces a frame while someone lacks any.
            std::uint32_t index;
            for (int n = 0; n < 2 && sender.next(static_cast<std::uint32_t>(file.size()), index); ++n)
            {
                const std::size_t at = static_cast<std::size_t>(index) * kNetFilePiece;
                const std::size_t left = file.size() - at;
                const int count = left < kNetFilePiece ? static_cast<int>(left) : kNetFilePiece;
                down.send(frame, sender.piece(static_cast<std::uint32_t>(file.size()), index, file.data() + at,
                                              count, frame));
            }
            for (const auto &bytes : down.land())
            {
                // (The first frame says how long it is: room is made then.)
                if (receiver.known && got.size() != receiver.total)
                    got.resize(receiver.total);
                receiver.piece(2, bytes.data(), static_cast<int>(bytes.size()), got.empty() ? nullptr : got.data(),
                               static_cast<std::uint32_t>(got.size()));
            }
        }
        return ticks;
    }

    void testFile()
    {
        std::vector<std::uint8_t> file(300 * 1024 + 123);
        for (std::size_t i = 0; i < file.size(); ++i)
            file[i] = static_cast<std::uint8_t>(rnd());
        std::vector<std::uint8_t> got;
        FileReceiver receiver;
        FileSender sender;
        const int bad = sendFile(file, 20, 5, got, receiver, sender);
        check(receiver.done() && !receiver.missing && got == file,
              "a file of 300 KB arrives whole, byte for byte, through air that loses a fifth of the frames");
        check(std::strcmp(sender.name, "map:/lvl/crossfire.lvl") == 0 && sender.to == 2,
              "... the host knowing which file, and for whom");
        const int good = sendFile(file, 2, 1, got, receiver, sender);
        check(receiver.done() && got == file, "... and through good air");
        const int perfect = static_cast<int>(file.size()) / (2 * kNetFilePiece);
        check(good < perfect * 11 / 10 && bad < perfect * 3 / 2,
              "losing a frame costs that frame and little more: nothing is sent twice that came");
        std::printf("  (300 KB at two pieces a frame: %d frames with nothing lost, %d through good air, %d through bad)\n",
                    perfect, good, bad);

        // A file there is not.
        std::uint8_t frame[kNetCmdBytes];
        receiver.open("map:/none");
        const int ask = receiver.ask(frame);
        check(ask > 0 && ask <= kNetReplyBytes && sender.asked(2, frame, ask) && sender.fresh,
              "a second file is a new asking");
        receiver.piece(2, frame, sender.piece(0xFFFFFFFFu, 0, nullptr, 0, frame), nullptr, 0);
        check(receiver.done() && receiver.missing, "the host says when there is no such file");
        // A piece meant for someone else, or of an earlier asking.
        std::uint8_t small[16] = {};
        receiver.open("map:/a");
        sender.asked(2, frame, receiver.ask(frame));
        const int other = sender.piece(10, 0, file.data(), 10, frame);
        check(!receiver.piece(3, frame, other, small, sizeof(small)), "a piece for another client is not taken");
        receiver.open("map:/b");
        check(!receiver.piece(2, frame, other, small, sizeof(small)), "... nor one of the file asked for before");
        // An empty file is a file.
        receiver.open("map:/empty");
        sender.asked(2, frame, receiver.ask(frame));
        receiver.piece(2, frame, sender.piece(0, 0, nullptr, 0, frame), nullptr, 0);
        check(receiver.done() && !receiver.missing && receiver.total == 0, "a file of no bytes is there, and done");
    }

    void testGlide()
    {
        NetGlide glide;
        NetPlayer heard;
        heard.pos[0] = 400; // 100 units
        netGlide(glide, heard, 17);
        check(glide.pos[0] == 100 * 4096, "someone first heard of is put where they are");
        heard.pos[0] = 440; // ten units on
        for (int n = 0; n < 30; ++n)
            netGlide(glide, heard, 17);
        check(glide.pos[0] > 109 * 4096 && glide.pos[0] <= 110 * 4096, "... and then glides to where they are heard to be");
        heard.pos[0] = 4000;
        netGlide(glide, heard, 17);
        check(glide.pos[0] == 1000 * 4096, "... but is put there if it is far");
        heard.pos[0] = 4020;
        ++heard.spawns;
        netGlide(glide, heard, 17);
        check(glide.pos[0] == 1005 * 4096, "... or if they have been put somewhere, however near");
    }

    // ------------------------------------------------------------ a match

    /// One console: a whole game of its own, and its account of the match.
    struct Console
    {
        std::unique_ptr<Game> game;
        std::unique_ptr<NetMatch> match = std::make_unique<NetMatch>();
        std::vector<NetSpawn> spawns;
        std::uint32_t did = 0; ///< what its player's weapon did this tick, as weaponsTick would say
        int model = -1;
        bool gone = false;

        Game &g() { return *game; }
        PlayerState &player() { return game->player; }
        ActorState &other(int p) { return game->actorStates[match->firstActor + p]; }
    };

    std::unique_ptr<Console> console(const char *dir, const char *map, NetMatch::Role role, int me, const char *name)
    {
        auto c = std::make_unique<Console>();
        c->game = std::make_unique<Game>();
        std::string error;
        if (!c->game->bsp.load(std::string(dir) + "/maps/" + map + ".bsp", error))
            return nullptr;
        finish(*c->game, nullptr, dir);
        const LevelView &v = c->game->view;
        for (std::uint32_t e = 0; e < v.entityCount; ++e)
        {
            if (v.entities[e].classId != static_cast<std::uint16_t>(EntityClass::InfoPlayerDeathmatch))
                continue;
            NetSpawn spawn;
            for (int i = 0; i < 3; ++i)
                spawn.pos[i] = v.entities[e].origin[i];
            spawn.yaw = v.entities[e].angles[1];
            c->spawns.push_back(spawn);
        }
        int model = -1;
        for (std::uint32_t m = 0; m < v.modelRefCount; ++m)
            if (std::strcmp(v.modelRefs[m].name, "player") == 0)
                model = static_cast<int>(m);
        netBegin(*c->match, role, static_cast<std::uint16_t>(500 + me), name);
        c->model = model;
        if (role == NetMatch::Role::Host)
            netStart(*c->match, c->game->logic, c->game->player, static_cast<int>(v.actorCount), model, c->spawns.data(),
                 static_cast<int>(c->spawns.size()), 77u * static_cast<unsigned>(me + 1));
        return c;
    }

    /// Three consoles and the air between them.
    struct Room
    {
        static constexpr int kCount = 3;
        std::unique_ptr<Console> c[kCount];
        Air down[kCount], up[kCount];
        int now = 0;

        void tick(int ms = 33)
        {
            ++now;
            for (int n = 0; n < kCount; ++n)
            {
                if (c[n]->gone)
                    continue;
                Game &g = c[n]->g();
                g.player.onGround = true; // (nobody is walking: they stand where they are put)
                logicTick(g.logic, g.player, ms);
                monstersTick(g.logic, g.player, ms);
                // (A client's match begins when the host has said which player it is.)
                if (!c[n]->match->playing && netPlaced(*c[n]->match))
                    netStart(*c[n]->match, g.logic, g.player, static_cast<int>(g.view.actorCount), c[n]->model,
                             c[n]->spawns.data(), static_cast<int>(c[n]->spawns.size()), 77u * static_cast<unsigned>(n + 1));
                netTick(*c[n]->match, g.logic, g.player, static_cast<std::uint16_t>(n * 1000), 0, c[n]->did, false, ms);
                c[n]->did = 0;
                g.heard.count = 0;
                g.player.hurt = 0;
            }
            std::uint8_t frame[kNetCmdBytes];
            const int size = netWrite(*c[0]->match, frame);
            for (int n = 1; n < kCount; ++n)
            {
                down[n].now = up[n].now = now;
                if (c[n]->gone)
                    continue;
                down[n].send(frame, size);
                for (const auto &bytes : down[n].land())
                    netRead(*c[n]->match, bytes.data(), static_cast<int>(bytes.size()));
                std::uint8_t reply[kNetReplyBytes];
                up[n].send(reply, netWrite(*c[n]->match, reply));
                for (const auto &bytes : up[n].land())
                    netRead(*c[0]->match, bytes.data(), static_cast<int>(bytes.size()), n);
            }
        }
        void run(int ticks)
        {
            for (int n = 0; n < ticks; ++n)
                tick();
        }
    };

    /// The lobby: three consoles with no map loaded.
    void testLobby()
    {
        std::unique_ptr<NetMatch> m[3];
        Air down[3], up[3];
        static const char *const kNames[] = {"Host", "Anna", "Bob"};
        static LogicWorld none;
        static PlayerState nobody;
        for (int n = 0; n < 3; ++n)
        {
            m[n] = std::make_unique<NetMatch>();
            netBegin(*m[n], n == 0 ? NetMatch::Role::Host : NetMatch::Role::Client, static_cast<std::uint16_t>(40 + n),
                     kNames[n]);
            if (n > 0)
                netHostJoin(m[0]->host, n);
        }
        auto run = [&](int ticks)
        {
            for (int t = 0; t < ticks; ++t)
            {
                std::uint8_t frame[kNetCmdBytes];
                for (int n = 0; n < 3; ++n)
                    netTick(*m[n], none, nobody, 0, 0, 0, false, 17);
                const int size = netWrite(*m[0], frame);
                for (int n = 1; n < 3; ++n)
                {
                    down[n].now = up[n].now = down[n].now + 1;
                    down[n].send(frame, size);
                    for (const auto &bytes : down[n].land())
                        netRead(*m[n], bytes.data(), static_cast<int>(bytes.size()));
                    std::uint8_t reply[kNetReplyBytes];
                    up[n].send(reply, netWrite(*m[n], reply));
                    for (const auto &bytes : up[n].land())
                        netRead(*m[0], bytes.data(), static_cast<int>(bytes.size()), n);
                }
            }
        };
        run(60);
        bool named = true;
        for (int n = 0; n < 3; ++n)
            for (int p = 0; p < 3; ++p)
                named = named && std::strcmp(m[n]->names[p], kNames[p]) == 0 && netHere(*m[n], p);
        check(named && m[1]->me == 1 && m[2]->me == 2,
              "in the lobby, with no map loaded, everyone learns who is there and what they are called");
        check(!netHere(*m[1], 3), "... and that the fourth place is empty");
        netSay(*m[0], netEvent(NetEventType::Start, 0, 0x1234, 0x5678, 20, 10));
        run(30);
        bool started = true;
        for (int n = 1; n < 3; ++n)
            started = started && m[n]->passedCount == 1 && m[n]->passed[0].type == NetEventType::Start &&
                      m[n]->passed[0].v[0] == 0x1234 && m[n]->passed[0].v[2] == 20;
        check(started, "the host's word to start reaches each of them, once, with which map and what limits");
    }

    int far(const fx *a, const fx *b)
    {
        fx d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
        return fxToInt(fxLength(d));
    }

    void testMatch(const char *dir)
    {
        Room room;
        static const char *const kNames[] = {"Host", "Anna", "Bob"};
        for (int n = 0; n < Room::kCount; ++n)
        {
            room.c[n] = console(dir, "crossfire", n == 0 ? NetMatch::Role::Host : NetMatch::Role::Client, n, kNames[n]);
            if (!room.c[n])
            {
                check(false, "crossfire loads");
                return;
            }
            if (n > 0)
                netHostJoin(room.c[0]->match->host, n);
        }
        check(room.c[0]->spawns.size() >= 4 && room.c[0]->model >= 0,
              "crossfire has its places to come in at, and the players' model");
        room.run(90);
        check(room.c[1]->match->me == 1 && room.c[2]->match->me == 2 && room.c[1]->match->playing,
              "each client is told by the host which player it is");

        // ---- everyone is seen by everyone, where they are
        bool seen = true, named = true, apart = true;
        for (int n = 0; n < Room::kCount; ++n)
        {
            for (int p = 0; p < Room::kCount; ++p)
            {
                if (p == n)
                    continue;
                const ActorState &actor = room.c[n]->other(p);
                fx feet[3] = {room.c[p]->player().origin[0], room.c[p]->player().origin[1],
                              room.c[p]->player().origin[2] - fxFromInt(36)};
                seen = seen && actor.active && actor.kind == static_cast<std::uint8_t>(ActorKind::Player) &&
                       far(actor.pos, feet) <= 2 && (actor.flags & kActorSolid) && actor.clip >= 0 &&
                       room.c[n]->g().actorBoxes[room.c[n]->match->firstActor + p].active;
                named = named && std::strcmp(room.c[n]->match->names[p], kNames[p]) == 0;
                apart = apart && far(room.c[n]->player().origin, room.c[p]->player().origin) > 100;
            }
        }
        check(seen, "each console sees the other two where they are, solid, in a clip of the players' model");
        check(named, "... and knows what they are called");
        check(apart, "they came in at different places");
        bool empty = true;
        for (int i = 0; i < room.c[1]->g().actors.count; ++i)
        {
            const ActorState &actor = room.c[1]->g().actorStates[i];
            empty = empty && (!actor.active || actor.kind == static_cast<std::uint8_t>(ActorKind::Item) ||
                              actor.kind == static_cast<std::uint8_t>(ActorKind::Furniture) ||
                              actor.kind == static_cast<std::uint8_t>(ActorKind::Player));
        }
        check(empty, "nobody of the map's own is in the match");

        // ---- a hit: claimed by the shooter, taken by the victim, once
        Console &anna = *room.c[1], &bob = *room.c[2], &host = *room.c[0];
        PlayerState expected = bob.player();
        playerHurt(expected, 30);
        monsterHurt(anna.g().logic, anna.match->firstActor + 2, 30, true, kHitBody);
        room.run(40);
        check(bob.player().health == expected.health && bob.player().health < 100,
              "Anna's shot on Bob, as her console sees him, hurts Bob on his own: once, and as much");
        check(host.player().health == 100 && anna.player().health == 100, "... and nobody else");
        // A blast near him, from her grenade.
        const int before = bob.player().health;
        fx blast[3] = {bob.player().origin[0] + fxFromInt(40), bob.player().origin[1], bob.player().origin[2]};
        radiusDamage(anna.g().logic, anna.player(), blast, 60, 200, -1);
        room.run(40);
        check(bob.player().health < before && bob.player().health > 0, "a blast of hers near him hurts him too");
        // The head: three times a bullet.
        expected = bob.player();
        playerHurt(expected, 10 * 3);
        monsterHurt(anna.g().logic, anna.match->firstActor + 2, 10, true, kHitHead);
        room.run(40);
        check(bob.player().health == expected.health, "a bullet in the head is three");

        // ---- a death: scored by the host, known to all; and back after a while
        const std::uint8_t spawnsBefore = bob.match->spawns;
        monsterHurt(anna.g().logic, anna.match->firstActor + 2, 500, true, kHitBody);
        room.run(45);
        check(bob.player().health <= 0, "enough of it kills him");
        bool scored = true;
        for (int n = 0; n < Room::kCount; ++n)
            scored = scored && room.c[n]->match->frags[1] == 1 && room.c[n]->match->deaths[2] == 1 &&
                     room.c[n]->match->frags[2] == 0 && room.c[n]->match->frags[0] == 0;
        check(scored, "... a frag for Anna and a death for Bob, on every console");
        check(!(anna.other(2).flags & kActorSolid) && anna.match->pose[2] == kPoseDead,
              "... and he lies there, in nobody's way");
        bool told = false;
        for (int k = 0; k < host.match->noticeCount; ++k)
            told = told || (host.match->notices[k].type == NetEventType::Died && host.match->notices[k].who == 2 &&
                            host.match->notices[k].other == 1);
        check(told, "... and the host's screen can say who killed whom");
        room.run(120);
        check(bob.player().health == 100 && bob.match->spawns == static_cast<std::uint8_t>(spawnsBefore + 1) &&
                  (bob.player().inv.have & (1u << kWeaponGlock)) && bob.match->respawned,
              "three seconds on he is back, whole, with a pistol");
        check((anna.other(2).flags & kActorSolid) && anna.match->pose[2] != kPoseDead, "... and is seen to be");
        // Falling to his death is nobody's frag.
        room.run(200); // (long past any blame)
        bob.player().health = 0;
        room.run(45);
        bool own = true;
        for (int n = 0; n < Room::kCount; ++n)
            own = own && room.c[n]->match->frags[2] == -1 && room.c[n]->match->deaths[2] == 2 &&
                  room.c[n]->match->frags[1] == 1;
        check(own, "a death by nobody else costs him a frag, and gives nobody one");
        room.run(120);

        // ---- something to pick up, wanted by two at once
        int item = -1;
        for (int i = 0; i < host.g().actors.count && item < 0; ++i)
        {
            const ActorState &actor = host.g().actorStates[i];
            if (actor.active && actor.kind == static_cast<std::uint8_t>(ActorKind::Item) &&
                actor.item == static_cast<std::uint8_t>(ItemKind::Shotgun))
                item = i;
        }
        check(item >= 0, "crossfire has a shotgun lying about");
        if (item >= 0)
        {
            for (Console *who : {&anna, &bob})
            {
                for (int i = 0; i < 3; ++i)
                    who->player().origin[i] = host.g().actorStates[item].pos[i];
                who->player().origin[2] += fxFromInt(36);
            }
            room.run(40);
            const bool hers = (anna.player().inv.have & (1u << kWeaponShotgun)) != 0;
            const bool his = (bob.player().inv.have & (1u << kWeaponShotgun)) != 0;
            check(hers != his, "two reach for one shotgun: one of them has it, and only one");
            bool gone = true;
            for (int n = 0; n < Room::kCount; ++n)
                gone = gone && !room.c[n]->g().actorStates[item].active;
            check(gone, "... and it is gone from where it lay, on every console");
            // (They stand on it still: it is theirs again the moment it is back. Away.)
            anna.player().origin[0] += fxFromInt(200);
            bob.player().origin[0] -= fxFromInt(200);
            room.run(kNetWeaponBackMs / 33 - 60);
            bool still = true;
            for (int n = 0; n < Room::kCount; ++n)
                still = still && !room.c[n]->g().actorStates[item].active;
            check(still, "... for twenty seconds");
            room.run(120);
            bool back = true;
            for (int n = 0; n < Room::kCount; ++n)
                back = back && room.c[n]->g().actorStates[item].active;
            check(back, "... and then it is back, for all of them");
        }

        // ---- the map's machinery: used by one, moves for all
        int door = -1, trigger = -1;
        for (int i = 0; i < host.g().logic.recordCount; ++i)
        {
            const LogicRecord &record = host.g().logic.records[i];
            const auto kind = static_cast<LogicKind>(record.kind);
            if (door < 0 && (kind == LogicKind::Door || kind == LogicKind::Button) && record.brush != kNoBrush &&
                host.g().logic.states[i].move == MoveState::AtBottom && !host.g().logic.states[i].dead)
                door = i;
            if (trigger < 0 && kind == LogicKind::TriggerMultiple && !(record.flags & kLogicNoPlayer) &&
                host.g().logic.states[i].timer < 0 && !host.g().logic.states[i].dead)
                trigger = i;
        }
        check(door >= 0, "crossfire has a door or a button");
        if (door >= 0)
        {
            logicUse(bob.g().logic, bob.player(), door, -1, kUseSet);
            netUsed(*bob.match, door);
            const bool went = bob.g().logic.states[door].move != MoveState::AtBottom;
            room.run(20);
            bool all = went;
            for (int n = 0; n < Room::kCount; ++n)
                all = all && (room.c[n]->g().logic.states[door].move != MoveState::AtBottom ||
                              room.c[n]->g().logic.states[door].timer >= 0);
            check(!went || all, "what Bob uses goes on every console");
        }
        if (trigger >= 0)
        {
            const LogicRecord &record = host.g().logic.records[trigger];
            for (int i = 0; i < 3; ++i)
                anna.player().origin[i] = fxFromInt((record.mins[i] + record.maxs[i]) / 2);
            room.run(12);
            bool all = true;
            for (int n = 0; n < Room::kCount; ++n)
                all = all && (room.c[n]->g().logic.states[trigger].timer >= 0 || room.c[n]->g().logic.states[trigger].dead);
            check(all, "a trigger Anna walks into fires on every console, by where each sees her");
        }

        // ---- someone leaves
        bob.gone = true;
        netHostLeave(host.match->host, 2);
        room.run(60);
        check(!host.other(2).active && !anna.other(2).active && !host.g().actorBoxes[host.match->firstActor + 2].active,
              "Bob leaves: he is gone from the others' maps");
        const int health = anna.player().health;
        monsterHurt(host.g().logic, host.match->firstActor + 1, 20, true, kHitBody);
        room.run(40);
        check(anna.player().health < health, "... and the match goes on between those who stay");
        check(host.match->host.out.lost == 0 && anna.match->client.out.lost == 0, "nothing was ever dropped for want of room");

        // ---- the end
        host.match->fragLimit = 1; // (Anna has one already)
        room.run(40);
        check(host.match->over && anna.match->over, "the frag limit reached, the match is over for everyone");
    }
} // namespace

int main()
{
    testPacking();
    testEvents();
    testFile();
    testGlide();
    testLobby();
    const char *dir = std::getenv("HLDS_VALVE_DIR");
    if (dir == nullptr)
        std::printf("test_net: HLDS_VALVE_DIR not set, no match was played\n");
    else
        testMatch(dir);
    if (g_failures == 0)
        std::printf("test_net: ok\n");
    return g_failures == 0 ? 0 : 1;
}

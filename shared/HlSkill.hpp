#pragma once

// How hard the game is: the three columns of the game's skill.cfg (its
// gSkillData, gamerules.cpp's RefreshSkillData), every number of it, by the
// name it has there. The level is chosen when a game is begun (NEW GAME's
// EASY, MEDIUM, DIFFICULT) and kept with the game's switches (HlLogic.hpp,
// kGlobalSkill): it goes from map to map and into a save, and a save from
// before there was a choice is MEDIUM -- as everything here was until then.
//
// Typed in from valve/skill.cfg; test_skill checks it against the file.
// std-only and inline: the host tests run it.

#include <cstdint>

namespace hldata
{
    inline constexpr int kSkillEasy = 1;
    inline constexpr int kSkillMedium = 2;
    inline constexpr int kSkillHard = 3;

    struct SkillData
    {
        int agruntHealth = 0; ///< sk_agrunt_health
        int agruntDmgPunch = 0; ///< sk_agrunt_dmg_punch
        int apacheHealth = 0; ///< sk_apache_health
        int barneyHealth = 0; ///< sk_barney_health
        int bullsquidHealth = 0; ///< sk_bullsquid_health
        int bullsquidDmgBite = 0; ///< sk_bullsquid_dmg_bite
        int bullsquidDmgWhip = 0; ///< sk_bullsquid_dmg_whip
        int bullsquidDmgSpit = 0; ///< sk_bullsquid_dmg_spit
        int bigmommaHealthFactorTenths = 0; ///< sk_bigmomma_health_factor  (tenths: 1.5 is 15)
        int bigmommaDmgSlash = 0; ///< sk_bigmomma_dmg_slash
        int bigmommaDmgBlast = 0; ///< sk_bigmomma_dmg_blast
        int bigmommaRadiusBlast = 0; ///< sk_bigmomma_radius_blast
        int gargantuaHealth = 0; ///< sk_gargantua_health
        int gargantuaDmgSlash = 0; ///< sk_gargantua_dmg_slash
        int gargantuaDmgFire = 0; ///< sk_gargantua_dmg_fire
        int gargantuaDmgStomp = 0; ///< sk_gargantua_dmg_stomp
        int hassassinHealth = 0; ///< sk_hassassin_health
        int headcrabHealth = 0; ///< sk_headcrab_health
        int headcrabDmgBite = 0; ///< sk_headcrab_dmg_bite
        int hgruntHealth = 0; ///< sk_hgrunt_health
        int hgruntKick = 0; ///< sk_hgrunt_kick
        int hgruntPellets = 0; ///< sk_hgrunt_pellets
        int hgruntGspeed = 0; ///< sk_hgrunt_gspeed
        int houndeyeHealth = 0; ///< sk_houndeye_health
        int houndeyeDmgBlast = 0; ///< sk_houndeye_dmg_blast
        int islaveHealth = 0; ///< sk_islave_health
        int islaveDmgClaw = 0; ///< sk_islave_dmg_claw
        int islaveDmgClawrake = 0; ///< sk_islave_dmg_clawrake
        int islaveDmgZap = 0; ///< sk_islave_dmg_zap
        int ichthyosaurHealth = 0; ///< sk_ichthyosaur_health
        int ichthyosaurShake = 0; ///< sk_ichthyosaur_shake
        int leechHealth = 0; ///< sk_leech_health
        int leechDmgBite = 0; ///< sk_leech_dmg_bite
        int controllerHealth = 0; ///< sk_controller_health
        int controllerDmgzap = 0; ///< sk_controller_dmgzap
        int controllerSpeedball = 0; ///< sk_controller_speedball
        int controllerDmgball = 0; ///< sk_controller_dmgball
        int nihilanthHealth = 0; ///< sk_nihilanth_health
        int nihilanthZap = 0; ///< sk_nihilanth_zap
        int scientistHealth = 0; ///< sk_scientist_health
        int snarkHealth = 0; ///< sk_snark_health
        int snarkDmgBite = 0; ///< sk_snark_dmg_bite
        int snarkDmgPop = 0; ///< sk_snark_dmg_pop
        int zombieHealth = 0; ///< sk_zombie_health
        int zombieDmgOneSlash = 0; ///< sk_zombie_dmg_one_slash
        int zombieDmgBothSlash = 0; ///< sk_zombie_dmg_both_slash
        int turretHealth = 0; ///< sk_turret_health
        int miniturretHealth = 0; ///< sk_miniturret_health
        int sentryHealth = 0; ///< sk_sentry_health
        int plrCrowbar = 0; ///< sk_plr_crowbar
        int plr9mmBullet = 0; ///< sk_plr_9mm_bullet
        int plr357Bullet = 0; ///< sk_plr_357_bullet
        int plr9mmARBullet = 0; ///< sk_plr_9mmAR_bullet
        int plr9mmARGrenade = 0; ///< sk_plr_9mmAR_grenade
        int plrBuckshot = 0; ///< sk_plr_buckshot
        int plrXbowBoltClient = 0; ///< sk_plr_xbow_bolt_client
        int plrXbowBoltMonster = 0; ///< sk_plr_xbow_bolt_monster
        int plrRpg = 0; ///< sk_plr_rpg
        int plrGauss = 0; ///< sk_plr_gauss
        int plrEgonNarrow = 0; ///< sk_plr_egon_narrow
        int plrEgonWide = 0; ///< sk_plr_egon_wide
        int plrHandGrenade = 0; ///< sk_plr_hand_grenade
        int plrSatchel = 0; ///< sk_plr_satchel
        int plrTripmine = 0; ///< sk_plr_tripmine
        int monster12mmBullet = 0; ///< sk_12mm_bullet
        int monster9mmARBullet = 0; ///< sk_9mmAR_bullet
        int monster9mmBullet = 0; ///< sk_9mm_bullet
        int hornetDmg = 0; ///< sk_hornet_dmg
        int suitcharger = 0; ///< sk_suitcharger
        int battery = 0; ///< sk_battery
        int healthcharger = 0; ///< sk_healthcharger
        int healthkit = 0; ///< sk_healthkit
        int scientistHeal = 0; ///< sk_scientist_heal
        int monsterHead = 0; ///< sk_monster_head
        int monsterChest = 0; ///< sk_monster_chest
        int monsterStomach = 0; ///< sk_monster_stomach
        int monsterArm = 0; ///< sk_monster_arm
        int monsterLeg = 0; ///< sk_monster_leg
        int playerHead = 0; ///< sk_player_head
        int playerChest = 0; ///< sk_player_chest
        int playerStomach = 0; ///< sk_player_stomach
        int playerArm = 0; ///< sk_player_arm
        int playerLeg = 0; ///< sk_player_leg
    };

    /// skill.cfg's values: [0] easy, [1] medium, [2] hard.
    inline constexpr SkillData kSkill[3] = {
        {
            .agruntHealth = 60,
            .agruntDmgPunch = 10,
            .apacheHealth = 150,
            .barneyHealth = 35,
            .bullsquidHealth = 40,
            .bullsquidDmgBite = 15,
            .bullsquidDmgWhip = 25,
            .bullsquidDmgSpit = 10,
            .bigmommaHealthFactorTenths = 10,
            .bigmommaDmgSlash = 50,
            .bigmommaDmgBlast = 100,
            .bigmommaRadiusBlast = 250,
            .gargantuaHealth = 800,
            .gargantuaDmgSlash = 10,
            .gargantuaDmgFire = 3,
            .gargantuaDmgStomp = 50,
            .hassassinHealth = 30,
            .headcrabHealth = 10,
            .headcrabDmgBite = 5,
            .hgruntHealth = 50,
            .hgruntKick = 5,
            .hgruntPellets = 3,
            .hgruntGspeed = 400,
            .houndeyeHealth = 20,
            .houndeyeDmgBlast = 10,
            .islaveHealth = 30,
            .islaveDmgClaw = 8,
            .islaveDmgClawrake = 25,
            .islaveDmgZap = 10,
            .ichthyosaurHealth = 200,
            .ichthyosaurShake = 20,
            .leechHealth = 2,
            .leechDmgBite = 2,
            .controllerHealth = 60,
            .controllerDmgzap = 15,
            .controllerSpeedball = 650,
            .controllerDmgball = 3,
            .nihilanthHealth = 800,
            .nihilanthZap = 30,
            .scientistHealth = 20,
            .snarkHealth = 2,
            .snarkDmgBite = 10,
            .snarkDmgPop = 5,
            .zombieHealth = 50,
            .zombieDmgOneSlash = 10,
            .zombieDmgBothSlash = 25,
            .turretHealth = 50,
            .miniturretHealth = 40,
            .sentryHealth = 40,
            .plrCrowbar = 10,
            .plr9mmBullet = 8,
            .plr357Bullet = 40,
            .plr9mmARBullet = 5,
            .plr9mmARGrenade = 100,
            .plrBuckshot = 5,
            .plrXbowBoltClient = 10,
            .plrXbowBoltMonster = 50,
            .plrRpg = 100,
            .plrGauss = 20,
            .plrEgonNarrow = 6,
            .plrEgonWide = 14,
            .plrHandGrenade = 100,
            .plrSatchel = 150,
            .plrTripmine = 150,
            .monster12mmBullet = 8,
            .monster9mmARBullet = 3,
            .monster9mmBullet = 5,
            .hornetDmg = 4,
            .suitcharger = 75,
            .battery = 15,
            .healthcharger = 50,
            .healthkit = 15,
            .scientistHeal = 25,
            .monsterHead = 3,
            .monsterChest = 1,
            .monsterStomach = 1,
            .monsterArm = 1,
            .monsterLeg = 1,
            .playerHead = 3,
            .playerChest = 1,
            .playerStomach = 1,
            .playerArm = 1,
            .playerLeg = 1,
        },
        {
            .agruntHealth = 90,
            .agruntDmgPunch = 20,
            .apacheHealth = 250,
            .barneyHealth = 35,
            .bullsquidHealth = 40,
            .bullsquidDmgBite = 25,
            .bullsquidDmgWhip = 35,
            .bullsquidDmgSpit = 10,
            .bigmommaHealthFactorTenths = 15,
            .bigmommaDmgSlash = 60,
            .bigmommaDmgBlast = 120,
            .bigmommaRadiusBlast = 250,
            .gargantuaHealth = 800,
            .gargantuaDmgSlash = 30,
            .gargantuaDmgFire = 5,
            .gargantuaDmgStomp = 100,
            .hassassinHealth = 50,
            .headcrabHealth = 10,
            .headcrabDmgBite = 10,
            .hgruntHealth = 50,
            .hgruntKick = 10,
            .hgruntPellets = 5,
            .hgruntGspeed = 600,
            .houndeyeHealth = 20,
            .houndeyeDmgBlast = 15,
            .islaveHealth = 30,
            .islaveDmgClaw = 10,
            .islaveDmgClawrake = 25,
            .islaveDmgZap = 10,
            .ichthyosaurHealth = 200,
            .ichthyosaurShake = 35,
            .leechHealth = 2,
            .leechDmgBite = 2,
            .controllerHealth = 60,
            .controllerDmgzap = 25,
            .controllerSpeedball = 800,
            .controllerDmgball = 4,
            .nihilanthHealth = 800,
            .nihilanthZap = 30,
            .scientistHealth = 20,
            .snarkHealth = 2,
            .snarkDmgBite = 10,
            .snarkDmgPop = 5,
            .zombieHealth = 50,
            .zombieDmgOneSlash = 20,
            .zombieDmgBothSlash = 40,
            .turretHealth = 50,
            .miniturretHealth = 40,
            .sentryHealth = 40,
            .plrCrowbar = 10,
            .plr9mmBullet = 8,
            .plr357Bullet = 40,
            .plr9mmARBullet = 5,
            .plr9mmARGrenade = 100,
            .plrBuckshot = 5,
            .plrXbowBoltClient = 10,
            .plrXbowBoltMonster = 50,
            .plrRpg = 100,
            .plrGauss = 20,
            .plrEgonNarrow = 6,
            .plrEgonWide = 14,
            .plrHandGrenade = 100,
            .plrSatchel = 150,
            .plrTripmine = 150,
            .monster12mmBullet = 10,
            .monster9mmARBullet = 4,
            .monster9mmBullet = 5,
            .hornetDmg = 5,
            .suitcharger = 50,
            .battery = 15,
            .healthcharger = 40,
            .healthkit = 15,
            .scientistHeal = 25,
            .monsterHead = 3,
            .monsterChest = 1,
            .monsterStomach = 1,
            .monsterArm = 1,
            .monsterLeg = 1,
            .playerHead = 3,
            .playerChest = 1,
            .playerStomach = 1,
            .playerArm = 1,
            .playerLeg = 1,
        },
        {
            .agruntHealth = 120,
            .agruntDmgPunch = 20,
            .apacheHealth = 400,
            .barneyHealth = 35,
            .bullsquidHealth = 120,
            .bullsquidDmgBite = 25,
            .bullsquidDmgWhip = 35,
            .bullsquidDmgSpit = 15,
            .bigmommaHealthFactorTenths = 20,
            .bigmommaDmgSlash = 70,
            .bigmommaDmgBlast = 160,
            .bigmommaRadiusBlast = 275,
            .gargantuaHealth = 1000,
            .gargantuaDmgSlash = 30,
            .gargantuaDmgFire = 5,
            .gargantuaDmgStomp = 100,
            .hassassinHealth = 50,
            .headcrabHealth = 20,
            .headcrabDmgBite = 10,
            .hgruntHealth = 80,
            .hgruntKick = 10,
            .hgruntPellets = 6,
            .hgruntGspeed = 800,
            .houndeyeHealth = 30,
            .houndeyeDmgBlast = 15,
            .islaveHealth = 60,
            .islaveDmgClaw = 10,
            .islaveDmgClawrake = 25,
            .islaveDmgZap = 15,
            .ichthyosaurHealth = 400,
            .ichthyosaurShake = 50,
            .leechHealth = 2,
            .leechDmgBite = 2,
            .controllerHealth = 100,
            .controllerDmgzap = 35,
            .controllerSpeedball = 1000,
            .controllerDmgball = 5,
            .nihilanthHealth = 1000,
            .nihilanthZap = 50,
            .scientistHealth = 20,
            .snarkHealth = 2,
            .snarkDmgBite = 10,
            .snarkDmgPop = 5,
            .zombieHealth = 100,
            .zombieDmgOneSlash = 20,
            .zombieDmgBothSlash = 40,
            .turretHealth = 60,
            .miniturretHealth = 50,
            .sentryHealth = 50,
            .plrCrowbar = 10,
            .plr9mmBullet = 8,
            .plr357Bullet = 40,
            .plr9mmARBullet = 5,
            .plr9mmARGrenade = 100,
            .plrBuckshot = 5,
            .plrXbowBoltClient = 10,
            .plrXbowBoltMonster = 50,
            .plrRpg = 100,
            .plrGauss = 20,
            .plrEgonNarrow = 6,
            .plrEgonWide = 14,
            .plrHandGrenade = 100,
            .plrSatchel = 150,
            .plrTripmine = 150,
            .monster12mmBullet = 10,
            .monster9mmARBullet = 5,
            .monster9mmBullet = 8,
            .hornetDmg = 8,
            .suitcharger = 35,
            .battery = 10,
            .healthcharger = 25,
            .healthkit = 10,
            .scientistHeal = 25,
            .monsterHead = 3,
            .monsterChest = 1,
            .monsterStomach = 1,
            .monsterArm = 1,
            .monsterLeg = 1,
            .playerHead = 3,
            .playerChest = 1,
            .playerStomach = 1,
            .playerArm = 1,
            .playerLeg = 1,
        },
    };

    /// The values of `level` (kSkill*): MEDIUM for anything else -- a game
    /// that never chose.
    constexpr const SkillData &skillData(int level)
    {
        return kSkill[level >= kSkillEasy && level <= kSkillHard ? level - 1 : kSkillMedium - 1];
    }
} // namespace hldata

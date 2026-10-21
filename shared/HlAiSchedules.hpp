#pragma once

// The schedules every monster has (dlls/defaultai.cpp), as the game lists
// them: their tasks, what interrupts them, and which sounds do. Times are in
// milliseconds (the game's are seconds); everything else is the game's own.
//
// Included by HlAi.hpp.

namespace hldata
{
    inline constexpr AiTask tlFail[] = {
        {kTaskStopMoving, 0},
        {kTaskSetActivity, kActIdle},
        {kTaskWait, 2000},
        {kTaskWaitPvs, 0},
    };
    inline constexpr AiSchedule slFail = {tlFail, 4, kCondCanAttack, 0, "Fail"};

    inline constexpr AiTask tlIdleStand1[] = {
        {kTaskStopMoving, 0},
        {kTaskSetActivity, kActIdle},
        {kTaskWait, 5000}, // (chosen again every five seconds)
    };
    inline constexpr AiSchedule slIdleStand = {
        tlIdleStand1, 3,
        kCondNewEnemy | kCondSeeFear | kCondLightDamage | kCondHeavyDamage | kCondHearSound | kCondSmellFood |
            kCondSmell | kCondProvoked,
        kSoundCombat | kSoundWorld | kSoundPlayer | kSoundDanger | kSoundMeat | kSoundCarcass | kSoundGarbage,
        "IdleStand"};

    inline constexpr AiSchedule slIdleTrigger = {tlIdleStand1, 3, kCondLightDamage | kCondHeavyDamage, 0,
                                                 "Idle Trigger"};

    inline constexpr AiTask tlIdleWalk1[] = {
        {kTaskWalkPath, 9999},
        {kTaskWaitForMovement, 0},
    };
    inline constexpr AiSchedule slIdleWalk = {
        tlIdleWalk1, 2,
        kCondNewEnemy | kCondLightDamage | kCondHeavyDamage | kCondHearSound | kCondSmellFood | kCondSmell |
            kCondProvoked,
        kSoundCombat | kSoundMeat | kSoundCarcass | kSoundGarbage, "Idle Walk"};

    inline constexpr AiTask tlAmbush[] = {
        {kTaskStopMoving, 0},
        {kTaskSetActivity, kActIdle},
        {kTaskWaitIndefinite, 0},
    };
    inline constexpr AiSchedule slAmbush = {tlAmbush, 3,
                                            kCondNewEnemy | kCondLightDamage | kCondHeavyDamage | kCondProvoked, 0,
                                            "Ambush"};

    inline constexpr AiTask tlActiveIdle[] = {
        {kTaskFindHintNode, 0},          {kTaskGetPathToHintNode, 0}, {kTaskStoreLastPosition, 0},
        {kTaskWalkPath, 0},              {kTaskWaitForMovement, 0},   {kTaskFaceHintNode, 0},
        {kTaskPlayActiveIdle, 0},        {kTaskGetPathToLastPosition, 0}, {kTaskWalkPath, 0},
        {kTaskWaitForMovement, 0},       {kTaskClearLastPosition, 0}, {kTaskClearHintNode, 0},
    };
    inline constexpr AiSchedule slActiveIdle = {
        tlActiveIdle, 12, kCondNewEnemy | kCondLightDamage | kCondHeavyDamage | kCondProvoked | kCondHearSound,
        kSoundCombat | kSoundWorld | kSoundPlayer | kSoundDanger, "Active Idle"};

    inline constexpr AiTask tlWakeAngry1[] = {
        {kTaskStopMoving, 0},
        {kTaskSetActivity, kActIdle},
        {kTaskSoundWake, 0},
        {kTaskFaceIdeal, 0},
    };
    inline constexpr AiSchedule slWakeAngry = {tlWakeAngry1, 4, 0, 0, "Wake Angry"};

    inline constexpr AiTask tlAlertFace1[] = {
        {kTaskStopMoving, 0},
        {kTaskSetActivity, kActIdle},
        {kTaskFaceIdeal, 0},
    };
    inline constexpr AiSchedule slAlertFace = {
        tlAlertFace1, 3, kCondNewEnemy | kCondSeeFear | kCondLightDamage | kCondHeavyDamage | kCondProvoked, 0,
        "Alert Face"};

    inline constexpr AiTask tlAlertSmallFlinch[] = {
        {kTaskStopMoving, 0},
        {kTaskRemember, static_cast<std::int32_t>(kMemoryFlinched)},
        {kTaskSmallFlinch, 0},
        {kTaskSetSchedule, kSchedAlertFace},
    };
    inline constexpr AiSchedule slAlertSmallFlinch = {tlAlertSmallFlinch, 4, 0, 0, "Alert Small Flinch"};

    inline constexpr AiTask tlAlertStand1[] = {
        {kTaskStopMoving, 0},
        {kTaskSetActivity, kActIdle},
        {kTaskWait, 20000},
        {kTaskSuggestState, kStateIdle},
    };
    inline constexpr AiSchedule slAlertStand = {
        tlAlertStand1, 4,
        kCondNewEnemy | kCondSeeEnemy | kCondSeeFear | kCondLightDamage | kCondHeavyDamage | kCondProvoked |
            kCondSmell | kCondSmellFood | kCondHearSound,
        kSoundCombat | kSoundWorld | kSoundPlayer | kSoundDanger | kSoundMeat | kSoundCarcass | kSoundGarbage,
        "Alert Stand"};

    inline constexpr AiTask tlInvestigateSound[] = {
        {kTaskStopMoving, 0},           {kTaskStoreLastPosition, 0},     {kTaskGetPathToBestSound, 0},
        {kTaskFaceIdeal, 0},            {kTaskWalkPath, 0},              {kTaskWaitForMovement, 0},
        {kTaskPlaySequence, kActIdle},  {kTaskWait, 10000},              {kTaskGetPathToLastPosition, 0},
        {kTaskWalkPath, 0},             {kTaskWaitForMovement, 0},       {kTaskClearLastPosition, 0},
    };
    inline constexpr AiSchedule slInvestigateSound = {
        tlInvestigateSound, 12, kCondNewEnemy | kCondSeeFear | kCondLightDamage | kCondHeavyDamage | kCondHearSound,
        kSoundDanger, "InvestigateSound"};

    inline constexpr AiTask tlCombatStand1[] = {
        {kTaskStopMoving, 0},
        {kTaskSetActivity, kActIdle},
        {kTaskWaitIndefinite, 0},
    };
    inline constexpr AiSchedule slCombatStand = {
        tlCombatStand1, 3, kCondNewEnemy | kCondEnemyDead | kCondLightDamage | kCondHeavyDamage | kCondCanAttack, 0,
        "Combat Stand"};

    inline constexpr AiTask tlCombatFace1[] = {
        {kTaskStopMoving, 0},
        {kTaskSetActivity, kActIdle},
        {kTaskFaceEnemy, 0},
    };
    inline constexpr AiSchedule slCombatFace = {tlCombatFace1, 3, kCondCanAttack | kCondNewEnemy | kCondEnemyDead, 0,
                                                "Combat Face"};

    inline constexpr AiTask tlStandoff[] = {
        {kTaskStopMoving, 0},
        {kTaskSetActivity, kActIdle},
        {kTaskWaitFaceEnemy, 2000},
    };
    inline constexpr AiSchedule slStandoff = {
        tlStandoff, 3,
        kCondCanRangeAttack1 | kCondCanRangeAttack2 | kCondEnemyDead | kCondNewEnemy | kCondHearSound, kSoundDanger,
        "Standoff"};

    inline constexpr AiTask tlArmWeapon[] = {
        {kTaskStopMoving, 0},
        {kTaskPlaySequence, kActArm},
    };
    inline constexpr AiSchedule slArmWeapon = {tlArmWeapon, 2, 0, 0, "Arm Weapon"};

    inline constexpr AiTask tlReload[] = {
        {kTaskStopMoving, 0},
        {kTaskPlaySequence, kActReload},
    };
    inline constexpr AiSchedule slReload = {tlReload, 2, kCondHeavyDamage, 0, "Reload"};

    inline constexpr AiTask tlRangeAttack1[] = {
        {kTaskStopMoving, 0},
        {kTaskFaceEnemy, 0},
        {kTaskRangeAttack1, 0},
    };
    inline constexpr AiSchedule slRangeAttack1 = {
        tlRangeAttack1, 3,
        kCondNewEnemy | kCondEnemyDead | kCondLightDamage | kCondHeavyDamage | kCondEnemyOccluded |
            kCondNoAmmoLoaded | kCondHearSound,
        kSoundDanger, "Range Attack1"};

    inline constexpr AiTask tlRangeAttack2[] = {
        {kTaskStopMoving, 0},
        {kTaskFaceEnemy, 0},
        {kTaskRangeAttack2, 0},
    };
    inline constexpr AiSchedule slRangeAttack2 = {
        tlRangeAttack2, 3,
        kCondNewEnemy | kCondEnemyDead | kCondLightDamage | kCondHeavyDamage | kCondEnemyOccluded | kCondHearSound,
        kSoundDanger, "Range Attack2"};

    inline constexpr AiTask tlPrimaryMeleeAttack1[] = {
        {kTaskStopMoving, 0},
        {kTaskFaceEnemy, 0},
        {kTaskMeleeAttack1, 0},
    };
    inline constexpr AiSchedule slPrimaryMeleeAttack = {
        tlPrimaryMeleeAttack1, 3,
        kCondNewEnemy | kCondEnemyDead | kCondLightDamage | kCondHeavyDamage | kCondEnemyOccluded, 0,
        "Primary Melee Attack"};

    inline constexpr AiTask tlSecondaryMeleeAttack1[] = {
        {kTaskStopMoving, 0},
        {kTaskFaceEnemy, 0},
        {kTaskMeleeAttack2, 0},
    };
    inline constexpr AiSchedule slSecondaryMeleeAttack = {
        tlSecondaryMeleeAttack1, 3,
        kCondNewEnemy | kCondEnemyDead | kCondLightDamage | kCondHeavyDamage | kCondEnemyOccluded, 0,
        "Secondary Melee Attack"};

    inline constexpr AiTask tlSpecialAttack1[] = {
        {kTaskStopMoving, 0},
        {kTaskFaceEnemy, 0},
        {kTaskSpecialAttack1, 0},
    };
    inline constexpr AiSchedule slSpecialAttack1 = {
        tlSpecialAttack1, 3,
        kCondNewEnemy | kCondEnemyDead | kCondLightDamage | kCondHeavyDamage | kCondEnemyOccluded |
            kCondNoAmmoLoaded | kCondHearSound,
        kSoundDanger, "Special Attack1"};

    inline constexpr AiTask tlSpecialAttack2[] = {
        {kTaskStopMoving, 0},
        {kTaskFaceEnemy, 0},
        {kTaskSpecialAttack2, 0},
    };
    inline constexpr AiSchedule slSpecialAttack2 = {
        tlSpecialAttack2, 3,
        kCondNewEnemy | kCondEnemyDead | kCondLightDamage | kCondHeavyDamage | kCondEnemyOccluded |
            kCondNoAmmoLoaded | kCondHearSound,
        kSoundDanger, "Special Attack2"};

    inline constexpr AiTask tlChaseEnemy1[] = {
        {kTaskSetFailSchedule, kSchedChaseEnemyFailed},
        {kTaskGetPathToEnemy, 0},
        {kTaskRunPath, 0},
        {kTaskWaitForMovement, 0},
    };
    inline constexpr AiSchedule slChaseEnemy = {
        tlChaseEnemy1, 4,
        kCondNewEnemy | kCondCanRangeAttack1 | kCondCanMeleeAttack1 | kCondCanRangeAttack2 | kCondCanMeleeAttack2 |
            kCondTaskFailed | kCondHearSound,
        kSoundDanger, "Chase Enemy"};

    inline constexpr AiTask tlChaseEnemyFailed[] = {
        {kTaskStopMoving, 0},        {kTaskWait, 200},
        {kTaskFindCoverFromEnemy, 0}, {kTaskRunPath, 0},
        {kTaskWaitForMovement, 0},   {kTaskRemember, static_cast<std::int32_t>(kMemoryInCover)},
        {kTaskFaceEnemy, 0},         {kTaskWait, 1000},
    };
    inline constexpr AiSchedule slChaseEnemyFailed = {
        tlChaseEnemyFailed, 8,
        kCondNewEnemy | kCondCanRangeAttack1 | kCondCanMeleeAttack1 | kCondCanRangeAttack2 | kCondCanMeleeAttack2 |
            kCondHearSound,
        kSoundDanger, "tlChaseEnemyFailed"};

    inline constexpr AiTask tlSmallFlinch[] = {
        {kTaskRemember, static_cast<std::int32_t>(kMemoryFlinched)},
        {kTaskStopMoving, 0},
        {kTaskSmallFlinch, 0},
    };
    inline constexpr AiSchedule slSmallFlinch = {tlSmallFlinch, 3, 0, 0, "Small Flinch"};

    inline constexpr AiTask tlDie1[] = {
        {kTaskStopMoving, 0},
        {kTaskSoundDie, 0},
        {kTaskDie, 0},
    };
    inline constexpr AiSchedule slDie = {tlDie1, 3, 0, 0, "Die"};

    inline constexpr AiTask tlVictoryDance[] = {
        {kTaskStopMoving, 0},
        {kTaskPlaySequence, kActVictoryDance},
        {kTaskWait, 0},
    };
    inline constexpr AiSchedule slVictoryDance = {tlVictoryDance, 3, 0, 0, "Victory Dance"};

    inline constexpr AiTask tlBarnacleVictimGrab[] = {
        {kTaskStopMoving, 0},
        {kTaskPlaySequence, kActBarnacleHit},
        {kTaskSetActivity, kActBarnaclePull},
        {kTaskWaitIndefinite, 0}, // (the pull goes round while the barnacle hoists)
    };
    inline constexpr AiSchedule slBarnacleVictimGrab = {tlBarnacleVictimGrab, 4, 0, 0, "Barnacle Victim"};

    inline constexpr AiTask tlBarnacleVictimChomp[] = {
        {kTaskStopMoving, 0},
        {kTaskPlaySequence, kActBarnacleChomp},
        {kTaskSetActivity, kActBarnacleChew},
        {kTaskWaitIndefinite, 0},
    };
    inline constexpr AiSchedule slBarnacleVictimChomp = {tlBarnacleVictimChomp, 4, 0, 0, "Barnacle Chomp"};

    inline constexpr AiTask tlError[] = {
        {kTaskStopMoving, 0},
        {kTaskWaitIndefinite, 0},
    };
    inline constexpr AiSchedule slError = {tlError, 2, 0, 0, "Error"};

    inline constexpr AiTask tlCower[] = {
        {kTaskStopMoving, 0},
        {kTaskPlaySequence, kActCower},
    };
    inline constexpr AiSchedule slCower = {tlCower, 2, 0, 0, "Cower"};

    inline constexpr AiTask tlTakeCoverFromOrigin[] = {
        {kTaskStopMoving, 0},
        {kTaskFindCoverFromOrigin, 0},
        {kTaskRunPath, 0},
        {kTaskWaitForMovement, 0},
        {kTaskRemember, static_cast<std::int32_t>(kMemoryInCover)},
        {kTaskTurnLeft, 179},
    };
    inline constexpr AiSchedule slTakeCoverFromOrigin = {tlTakeCoverFromOrigin, 6, kCondNewEnemy, 0,
                                                         "TakeCoverFromOrigin"};

    inline constexpr AiTask tlTakeCoverFromBestSound[] = {
        {kTaskStopMoving, 0},
        {kTaskFindCoverFromBestSound, 0},
        {kTaskRunPath, 0},
        {kTaskWaitForMovement, 0},
        {kTaskRemember, static_cast<std::int32_t>(kMemoryInCover)},
        {kTaskTurnLeft, 179},
    };
    inline constexpr AiSchedule slTakeCoverFromBestSound = {tlTakeCoverFromBestSound, 6, kCondNewEnemy, 0,
                                                            "TakeCoverFromBestSound"};

    inline constexpr AiTask tlTakeCoverFromEnemy[] = {
        {kTaskStopMoving, 0},        {kTaskWait, 200},
        {kTaskFindCoverFromEnemy, 0}, {kTaskRunPath, 0},
        {kTaskWaitForMovement, 0},   {kTaskRemember, static_cast<std::int32_t>(kMemoryInCover)},
        {kTaskFaceEnemy, 0},         {kTaskWait, 1000},
    };
    inline constexpr AiSchedule slTakeCoverFromEnemy = {tlTakeCoverFromEnemy, 8, kCondNewEnemy, 0,
                                                        "tlTakeCoverFromEnemy"};
} // namespace hldata

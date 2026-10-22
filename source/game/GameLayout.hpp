#pragma once

// GameLayout (GameApi.hpp) as this side of the line sees it.

#include "game/GameApi.hpp"

#include "audio/Sound.hpp"
#include "data/LevelFile.hpp"
#include "data/ModelCache.hpp"
#include "data/TextureStream.hpp"
#include "game/Input.hpp"
#include "game/Player.hpp"
#include "game/Think.hpp"
#include "net/NetGame.hpp"
#include "render/BottomScreen.hpp"
#include "render/Camera.hpp"
#include "render/HudDraw.hpp"
#include "render/ScreenFade.hpp"
#include "render/TopText.hpp"

namespace hlrt
{
    inline GameLayout gameLayout()
    {
        return GameLayout{{
            sizeof(Game),
            sizeof(hldata::LogicWorld),
            sizeof(hldata::ActorWorld),
            sizeof(hldata::ActorState),
            sizeof(hldata::AiWorld),
            sizeof(hldata::AiMonster),
            sizeof(hldata::PlayerState),
            sizeof(hldata::SaveData),
            sizeof(hldata::NetMatch),
            sizeof(EngineFuncs),
            sizeof(GameFuncs),
            sizeof(BottomScreen),
            sizeof(Camera),
            sizeof(HudDraw),
            sizeof(Input),
            sizeof(Level),
            sizeof(ModelCache),
            sizeof(NetGame),
            sizeof(Player),
            sizeof(ScreenFade),
            sizeof(Sound),
            sizeof(TextureStream),
            sizeof(Think),
            sizeof(TopText),
        }};
    }
} // namespace hlrt

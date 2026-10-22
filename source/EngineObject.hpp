#pragma once

// HL_ENGINE_OBJECT(Type, name) declares one of the engine's objects (the
// camera, the sound, the running map...) where its class is.
//
// In the engine it is the object itself. In the game library (HL_DSL, see
// game/GameApi.hpp) it is a reference to the engine's, bound from the
// engine's table when the library is loaded (dll/hl/EngineLink.cpp): the
// library has no object of its own, and takes no address of the engine's but
// through the table.
#ifdef HL_DSL
#define HL_ENGINE_OBJECT(Type, name) extern Type &name
#else
#define HL_ENGINE_OBJECT(Type, name) extern Type name
#endif

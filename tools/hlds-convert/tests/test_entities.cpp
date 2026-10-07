// The entity lump parser. Small, but every later stage keys off the entity
// list it produces, and real maps hold oddities worth pinning: repeated keys,
// values with spaces, and a NUL after the last brace.

#include <cstdio>

#include "TestBspBuilder.hpp"
#include "hlds/EntityParser.hpp"

using namespace hlds;
using namespace hldstest;

int main()
{
    std::vector<Entity> ents;
    std::string error;

    const std::string text =
        "{\n\"classname\" \"worldspawn\"\n\"message\" \"Black Mesa Inbound\"\n}\n"
        "{\n\"classname\" \"multi_manager\"\n\"door1\" \"0\"\n\"door1\" \"2.5\"\n"
        "\"origin\" \"-12 34.5 100\"\n}\n";
    check(parseEntities(text, ents, error), "two entities parse");
    check(ents.size() == 2, "two entities");
    check(ents[0].className() == "worldspawn", "classname");
    check(ents[0].value("message") == "Black Mesa Inbound", "values keep their spaces");
    check(ents[0].value("absent", "x") == "x", "fallback for an absent key");
    check(ents[1].pairs.size() == 4, "repeated keys are both kept");
    check(ents[1].pairs[1].second == "0" && ents[1].pairs[2].second == "2.5", "in order");

    double v[3] = {0, 0, 0};
    check(ents[1].vec3("origin", v) && v[0] == -12 && v[1] == 34.5 && v[2] == 100, "vec3");
    check(!ents[1].vec3("door1", v), "a single number is not a vec3");
    check(!ents[0].vec3("origin", v), "an absent key is not a vec3");

    check(parseEntities(std::string("{\"a\" \"b\"}\n\0", 11), ents, error) && ents.size() == 1,
          "a trailing NUL is ignored");
    check(parseEntities("", ents, error) && ents.empty(), "an empty lump is no entities");

    check(!parseEntities("{\"a\" \"b\"", ents, error) && ents.empty(), "unclosed entity");
    check(!parseEntities("{\"a\"}", ents, error), "key without a value");
    check(!parseEntities("\"a\" \"b\"", ents, error), "pair outside braces");

    // The builder's own lump must go through too: the BSP tests rely on it.
    BspFile bsp;
    check(bsp.parse(buildBoxRoom(), error), "box room parses");
    check(parseEntities(bsp.entities, ents, error) && ents.size() == 2, "box room entities");
    check(ents[1].vec3("origin", v) && v[2] == 40, "player start origin");

    if (g_failures == 0)
        std::printf("test_entities: ok\n");
    return g_failures == 0 ? 0 : 1;
}

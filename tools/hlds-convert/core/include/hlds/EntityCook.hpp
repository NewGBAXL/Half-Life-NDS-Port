#pragma once

// The map's entities that DO something, turned into logic records.
//
// On the PC an entity is a bag of text keys that the game parses when the map
// loads and keeps consulting as it runs. On the DS none of that happens: this
// reads the keys once, here, and writes down what they amount to -- a door's
// travel as a vector, a wait as milliseconds, a name as a number -- so that
// the console only ever sees fixed records (LogicRecord, shared/HlLevel.hpp)
// and the code that runs them (shared/HlLogic.hpp) never touches a string.
//
// Classes this does not know are COUNTED, not refused. A map with monsters and
// sound entities still cooks; the count is how far the port has to go.

#include <map>
#include <string>
#include <vector>

#include "HlLevel.hpp"
#include "hlds/SoundCook.hpp"
#include "hlds/TitlesFile.hpp"
#include "hlds/BrushInfo.hpp"
#include "hlds/BspFile.hpp"
#include "hlds/EntityParser.hpp"

namespace hlds
{
    struct LogicCook
    {
        std::vector<hldata::LogicRecord> records;
        std::vector<hldata::LogicAux> aux;
        std::vector<hldata::PathCorner> corners;
        std::vector<std::uint32_t> nameHashes; ///< by name id
        std::vector<hldata::Landmark> landmarks;
        std::vector<char> strings;
        std::vector<hldata::ModelRef> modelRefs;
        std::vector<hldata::ActorRecord> actors;
        /// The sounds the map plays, as the game's own paths under sound/
        /// ("doors/doormove2.wav", lower-case); empty where the map has no
        /// use for a common sound. The index is the index in the level's
        /// Sounds. `soundLoops` says which go round.
        std::vector<std::string> sounds;
        std::vector<bool> soundLoops;
        std::vector<hldata::LogicSound> logicSounds; ///< parallel to `records`
        /// The sprites the map draws, as the game's own paths
        /// ("sprites/flare1.spr", lower-case), the common ones first; and
        /// whether each is drawn as light, whatever its file says.
        std::vector<std::string> sprites;
        std::vector<bool> spriteLights;
        std::vector<hldata::SentenceRecord> sentences;
        std::vector<hldata::SentenceWord> sentenceWords;
        std::vector<hldata::RoomRecord> rooms;
        /// What soldiers call out, if the map has any: kSpeechGroups *
        /// kSpeechLines indices into `sentences` (0xFFFF: none).
        std::vector<std::uint16_t> speech;
        /// What the map writes on the screen (their text is in `strings`).
        std::vector<hldata::MessageRecord> messages;
        /// By entity index: its logic record, or -1.
        std::vector<int> recordOfEntity;
        int names = 0; ///< distinct target names
        /// Classes with no logic record and no other handling, with how many
        /// entities of each the map has.
        std::map<std::string, int> unsupported;
    };

    /// A model a map needs, and the sequences of it the map plays.
    struct ModelNeed
    {
        std::string name; ///< lower-case, no directory or extension: "scientist"
        std::string file; ///< as the game names it: "models/scientist.mdl"
        std::vector<std::string> clips; ///< sequence labels, lower-case
        bool firstSequence = false;     ///< ... and whichever its first sequence is
    };

    /// What the map's entities need by way of models: every model some actor
    /// is made of, with the clips those actors stand in and are scripted to
    /// play. Nothing is read from the models themselves.
    std::vector<ModelNeed> collectModelNeeds(const std::vector<Entity> &entities);

    /// Add to `needs` what the actors who may ARRIVE from a neighbouring map
    /// need: those with a name who stand near the landmark called `landmark`
    /// there come along when the player crosses (the guard who opens the
    /// tram's door at the station is waiting in the map before it), and this
    /// map must have their models, with the clips its scripts have them play.
    void addVisitorNeeds(const std::vector<Entity> &entities, const std::vector<Entity> &neighbour,
                         const std::string &landmark, std::vector<ModelNeed> &needs);

    /// `models` are the names of the models that were cooked for this map
    /// (ModelNeed::name), in the order they will be in the level: an actor
    /// whose model is not among them is left out.
    /// `brushRecordOfEntity` gives, by entity index, the Brush record the
    /// cooker made for that entity, or -1.
    /// `book` is the game's sentences.txt, if it was there to be read: the
    /// lines the map speaks are looked up in it. `titles` is its titles.txt,
    /// likewise: what the map's messages say and how they are shown.
    /// `chapter` is the name, in titles.txt, of the title of the chapter the
    /// map is part of (Converter works it out: chapterOf); empty if unknown.
    void cookLogic(const BspFile &bsp, const std::vector<Entity> &entities,
                   const std::vector<int> &brushRecordOfEntity,
                   const std::vector<std::string> &models, LogicCook &out,
                   const SentenceBook *book = nullptr, const TitlesBook *titles = nullptr,
                   const std::string &chapter = std::string());

    /// How far a door or button travels: the engine's formula, from the
    /// direction it moves in, the size of its brush and its "lip".
    void moverTravel(const double *direction, const double *size, double lip, double *out);

    /// The direction an entity's "angles" / "angle" key means for a mover: -1
    /// is up and -2 is down, anything else is a compass heading.
    void moverDirection(const Entity &entity, double *out);
} // namespace hlds

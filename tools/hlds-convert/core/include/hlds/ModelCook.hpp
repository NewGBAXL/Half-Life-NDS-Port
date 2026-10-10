#pragma once

// From a studio model to what the DS draws: mdl/<name>.hmd (shared/HlModel.hpp)
// and its skin, one texture.
//
// Three things are worked out here so that the DS does not have to:
//
//   THE SKIN AS COMMAND STREAMS. The model's strips and fans become triangle
//   strips and triangles of geometry-engine commands, cut into batches that
//   each use few enough bones to fit the hardware's matrix stack, with a
//   "use this bone's matrix" command wherever the bone changes.
//
//   ONE TEXTURE. A model comes with twenty or thirty little textures of any
//   size; the DS wants few, and powers of two. They are packed into one atlas
//   and the triangles' coordinates moved to match.
//
//   ANIMATION AS KEYS. Each sequence the map uses becomes, per bone, a short
//   list of keys: a bone that does not move in the sequence gets one, a bone
//   that moves slowly gets one every several frames, and the step is the
//   largest that reproduces every original frame to within a degree and a
//   quarter of a unit.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "HlModel.hpp"
#include "hlds/MdlFile.hpp"
#include "hlds/TextureImage.hpp"

namespace hlds
{
    /// Palette colours a model's skin is encoded with.
    inline constexpr int kModelSkinColours = 512;
    /// What a model's skin is thinned to: this many triangles, or a quarter of
    /// what it had if that is more (a loader is not a scientist). A model at
    /// or under it is left alone. Three or four people at this fit a pass
    /// with the world; at their original 600 to 800 triangles, one did.
    inline constexpr int kModelTriangleTarget = 350;
    /// Below that there are two coarser levels, of a half and of about a
    /// third of it (a person in fewer than that has lost half of their
    /// outline: measured) -- but none of fewer triangles than this, and none
    /// at all for a model of not twice as many.
    inline constexpr int kModelLevelLeast = 60;

    /// The first-person model every map is given.
    inline constexpr const char *kViewModelName = "v_crowbar";

    struct CookedModel
    {
        hldata::ModelHeader header{};
        std::vector<std::int16_t> parents;
        std::vector<hldata::ModelBodyPart> bodyParts;
        std::vector<hldata::ModelBatch> batches;
        std::vector<std::uint32_t> gx;
        std::vector<hldata::ModelClip> clips;
        std::vector<hldata::ModelTrack> tracks;
        std::vector<hldata::ModelKey> keys;
        std::vector<hldata::ModelEvent> events;
        /// The sounds its kept clips make (the game's names, lower-case,
        /// without the mark in front that says how to stream them): not
        /// written with the model -- the level says where each is.
        std::vector<std::string> eventSounds;
        std::vector<hldata::ModelController> controllers;
        std::vector<hldata::ModelControlAxis> controlAxes;
        std::vector<hldata::ModelHitbox> hitboxes;
        std::vector<hldata::ModelAttachment> attachments;
        /// The skin, before it is encoded: empty for a model with no textures.
        RgbaImage atlas;
        /// Each level's error, as the header has it (ModelHeader::levelError).
        std::uint8_t levelError[hldata::kModelLevels] = {0, hldata::kModelNoLevel, hldata::kModelNoLevel,
                                                         hldata::kModelNoLevel};
    };

    struct ModelCookReport
    {
        int sourceTriangles = 0; ///< body 0, as the model had them
        int triangles = 0;     ///< body 0, as cooked, at its finest level
        /// Body 0 at each level of detail the file has, finest first.
        struct Level
        {
            int triangles = 0;
            int vertices = 0; ///< as sent: what its strips cost
            double error = 0; ///< units (MeshLevel::error)
        };
        std::vector<Level> levels;
        int batches = 0;
        int clips = 0;
        int sourceFrames = 0;  ///< frames * bones of the clips, as the model had them
        int keys = 0;          ///< ... and as cooked
        std::vector<std::string> missingClips; ///< asked for, not in the model
    };

    /// The most texels a model's skin may take, unless told less.
    inline constexpr int kModelSkinTexels = 256 * 256;
    /// What a map's models may take of the video memory between them: the
    /// largest skins are made smaller until they do (Converter).
    inline constexpr int kMapSkinTexels = 208 * 1024;

    /// Cook `mdl` with the sequences named in `clipNames` (labels, any case).
    /// Its skin is at most `skinTexels` texels (a power of two each way; the
    /// smallest is 32 x 32).
    /// `full`: with the model as the game has it as its finest level, above
    /// the thinned ones (where it was thinned at all) -- a DSi's file.
    /// A sound's name as an event has it, as the level knows it.
    std::string eventSoundName(std::string path);

    bool cookModel(const MdlFile &mdl, const std::vector<std::string> &clipNames, CookedModel &out,
                   ModelCookReport &report, std::string &error, int skinTexels = kModelSkinTexels,
                   bool full = false);

    /// Serialize, then open the result with the DS's reader: fails rather
    /// than return a file the console would refuse.
    bool writeModel(const CookedModel &model, std::vector<std::uint8_t> &out, std::string &error);

    /// The game's own way from three angles (radians, about x y z) to a
    /// quaternion (x y z w): AngleQuaternion. Here for the cooker and for the
    /// tests that hold the DS's maths against the game's.
    void mdlAngleQuaternion(const float *angles, double *quat);

    /// Every bone's matrix in model space for one frame's poses (bones.size()
    /// of them), in doubles, the way the game builds them: twelve numbers a
    /// bone, three rows of rotation-then-place.
    void mdlBoneMatrices(const MdlFile &mdl, const MdlPose *poses,
                         std::vector<std::array<double, 12>> &out);
} // namespace hlds

#pragma once

// Half-Life's studio models: the .mdl file, read into plain structures.
//
// A model is a skeleton, any number of animations of it ("sequences"), and a
// skin of triangles in which EVERY VERTEX BELONGS TO EXACTLY ONE BONE. That
// last fact is what makes these models cheap enough for the DS: a vertex is
// stored where it sits relative to its bone and never changes; animating the
// model is animating a couple of dozen matrices.
//
// One model is up to three kinds of file:
//   <name>.mdl     "IDST": bones, sequences, meshes -- and textures, unless
//   <name>T.mdl    "IDST": ... they are here instead (when the first has none)
//   <name>NN.mdl   "IDSQ": the animation data of sequence group NN (01, 02...)
//
// The reader's job is to turn all of that into things the cooker can use
// without knowing the format: meshes as lists of strips and fans, animation
// as each bone's position and angles at each frame.
//
// Layouts are the SDK's engine/studio.h; the decoding of animation is its
// StudioModelRenderer (CalcBoneQuaternion, CalcBonePosition).

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace hlds
{
    // Texture flags (studio.h STUDIO_NF_*).
    inline constexpr int kMdlTextureFlatShade = 1;
    inline constexpr int kMdlTextureChrome = 2;
    inline constexpr int kMdlTextureMasked = 0x40;

    inline constexpr int kMdlSequenceLooping = 1;

    struct MdlBone
    {
        std::string name;
        int parent = -1; ///< always a lower index, or -1
        /// Where it rests relative to its parent: x y z, then rotation about
        /// x y z in radians. An animation gives values to ADD to these,
        /// scaled by `scale`.
        float value[6] = {0, 0, 0, 0, 0, 0};
        float scale[6] = {0, 0, 0, 0, 0, 0};
    };

    struct MdlEvent
    {
        int frame = 0;
        int event = 0; ///< 1003: fire the target named in `options`
        std::string options;
    };

    struct MdlSequence
    {
        std::string label;
        float fps = 0;
        int flags = 0; ///< kMdlSequence*
        int frameCount = 0;
        int blendCount = 0;
        int activity = 0;
        /// How far the whole sequence carries the model, in its own space:
        /// what a walk covers in one cycle.
        float linearMovement[3] = {0, 0, 0};
        int motionType = 0; ///< which axes of the motion bone that takes out
        int motionBone = 0;
        std::vector<MdlEvent> events;
        // Where its animation is: for MdlFile::decode.
        int group = 0;
        std::uint32_t animOffset = 0;
    };

    /// One bone at one frame: position, and rotation about x y z in radians.
    struct MdlPose
    {
        float pos[3];
        float rot[3];
    };

    struct MdlCorner
    {
        int vertex = 0;
        int normal = 0;
        int s = 0, t = 0; ///< texels of the mesh's texture
    };

    struct MdlStrip
    {
        bool fan = false; ///< a triangle fan; otherwise a strip
        std::vector<MdlCorner> corners;
    };

    struct MdlMesh
    {
        int skinRef = 0; ///< which entry of a skin family its texture is
        int triangles = 0;
        std::vector<MdlStrip> strips;
    };

    struct MdlSubModel
    {
        std::string name;
        std::vector<std::array<float, 3>> vertices; ///< each in its bone's space
        std::vector<std::uint8_t> vertexBone;
        std::vector<std::array<float, 3>> normals;
        std::vector<std::uint8_t> normalBone;
        std::vector<MdlMesh> meshes;

        int triangleCount() const;
    };

    /// A part of the body that comes in several versions: heads, a holster
    /// with or without its gun. An entity's "body" number picks one of each:
    /// (body / base) % models.size().
    struct MdlBodyPart
    {
        std::string name;
        int base = 1;
        std::vector<MdlSubModel> models;
    };

    struct MdlTexture
    {
        std::string name;
        int flags = 0; ///< kMdlTexture*
        int width = 0;
        int height = 0;
        std::vector<std::uint8_t> pixels;  ///< width * height palette indices
        std::array<std::uint8_t, 768> palette{}; ///< RGB
    };

    class MdlFile
    {
    public:
        std::string name; ///< the file's name without directory or extension, lower case
        std::vector<MdlBone> bones;
        std::vector<MdlSequence> sequences;
        std::vector<MdlBodyPart> bodyParts;
        std::vector<MdlTexture> textures;
        /// Skin families: each a list, indexed by a mesh's skinRef, of indices
        /// into `textures`. Family 0 is the model as it usually looks.
        std::vector<std::vector<int>> skins;

        /// Read a model and the files that belong to it. `path` is the main
        /// .mdl; its directory is searched for the others, whatever the case
        /// of their names.
        bool load(const std::filesystem::path &path, std::string &error);

        /// The whole of a sequence: frameCount * bones.size() poses, a frame
        /// after another, bones in order. (Blend 0 only: the sequences that
        /// blend are aiming and shooting, which nothing here does yet.)
        bool decode(int sequence, std::vector<MdlPose> &out, std::string &error) const;

        int sequenceByName(const std::string &label) const; ///< case-insensitive; -1 if none
        /// The sub-model of each body part that `body` selects.
        std::vector<const MdlSubModel *> selectBody(int body) const;
        /// Which bones any vertex of the model is on.
        std::vector<char> usedBones() const;

    private:
        std::vector<std::uint8_t> main_;
        std::vector<std::vector<std::uint8_t>> groups_; ///< [0] unused: group 0 is main_
    };
} // namespace hlds

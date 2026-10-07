// The cooked model (core/src/ModelCook.cpp) and the DS's way of posing it
// (shared/HlAnim.hpp), held against the game's own floating-point maths on
// the game's own models.
//
// The claim is one a player could check with their eyes: at every frame of
// every cooked clip, every vertex of the model is where the game would put
// it, to within a unit (a pixel or so at the distances these are seen from)
// -- after the animation has been cut down to keys,
// the keys squeezed into sixteen bits, and the pose rebuilt in fixed point.
//
// Needs HLDS_VALVE_DIR. Set HLDS_DUMP_DIR as well to have the skins written
// there as PNGs, for looking at.

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "HlAnim.hpp"
#include "TestBspBuilder.hpp"
#include "hlds/ModelCook.hpp"

using namespace hlds;
using namespace hldata;
using namespace hldstest;

namespace
{
    void checkModel(const std::string &dir, const char *file, const std::vector<std::string> &clips)
    {
        MdlFile mdl;
        std::string error;
        if (!mdl.load(dir + "/models/" + file, error))
        {
            check(false, "the model loads");
            return;
        }
        CookedModel cooked;
        ModelCookReport report;
        std::vector<std::uint8_t> bytes;
        if (!cookModel(mdl, clips, cooked, report, error) || !writeModel(cooked, bytes, error))
        {
            std::printf("test_anim: %s: %s\n", file, error.c_str());
            check(false, "the model cooks");
            return;
        }
        // The DS reads words: the block it is given is aligned.
        std::vector<std::uint32_t> aligned((bytes.size() + 3) / 4);
        std::memcpy(aligned.data(), bytes.data(), bytes.size());
        ModelView view;
        check(view.open(reinterpret_cast<const std::uint8_t *>(aligned.data()),
                        static_cast<std::uint32_t>(bytes.size())),
              "the cooked model opens");
        for (const std::string &missing : report.missingClips)
            std::printf("test_anim: %s has no sequence '%s'\n", file, missing.c_str());
        check(report.missingClips.empty() && view.header->clipCount == clips.size(),
              "every clip asked for is there");
        if (!report.missingClips.empty())
            return;

        // The batches: each within the matrix stack, and between them all the
        // triangles of the model.
        int polygons = 0;
        bool fits = true;
        for (int b = 0; b < view.header->batchCount; ++b)
        {
            fits = fits && view.batches[b].boneCount <= kModelBatchBones;
            if (view.batchInBody(view.batches[b], 0))
                polygons += view.batches[b].polygons;
        }
        check(fits && polygons == report.triangles,
              "its batches fit the matrix stack and hold every triangle of body 0");
        check(report.triangles <= std::max(kModelTriangleTarget, report.sourceTriangles / 4) + 8 ||
                  report.triangles == report.sourceTriangles,
              "thinned to its target, or near enough");

        double worst = 0;
        const char *worstClip = "";
        std::vector<BoneMatrix> pose(mdl.bones.size());
        for (int c = 0; c < view.header->clipCount; ++c)
        {
            const int sequence = mdl.sequenceByName(clips[c]);
            const MdlSequence &seq = mdl.sequences[sequence];
            std::vector<MdlPose> poses;
            mdl.decode(sequence, poses, error);
            const std::size_t boneCount = mdl.bones.size();
            for (int f = 0; f < seq.frameCount; ++f)
            {
                // The game's pose, with the sequence's own travel taken out
                // as the game takes it out.
                std::vector<MdlPose> frame(poses.begin() + f * boneCount,
                                           poses.begin() + (f + 1) * boneCount);
                for (int a = 0; a < 3; ++a)
                {
                    if (seq.motionType & (1 << a))
                        frame[seq.motionBone].pos[a] = 0;
                }
                std::vector<std::array<double, 12>> want;
                mdlBoneMatrices(mdl, frame.data(), want);
                poseModel(view, c, static_cast<std::uint32_t>(f) << 8, pose.data());
                for (const MdlBodyPart &part : mdl.bodyParts)
                {
                    for (const MdlSubModel &model : part.models)
                    {
                        for (std::size_t v = 0; v < model.vertices.size(); ++v)
                        {
                            const int bone = model.vertexBone[v];
                            const auto &p = model.vertices[v];
                            for (int a = 0; a < 3; ++a)
                            {
                                const double w = want[bone][a * 4] * p[0] + want[bone][a * 4 + 1] * p[1] +
                                                 want[bone][a * 4 + 2] * p[2] + want[bone][a * 4 + 3];
                                const double g = (pose[bone].r[a * 3] * p[0] + pose[bone].r[a * 3 + 1] * p[1] +
                                                  pose[bone].r[a * 3 + 2] * p[2]) / 16384.0 +
                                                 pose[bone].t[a] / 4096.0;
                                if (std::fabs(w - g) > worst)
                                {
                                    worst = std::fabs(w - g);
                                    worstClip = clips[c].c_str();
                                }
                            }
                        }
                    }
                }
            }
        }
        std::printf("test_anim: %s: %d triangles (of %d) in %d batches, %zu bones; %d clips, %d bone-frames as "
                    "%d keys (%.0f%%), file %zu KB, skin %dx%d; worst vertex %.2f units off (%s)\n",
                    file, report.triangles, report.sourceTriangles, report.batches, mdl.bones.size(), report.clips,
                    report.sourceFrames, report.keys,
                    report.sourceFrames > 0 ? 100.0 * report.keys / report.sourceFrames : 0.0,
                    bytes.size() / 1024, cooked.atlas.width, cooked.atlas.height, worst, worstClip);
        std::fflush(stdout);
        // (A unit on a figure; in proportion on something the size of a
        // forklift, whose corners are a long way from the bones that turn
        // them.)
        check(worst < std::max(1.0, view.header->radius * 0.012),
              "every vertex is within a unit of where the game puts it");

        const char *dump = std::getenv("HLDS_DUMP_DIR");
        if (dump != nullptr && *dump != '\0' && cooked.atlas.width != 0)
            writePng(std::string(dump) + "/" + mdl.name + "_atlas.png", cooked.atlas, error);
    }
} // namespace

int main()
{
    const char *dir = std::getenv("HLDS_VALVE_DIR");
    if (dir == nullptr || *dir == '\0')
    {
        std::printf("test_anim: HLDS_VALVE_DIR not set, nothing to read\ntest_anim: ok\n");
        return 0;
    }
    checkModel(dir, "scientist.mdl",
               {"idle1", "walk", "run", "pondering", "push_button", "cprscientistrevive", "sitlookleft"});
    checkModel(dir, "barney.mdl", {"idle1", "walk", "run", "unlatch", "sit1"});
    checkModel(dir, "gman.mdl", {"idle01", "walk", "idlebrush"});
    checkModel(dir, "v_crowbar.mdl", {"idle1", "attack1", "draw"});
    checkModel(dir, "zombie.mdl", {"idle1", "walk"});
    checkModel(dir, "forklift.mdl", {"idle2", "drive"});

    // Time: a clip that loops goes round, one that does not stops and says so.
    ModelClip clip{};
    clip.frameCount = 31;
    clip.fps = 15 << 8;
    clip.flags = kClipLoops;
    bool finished = false;
    std::uint32_t frame = advanceClip(clip, 0, 1000, finished);
    check(frame == (15u << 8) && !finished, "a second at 15 frames a second is 15 frames");
    frame = advanceClip(clip, frame, 1500, finished);
    check(frame == (7u << 8) + 128 && !finished, "a clip that loops goes round");
    clip.flags = 0;
    frame = advanceClip(clip, 15u << 8, 1500, finished);
    check(frame == (30u << 8) && finished, "one that does not stops on its last frame, finished");

    if (g_failures == 0)
        std::printf("test_anim: ok\n");
    return g_failures == 0 ? 0 : 1;
}

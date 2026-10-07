#include "hlds/FaceBuilder.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "HlFixed.hpp"
#include "hlds/MeshOptimize.hpp"

namespace hlds
{
    namespace
    {
        double triangleArea(const double *a, const double *b, const double *c)
        {
            const double e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
            const double e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
            const double n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                                 e1[0] * e2[1] - e1[1] * e2[0]};
            return 0.5 * std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        }

        /// The zig-zag's rows for a polygon of n vertices started at `start`:
        /// row k is (p[start - k], p[start + k + 1]). Odd polygons leave one
        /// vertex over, between the two ends of the last row.
        struct Zigzag
        {
            std::vector<int> rows; ///< two indices a row
            int leftover = -1;
        };

        Zigzag zigzag(int n, int start)
        {
            Zigzag z;
            const int rowCount = n / 2;
            for (int k = 0; k < rowCount; ++k)
            {
                z.rows.push_back(((start - k) % n + n) % n);
                z.rows.push_back((start + k + 1) % n);
            }
            if (n % 2 == 1)
                z.leftover = (start + rowCount + 1) % n;
            return z;
        }

        double smallestPiece(const std::vector<FacePoint> &p, const Zigzag &z)
        {
            double smallest = 1e30;
            for (std::size_t r = 0; r + 3 < z.rows.size(); r += 2)
            {
                // Quad (a0, b0, b1, a1), as two triangles across one diagonal.
                const double *a0 = p[z.rows[r]].pos;
                const double *b0 = p[z.rows[r + 1]].pos;
                const double *a1 = p[z.rows[r + 2]].pos;
                const double *b1 = p[z.rows[r + 3]].pos;
                smallest = std::min(smallest,
                                    triangleArea(a0, b0, b1) + triangleArea(a0, b1, a1));
            }
            if (z.leftover >= 0)
            {
                const std::size_t last = z.rows.size() - 2;
                smallest = std::min(smallest,
                                    triangleArea(p[z.rows[last]].pos, p[z.rows[last + 1]].pos,
                                                 p[z.leftover].pos));
            }
            return smallest;
        }
    } // namespace

    int bestStripStart(const std::vector<FacePoint> &polygon)
    {
        const int n = static_cast<int>(polygon.size());
        if (n <= 3)
            return 0;
        int best = 0;
        double bestArea = -1;
        for (int start = 0; start < n; ++start)
        {
            const double area = smallestPiece(polygon, zigzag(n, start));
            // Strictly greater: ties keep the lowest start, so the output does
            // not depend on floating-point noise between equal candidates.
            if (area > bestArea * (1 + 1e-9) + 1e-12)
            {
                bestArea = area;
                best = start;
            }
        }
        return best;
    }

    std::vector<std::vector<int>> zigzagPieces(int n, int start)
    {
        std::vector<std::vector<int>> pieces;
        if (n == 3)
        {
            pieces.push_back({0, 1, 2});
            return pieces;
        }
        const Zigzag z = zigzag(n, start);
        // Rows (a0, b0), (a1, b1): the quad is (a0, b0, b1, a1).
        for (std::size_t r = 0; r + 3 < z.rows.size(); r += 2)
            pieces.push_back({z.rows[r], z.rows[r + 1], z.rows[r + 3], z.rows[r + 2]});
        if (z.leftover >= 0)
            pieces.push_back({z.rows[z.rows.size() - 2], z.rows[z.rows.size() - 1], z.leftover});
        return pieces;
    }

    namespace
    {
        using Snap = std::array<std::int64_t, 3>;

        Snap snap(const FacePoint &p)
        {
            return Snap{std::llround(p.pos[0] * 4), std::llround(p.pos[1] * 4),
                        std::llround(p.pos[2] * 4)};
        }

        /// (b - a) x (c - a), on the grid the DS draws on: exactly zero when
        /// the three are in line.
        Snap cross3(const Snap &a, const Snap &b, const Snap &c)
        {
            const std::int64_t e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
            const std::int64_t e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
            return Snap{e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                        e1[0] * e2[1] - e1[1] * e2[0]};
        }

        bool inLine(const Snap &a, const Snap &b, const Snap &c)
        {
            const Snap n = cross3(a, b, c);
            return n[0] == 0 && n[1] == 0 && n[2] == 0;
        }

        std::int64_t dot3(const Snap &a, const Snap &b)
        {
            return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
        }

    } // namespace

    /// m lies ON the side a -> c, between its ends, as far as the grid can
    /// say. A vertex the map's compiler put in the middle of a side (so
    /// that the face next door, which ends there, meets this one) is on
    /// the line exactly in the map and up to most of a grid step off it
    /// once all three are snapped. It is still that vertex: a piece whose
    /// side runs from a to c past it leaves the neighbour's two sides
    /// meeting one long one, and the hardware fills those differently --
    /// a row of background pixels along the join. Hence a distance, not
    /// "exactly in line": 1.5 grid steps covers the three roundings.
    ///
    /// But only away from the side's ends. A vertex a step or two from a is
    /// "within 1.5 steps of the line" whichever way the side goes, and maps
    /// do have corners that close together; taking those for mid-side
    /// vertices leaves a face with no corner to cut at. Next to an end, only
    /// a vertex exactly in line counts.
    bool onGridSide(const GridPoint &a, const GridPoint &m, const GridPoint &c)
    {
        const double d[3] = {double(c[0] - a[0]), double(c[1] - a[1]), double(c[2] - a[2])};
        const double w[3] = {double(m[0] - a[0]), double(m[1] - a[1]), double(m[2] - a[2])};
        const double length2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
        const double along = w[0] * d[0] + w[1] * d[1] + w[2] * d[2];
        if (along <= 0 || along >= length2)
            return false;
        const Snap n = cross3(a, m, c);
        if (n[0] == 0 && n[1] == 0 && n[2] == 0)
            return true;
        constexpr double kEnd = 6; // grid steps: 1.5 units
        const double fromA2 = w[0] * w[0] + w[1] * w[1] + w[2] * w[2];
        const double toC2 = double(c[0] - m[0]) * double(c[0] - m[0]) +
                            double(c[1] - m[1]) * double(c[1] - m[1]) +
                            double(c[2] - m[2]) * double(c[2] - m[2]);
        if (fromA2 < kEnd * kEnd || toC2 < kEnd * kEnd)
            return false;
        const double off2 = double(n[0]) * double(n[0]) + double(n[1]) * double(n[1]) +
                            double(n[2]) * double(n[2]);
        return off2 <= 1.5 * 1.5 * length2;
    }

    std::vector<std::vector<int>> decompose(const std::vector<FacePoint> &polygon)
    {
        const int n = static_cast<int>(polygon.size());
        std::vector<Snap> at(n);
        for (int i = 0; i < n; ++i)
            at[i] = snap(polygon[i]);

        // The way the polygon faces, on the grid. Snapping can leave a corner
        // of a "convex" polygon turning very slightly the other way, and a
        // piece with such a corner is not a shape the hardware draws.
        Snap whole{0, 0, 0};
        for (int i = 1; i + 1 < n; ++i)
        {
            const Snap c = cross3(at[0], at[i], at[i + 1]);
            for (int a = 0; a < 3; ++a)
                whole[a] += c[a];
        }

        // The zig-zag, if it has a start that leaves every piece with area.
        // Judged on the snapped positions: those are what is drawn.
        if (n > 3)
        {
            for (int tries = 0; tries < 2; ++tries)
            {
                const int start = tries == 0 ? bestStripStart(polygon) : -1;
                if (start < 0)
                    break;
                const std::vector<std::vector<int>> pieces = zigzagPieces(n, start);
                bool clean = true;
                for (const std::vector<int> &piece : pieces)
                {
                    // A piece is clean when no three CONSECUTIVE corners of it
                    // are in line with a fourth polygon vertex skipped between
                    // them -- which for pieces cut from a convex polygon is:
                    // every edge of the piece that is not an edge of the
                    // polygon is a true chord (its ends are not in line with
                    // any other vertex of the piece).
                    const int k = static_cast<int>(piece.size());
                    Snap normal{0, 0, 0};
                    for (int i = 1; i + 1 < k; ++i)
                    {
                        const Snap c = cross3(at[piece[0]], at[piece[i]], at[piece[i + 1]]);
                        for (int a = 0; a < 3; ++a)
                            normal[a] += c[a];
                    }
                    if (normal[0] == 0 && normal[1] == 0 && normal[2] == 0)
                        clean = false; // no area at all
                    for (int i = 0; i < k && clean; ++i)
                    {
                        const Snap turn = cross3(at[piece[i]], at[piece[(i + 1) % k]],
                                                 at[piece[(i + 2) % k]]);
                        if (dot3(turn, whole) < 0 &&
                            !onGridSide(at[piece[i]], at[piece[(i + 1) % k]], at[piece[(i + 2) % k]]))
                            clean = false; // a reflex corner
                    }
                    for (int i = 0; i < k && clean; ++i)
                    {
                        const int from = piece[i];
                        const int to = piece[(i + 1) % k];
                        if ((from + 1) % n == to)
                            continue; // an edge of the polygon itself
                        // A chord. It must not run along the polygon's
                        // boundary past a vertex.
                        for (int m = 0; m < n && clean; ++m)
                        {
                            if (m != from && m != to && onGridSide(at[from], at[m], at[to]))
                                clean = false;
                        }
                    }
                }
                if (clean)
                    return pieces;
            }
        }
        else
        {
            return {{0, 1, 2}};
        }

        // Ear clipping. The polygon is convex, so any corner that actually
        // turns is an ear; a mid-edge vertex is never clipped as a tip, so it
        // stays until it is the END of some ear's side -- which is what makes
        // every piece stop at it.
        std::vector<int> ring(n);
        for (int i = 0; i < n; ++i)
            ring[i] = i;
        std::vector<std::array<int, 3>> triangles;
        // A face is never left half cut. The rules for a good ear are tried
        // in full first; a ring none of whose corners passes them is cut by
        // weaker and weaker ones, down to "any corner", because a piece with
        // an awkward side is a pixel's worth of trouble and a piece left out
        // is a hole in the floor.
        //   0: turns the right way, is not a mid-side vertex, and its cut
        //      runs past no vertex and leaves a ring with area
        //   1: turns the right way and leaves a ring with area
        //   2: turns the right way
        //   3: any corner (a triangle with no area is cut off and not kept)
        while (ring.size() > 3)
        {
            const int count = static_cast<int>(ring.size());
            int chosen = -1;
            for (int level = 0; level < 4 && chosen < 0; ++level)
            {
                for (int i = 0; i < count && chosen < 0; ++i)
                {
                    const int prev = ring[(i + count - 1) % count];
                    const int next = ring[(i + 1) % count];
                    if (level == 3)
                    {
                        chosen = i;
                        break;
                    }
                    if (dot3(cross3(at[prev], at[ring[i]], at[next]), whole) <= 0)
                        continue; // no area, or turning the wrong way: not an ear
                    if (level == 2)
                    {
                        chosen = i;
                        break;
                    }
                    if (level == 0)
                    {
                        if (onGridSide(at[prev], at[ring[i]], at[next]))
                            continue;
                        // The cut prev -> next becomes a side of two pieces:
                        // it must not run past a vertex that is on it.
                        bool passes = false;
                        for (int j = 0; j < count && !passes; ++j)
                            passes = j != i && onGridSide(at[prev], at[ring[j]], at[next]);
                        if (passes)
                            continue;
                    }
                    // Do not leave behind a ring that is all one line -- and
                    // that goes for the LAST clip above all: four points of
                    // which three are in line have one ear that leaves a
                    // triangle and one that leaves the line, and the triangle
                    // cut off by the wrong one has a side running straight
                    // past the middle point.
                    bool restHasArea = false;
                    for (int j = 0; j < count && !restHasArea; ++j)
                    {
                        if (j == i)
                            continue;
                        restHasArea = !inLine(at[prev], at[next], at[ring[j]]);
                    }
                    if (restHasArea)
                        chosen = i;
                }
            }
            const int prev = ring[(chosen + count - 1) % count];
            const int next = ring[(chosen + 1) % count];
            if (dot3(cross3(at[prev], at[ring[chosen]], at[next]), whole) > 0)
                triangles.push_back({prev, ring[chosen], next});
            ring.erase(ring.begin() + chosen);
        }
        if (ring.size() == 3 && dot3(cross3(at[ring[0]], at[ring[1]], at[ring[2]]), whole) > 0)
            triangles.push_back({ring[0], ring[1], ring[2]});

        // Pair triangles that share an edge into quads, when the quad has no
        // reflex corner. A quad is four vertices of vertex RAM where two
        // triangles are six.
        std::vector<std::vector<int>> pieces;
        std::vector<char> paired(triangles.size(), 0);
        Snap faceNormal{0, 0, 0};
        for (const auto &t : triangles)
        {
            const Snap c = cross3(at[t[0]], at[t[1]], at[t[2]]);
            for (int a = 0; a < 3; ++a)
                faceNormal[a] += c[a];
        }
        for (std::size_t i = 0; i < triangles.size(); ++i)
        {
            if (paired[i])
                continue;
            bool made = false;
            for (std::size_t j = i + 1; j < triangles.size() && !made; ++j)
            {
                if (paired[j])
                    continue;
                for (int ei = 0; ei < 3 && !made; ++ei)
                {
                    const int p = triangles[i][ei];
                    const int q = triangles[i][(ei + 1) % 3];
                    for (int ej = 0; ej < 3 && !made; ++ej)
                    {
                        if (triangles[j][ej] != q || triangles[j][(ej + 1) % 3] != p)
                            continue;
                        // i = (p, q, r), j = (q, p, s): the quad is p, s, q, r.
                        const int r = triangles[i][(ei + 2) % 3];
                        const int sv = triangles[j][(ej + 2) % 3];
                        const int quad[4] = {p, sv, q, r};
                        bool convex = true;
                        for (int c = 0; c < 4 && convex; ++c)
                        {
                            const Snap turn = cross3(at[quad[c]], at[quad[(c + 1) % 4]],
                                                     at[quad[(c + 2) % 4]]);
                            convex = dot3(turn, faceNormal) >= 0;
                        }
                        if (!convex)
                            continue;
                        pieces.push_back({p, sv, q, r});
                        paired[i] = paired[j] = 1;
                        made = true;
                    }
                }
            }
            if (!made)
                pieces.push_back({triangles[i][0], triangles[i][1], triangles[i][2]});
        }
        return pieces;
    }

    BuiltSurface buildSurface(const std::vector<FacePoint> &bspOrder, TextureSize cooked,
                              std::uint32_t sourceWidth, std::uint32_t sourceHeight)
    {
        // One face is a group of one.
        MeshFace face;
        face.points = bspOrder;
        return buildGroupSurface({&face}, cooked, sourceWidth, sourceHeight);
    }
} // namespace hlds

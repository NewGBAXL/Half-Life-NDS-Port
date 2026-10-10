#pragma once

// A structure's fields, each in turn -- for writing a game's state out and
// reading it back (HlSaveState.hpp) without a list of them kept by hand.
//
// C++ before its reflection can still take an aggregate apart in the order
// it is declared: `auto &[a, b, c] = s;`. It has to be told HOW MANY, and
// if it is told wrong it does not compile. So each structure that is saved
// says only its count of fields (HL_FIELDS), and adding a field to one
// without saying so is a build error, on the PC and the console alike --
// not a save that quietly leaves the new field out. What is done with each
// field is the caller's (`visit(field)`), by its type.
//
// (The numbered functions below are made by a script, not by hand: one for
// each count up to kFieldsMost.)

#include <cstddef>

namespace hldata
{
    /// How many fields a structure has: said for each by HL_FIELDS.
    template <class T>
    struct FieldCount;
    inline constexpr int kFieldsMost = 80;

#define HL_FIELDS(Type, count)                                                                                         \
    template <>                                                                                                        \
    struct FieldCount<Type>                                                                                            \
    {                                                                                                                  \
        static constexpr int value = count;                                                                            \
    }

    namespace detail
    {
        template <int N>
        struct EachField;
        template <>
        struct EachField<1>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0] = s;
                visit(f0);
            }
        };
        template <>
        struct EachField<2>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1] = s;
                visit(f0); visit(f1);
            }
        };
        template <>
        struct EachField<3>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2] = s;
                visit(f0); visit(f1); visit(f2);
            }
        };
        template <>
        struct EachField<4>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3] = s;
                visit(f0); visit(f1); visit(f2); visit(f3);
            }
        };
        template <>
        struct EachField<5>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4);
            }
        };
        template <>
        struct EachField<6>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5);
            }
        };
        template <>
        struct EachField<7>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6);
            }
        };
        template <>
        struct EachField<8>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7);
            }
        };
        template <>
        struct EachField<9>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8);
            }
        };
        template <>
        struct EachField<10>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9);
            }
        };
        template <>
        struct EachField<11>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10);
            }
        };
        template <>
        struct EachField<12>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11);
            }
        };
        template <>
        struct EachField<13>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12);
            }
        };
        template <>
        struct EachField<14>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13);
            }
        };
        template <>
        struct EachField<15>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14);
            }
        };
        template <>
        struct EachField<16>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15);
            }
        };
        template <>
        struct EachField<17>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16);
            }
        };
        template <>
        struct EachField<18>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17);
            }
        };
        template <>
        struct EachField<19>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18);
            }
        };
        template <>
        struct EachField<20>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19);
            }
        };
        template <>
        struct EachField<21>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20);
            }
        };
        template <>
        struct EachField<22>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21);
            }
        };
        template <>
        struct EachField<23>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22);
            }
        };
        template <>
        struct EachField<24>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23);
            }
        };
        template <>
        struct EachField<25>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24);
            }
        };
        template <>
        struct EachField<26>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25);
            }
        };
        template <>
        struct EachField<27>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26);
            }
        };
        template <>
        struct EachField<28>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27);
            }
        };
        template <>
        struct EachField<29>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28);
            }
        };
        template <>
        struct EachField<30>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29);
            }
        };
        template <>
        struct EachField<31>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30);
            }
        };
        template <>
        struct EachField<32>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31);
            }
        };
        template <>
        struct EachField<33>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32);
            }
        };
        template <>
        struct EachField<34>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33);
            }
        };
        template <>
        struct EachField<35>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34);
            }
        };
        template <>
        struct EachField<36>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35);
            }
        };
        template <>
        struct EachField<37>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36);
            }
        };
        template <>
        struct EachField<38>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37);
            }
        };
        template <>
        struct EachField<39>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38);
            }
        };
        template <>
        struct EachField<40>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39);
            }
        };
        template <>
        struct EachField<41>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40);
            }
        };
        template <>
        struct EachField<42>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41);
            }
        };
        template <>
        struct EachField<43>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42);
            }
        };
        template <>
        struct EachField<44>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43);
            }
        };
        template <>
        struct EachField<45>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44);
            }
        };
        template <>
        struct EachField<46>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45);
            }
        };
        template <>
        struct EachField<47>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46);
            }
        };
        template <>
        struct EachField<48>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47);
            }
        };
        template <>
        struct EachField<49>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48);
            }
        };
        template <>
        struct EachField<50>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49);
            }
        };
        template <>
        struct EachField<51>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50);
            }
        };
        template <>
        struct EachField<52>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51);
            }
        };
        template <>
        struct EachField<53>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52);
            }
        };
        template <>
        struct EachField<54>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53);
            }
        };
        template <>
        struct EachField<55>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54);
            }
        };
        template <>
        struct EachField<56>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55);
            }
        };
        template <>
        struct EachField<57>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56);
            }
        };
        template <>
        struct EachField<58>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57);
            }
        };
        template <>
        struct EachField<59>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58);
            }
        };
        template <>
        struct EachField<60>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58); visit(f59);
            }
        };
        template <>
        struct EachField<61>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58); visit(f59); visit(f60);
            }
        };
        template <>
        struct EachField<62>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58); visit(f59); visit(f60); visit(f61);
            }
        };
        template <>
        struct EachField<63>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58); visit(f59); visit(f60); visit(f61); visit(f62);
            }
        };
        template <>
        struct EachField<64>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58); visit(f59); visit(f60); visit(f61); visit(f62); visit(f63);
            }
        };
        template <>
        struct EachField<65>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63, f64] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58); visit(f59); visit(f60); visit(f61); visit(f62); visit(f63); visit(f64);
            }
        };
        template <>
        struct EachField<66>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63, f64, f65] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58); visit(f59); visit(f60); visit(f61); visit(f62); visit(f63); visit(f64); visit(f65);
            }
        };
        template <>
        struct EachField<67>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63, f64, f65, f66] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58); visit(f59); visit(f60); visit(f61); visit(f62); visit(f63); visit(f64); visit(f65); visit(f66);
            }
        };
        template <>
        struct EachField<68>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63, f64, f65, f66, f67] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58); visit(f59); visit(f60); visit(f61); visit(f62); visit(f63); visit(f64); visit(f65); visit(f66); visit(f67);
            }
        };
        template <>
        struct EachField<69>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63, f64, f65, f66, f67, f68] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58); visit(f59); visit(f60); visit(f61); visit(f62); visit(f63); visit(f64); visit(f65); visit(f66); visit(f67); visit(f68);
            }
        };
        template <>
        struct EachField<70>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63, f64, f65, f66, f67, f68, f69] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58); visit(f59); visit(f60); visit(f61); visit(f62); visit(f63); visit(f64); visit(f65); visit(f66); visit(f67); visit(f68); visit(f69);
            }
        };
        template <>
        struct EachField<71>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63, f64, f65, f66, f67, f68, f69, f70] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58); visit(f59); visit(f60); visit(f61); visit(f62); visit(f63); visit(f64); visit(f65); visit(f66); visit(f67); visit(f68); visit(f69); visit(f70);
            }
        };
        template <>
        struct EachField<72>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63, f64, f65, f66, f67, f68, f69, f70, f71] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58); visit(f59); visit(f60); visit(f61); visit(f62); visit(f63); visit(f64); visit(f65); visit(f66); visit(f67); visit(f68); visit(f69); visit(f70); visit(f71);
            }
        };
        template <>
        struct EachField<73>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63, f64, f65, f66, f67, f68, f69, f70, f71, f72] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58); visit(f59); visit(f60); visit(f61); visit(f62); visit(f63); visit(f64); visit(f65); visit(f66); visit(f67); visit(f68); visit(f69); visit(f70); visit(f71); visit(f72);
            }
        };
        template <>
        struct EachField<74>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63, f64, f65, f66, f67, f68, f69, f70, f71, f72, f73] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58); visit(f59); visit(f60); visit(f61); visit(f62); visit(f63); visit(f64); visit(f65); visit(f66); visit(f67); visit(f68); visit(f69); visit(f70); visit(f71); visit(f72); visit(f73);
            }
        };
        template <>
        struct EachField<75>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63, f64, f65, f66, f67, f68, f69, f70, f71, f72, f73, f74] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58); visit(f59); visit(f60); visit(f61); visit(f62); visit(f63); visit(f64); visit(f65); visit(f66); visit(f67); visit(f68); visit(f69); visit(f70); visit(f71); visit(f72); visit(f73); visit(f74);
            }
        };
        template <>
        struct EachField<76>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63, f64, f65, f66, f67, f68, f69, f70, f71, f72, f73, f74, f75] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58); visit(f59); visit(f60); visit(f61); visit(f62); visit(f63); visit(f64); visit(f65); visit(f66); visit(f67); visit(f68); visit(f69); visit(f70); visit(f71); visit(f72); visit(f73); visit(f74); visit(f75);
            }
        };
        template <>
        struct EachField<77>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63, f64, f65, f66, f67, f68, f69, f70, f71, f72, f73, f74, f75, f76] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58); visit(f59); visit(f60); visit(f61); visit(f62); visit(f63); visit(f64); visit(f65); visit(f66); visit(f67); visit(f68); visit(f69); visit(f70); visit(f71); visit(f72); visit(f73); visit(f74); visit(f75); visit(f76);
            }
        };
        template <>
        struct EachField<78>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63, f64, f65, f66, f67, f68, f69, f70, f71, f72, f73, f74, f75, f76, f77] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58); visit(f59); visit(f60); visit(f61); visit(f62); visit(f63); visit(f64); visit(f65); visit(f66); visit(f67); visit(f68); visit(f69); visit(f70); visit(f71); visit(f72); visit(f73); visit(f74); visit(f75); visit(f76); visit(f77);
            }
        };
        template <>
        struct EachField<79>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63, f64, f65, f66, f67, f68, f69, f70, f71, f72, f73, f74, f75, f76, f77, f78] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58); visit(f59); visit(f60); visit(f61); visit(f62); visit(f63); visit(f64); visit(f65); visit(f66); visit(f67); visit(f68); visit(f69); visit(f70); visit(f71); visit(f72); visit(f73); visit(f74); visit(f75); visit(f76); visit(f77); visit(f78);
            }
        };
        template <>
        struct EachField<80>
        {
            template <class T, class V>
            static void in(T &s, V &&visit)
            {
                auto &[f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41, f42, f43, f44, f45, f46, f47, f48, f49, f50, f51, f52, f53, f54, f55, f56, f57, f58, f59, f60, f61, f62, f63, f64, f65, f66, f67, f68, f69, f70, f71, f72, f73, f74, f75, f76, f77, f78, f79] = s;
                visit(f0); visit(f1); visit(f2); visit(f3); visit(f4); visit(f5); visit(f6); visit(f7); visit(f8); visit(f9); visit(f10); visit(f11); visit(f12); visit(f13); visit(f14); visit(f15); visit(f16); visit(f17); visit(f18); visit(f19); visit(f20); visit(f21); visit(f22); visit(f23); visit(f24); visit(f25); visit(f26); visit(f27); visit(f28); visit(f29); visit(f30); visit(f31); visit(f32); visit(f33); visit(f34); visit(f35); visit(f36); visit(f37); visit(f38); visit(f39); visit(f40); visit(f41); visit(f42); visit(f43); visit(f44); visit(f45); visit(f46); visit(f47); visit(f48); visit(f49); visit(f50); visit(f51); visit(f52); visit(f53); visit(f54); visit(f55); visit(f56); visit(f57); visit(f58); visit(f59); visit(f60); visit(f61); visit(f62); visit(f63); visit(f64); visit(f65); visit(f66); visit(f67); visit(f68); visit(f69); visit(f70); visit(f71); visit(f72); visit(f73); visit(f74); visit(f75); visit(f76); visit(f77); visit(f78); visit(f79);
            }
        };
    } // namespace detail

    /// `visit` called with each field of `s`, in the order they are declared.
    template <class T, class V>
    void eachField(T &s, V &&visit)
    {
        detail::EachField<FieldCount<T>::value>::in(s, visit);
    }
} // namespace hldata

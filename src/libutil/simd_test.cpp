/*
Copyright (c) 2014 Larry Gritz et al.
All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
* Neither the name of Sony Pictures Imageworks nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <sstream>

#include <OpenEXR/half.h>
#include <OpenEXR/ImathVec.h>
#include <OpenEXR/ImathMatrix.h>

#include <OpenImageIO/simd.h>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/unittest.h>
#include <OpenImageIO/typedesc.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>
#include <OpenImageIO/argparse.h>
#include <OpenImageIO/timer.h>
#include <OpenImageIO/ustring.h>
#include <OpenImageIO/fmath.h>



using namespace OIIO;

using namespace OIIO::simd;


static int iterations = 1000000;
static int ntrials = 5;
static bool verbose = false;
static Sysutil::Term term(std::cout);
float dummy_float[16];
float dummy_float2[16];
float dummy_int[16];



static void
getargs (int argc, char *argv[])
{
    bool help = false;
    ArgParse ap;
    ap.options ("simd_test\n"
                OIIO_INTRO_STRING "\n"
                "Usage:  simd_test [options]",
                // "%*", parse_files, "",
                "--help", &help, "Print help message",
                "-v", &verbose, "Verbose mode",
                "--iterations %d", &iterations,
                    ustring::format("Number of iterations (default: %d)", iterations).c_str(),
                "--trials %d", &ntrials, "Number of trials",
                NULL);
    if (ap.parse (argc, (const char**)argv) < 0) {
        std::cerr << ap.geterror() << std::endl;
        ap.usage ();
        exit (EXIT_FAILURE);
    }
    if (help) {
        ap.usage ();
        exit (EXIT_FAILURE);
    }
}



static void
category_heading (string_view name)
{
    std::cout << "\n" << term.ansi("bold,underscore,yellow", name) << "\n\n";
}



static void
test_heading (string_view name, string_view name2="")
{
    std::cout << term.ansi("bold") << name << ' ' << name2
              << term.ansi("normal") << "\n";
}



#define OIIO_CHECK_SIMD_EQUAL(x,y)                                      \
    (all ((x) == (y)) ? ((void)0)                                       \
         : ((std::cout << Sysutil::Term(std::cout).ansi("red,bold")     \
             << __FILE__ << ":" << __LINE__ << ":\n"                    \
             << "FAILED: " << Sysutil::Term(std::cout).ansi("normal")   \
             << #x << " == " << #y << "\n"                              \
             << "\tvalues were '" << (x) << "' and '" << (y) << "'\n"), \
            (void)++unit_test_failures))


#define OIIO_CHECK_SIMD_EQUAL_THRESH(x,y,eps)                           \
    (all (abs((x)-(y)) < (eps)) ? ((void)0)                             \
         : ((std::cout << Sysutil::Term(std::cout).ansi("red,bold")     \
             << __FILE__ << ":" << __LINE__ << ":\n"                    \
             << "FAILED: " << Sysutil::Term(std::cout).ansi("normal")   \
             << #x << " == " << #y << "\n"                              \
             << "\tvalues were '" << (x) << "' and '" << (y) << "'\n"), \
            (void)++unit_test_failures))



// What I really want to do is merge benchmark() and benchmark2() into
// one template using variadic arguments, like this:
//   template <typename FUNC, typename ...ARGS>
//   void benchmark (size_t work, string_view funcname, FUNC func, ARGS... args)
// But it seems that although this works for Clang, it does not for gcc 4.8
// (but does for 4.9). Some day I'll get back to this simplification, but
// for now, gcc 4.8 seems like an important barrier.


template <typename FUNC, typename T>
void benchmark (string_view funcname, FUNC func, T x,
                size_t work=0)
{
    if (!work)
        work = SimdElements<decltype(func(x))>::size;
    auto repeat_func = [&](){
        // Unroll the loop 8 times
        auto r = func(x); DoNotOptimize (r); clobber_all_memory();
        r = func(x); DoNotOptimize (r); clobber_all_memory();
        r = func(x); DoNotOptimize (r); clobber_all_memory();
        r = func(x); DoNotOptimize (r); clobber_all_memory();
        r = func(x); DoNotOptimize (r); clobber_all_memory();
        r = func(x); DoNotOptimize (r); clobber_all_memory();
        r = func(x); DoNotOptimize (r); clobber_all_memory();
        r = func(x); DoNotOptimize (r); clobber_all_memory();
    };
    float time = time_trial (repeat_func, ntrials, iterations/8);
    std::cout << Strutil::format ("  %s: %7.1f Mvals/sec, (%.1f Mcalls/sec)\n",
                                  funcname, ((iterations*work)/1.0e6)/time,
                                  (iterations/1.0e6)/time);
}


template <typename FUNC, typename T, typename U>
void benchmark2 (string_view funcname, FUNC func, T x, U y,
                 size_t work=0)
{
    if (!work)
        work = SimdElements<decltype(func(x,y))>::size;
    auto repeat_func = [&](){
        // Unroll the loop 8 times
        auto r = func(x, y); DoNotOptimize (r); clobber_all_memory();
        r = func(x, y); DoNotOptimize (r); clobber_all_memory();
        r = func(x, y); DoNotOptimize (r); clobber_all_memory();
        r = func(x, y); DoNotOptimize (r); clobber_all_memory();
        r = func(x, y); DoNotOptimize (r); clobber_all_memory();
        r = func(x, y); DoNotOptimize (r); clobber_all_memory();
        r = func(x, y); DoNotOptimize (r); clobber_all_memory();
        r = func(x, y); DoNotOptimize (r); clobber_all_memory();
    };
    float time = time_trial (repeat_func, ntrials, iterations/8);
    std::cout << Strutil::format ("  %s: %7.1f Mvals/sec, (%.1f Mcalls/sec)\n",
                                  funcname, ((iterations*work)/1.0e6)/time,
                                  (iterations/1.0e6)/time);
}



template<typename VEC>
inline VEC mkvec (typename VEC::value_t a, typename VEC::value_t b,
                  typename VEC::value_t c, typename VEC::value_t d=0)
{
    return VEC(a,b,c,d);
}

template<> inline float3 mkvec<float3> (float a, float b, float c, float d) {
    return float3(a,b,c);
}

template<> inline float8 mkvec<float8> (float a, float b, float c, float d) {
    return float8(a,b,c,d,a,b,c,d);
}

template<> inline float16 mkvec<float16> (float a, float b, float c, float d) {
    return float16(a,b,c,d,a,b,c,d,a,b,c,d,a,b,c,d);
}

template<> inline int8 mkvec<int8> (int a, int b, int c, int d) {
    return int8(a,b,c,d,a,b,c,d);
}

template<> inline bool8 mkvec<bool8> (bool a, bool b, bool c, bool d) {
    return bool8(a,b,c,d,a,b,c,d);
}

template<> inline bool16 mkvec<bool16> (bool a, bool b, bool c, bool d) {
    return bool16(a,b,c,d,a,b,c,d,a,b,c,d,a,b,c,d);
}



template<typename VEC>
inline VEC mkvec (typename VEC::value_t a, typename VEC::value_t b,
                  typename VEC::value_t c, typename VEC::value_t d,
                  typename VEC::value_t e, typename VEC::value_t f,
                  typename VEC::value_t g, typename VEC::value_t h)
{
    return VEC(a,b,c,d,e,f,g,h);
}


template<> inline bool4 mkvec<bool4> (bool a, bool b, bool c, bool d,
                                      bool e, bool f, bool g, bool h) {
    return bool4(a,b,c,d);
}

template<> inline int4 mkvec<int4> (int a, int b, int c, int d,
                                    int e, int f, int g, int h) {
    return int4(a,b,c,d);
}

template<> inline int16 mkvec<int16> (int a, int b, int c, int d,
                                      int e, int f, int g, int h) {
    return int16(a,b,c,d,e,f,g,h,
                 h+1,h+2,h+3,h+4,h+5,h+6,h+7,h+8);
}

template<> inline float4 mkvec<float4> (float a, float b, float c, float d,
                                        float e, float f, float g, float h) {
    return float4(a,b,c,d);
}

template<> inline float3 mkvec<float3> (float a, float b, float c, float d,
                                        float e, float f, float g, float h) {
    return float3(a,b,c);
}

template<> inline float16 mkvec<float16> (float a, float b, float c, float d,
                                          float e, float f, float g, float h) {
    return float16(a,b,c,d,e,f,g,h,
                   h+1,h+2,h+3,h+4,h+5,h+6,h+7,h+8);
}



template<typename VEC>
inline int loadstore_vec (int dummy) {
    typedef typename VEC::value_t ELEM;
    ELEM B[VEC::elements];
    VEC v;
    v.load ((ELEM *)dummy_float);
    v.store ((ELEM *)B);
    return 0;
}

template<typename VEC, int N>
inline int loadstore_vec_N (int dummy) {
    typedef typename VEC::value_t ELEM;
    ELEM B[VEC::elements];
    VEC v;
    v.load ((ELEM *)dummy_float, N);
    v.store ((ELEM *)B, N);
    return 0;
}


inline float dot_imath (const Imath::V3f &v) {
    return v.dot(v);
}
inline float dot_imath_simd (const Imath::V3f &v_) {
    float3 v (v_);
    return simd::dot(v,v);
}
inline float dot_simd (const simd::float3 v) {
    return dot(v,v);
}

inline Imath::V3f
norm_imath (const Imath::V3f &a) {
    return a.normalized();
}

inline Imath::V3f
norm_imath_simd (float3 a) {
    return a.normalized().V3f();
}

inline Imath::V3f
norm_imath_simd_fast (float3 a) {
    return a.normalized_fast().V3f();
}

inline float3
norm_simd_fast (float3 a) {
    return a.normalized_fast();
}

inline float3
norm_simd (float3 a) {
    return a.normalized();
}


inline Imath::M44f inverse_imath (const Imath::M44f &M)
{
    return M.inverse();
}


inline matrix44 inverse_simd (const matrix44 &M)
{
    return M.inverse();
}



template<typename VEC>
void test_partial_loadstore ()
{
    typedef typename VEC::value_t ELEM;
    test_heading ("partial loadstore ", VEC::type_name());
    VEC C1234 = VEC::Iota(1);
    ELEM partial[] = { 101, 102, 103, 104, 105, 106, 107, 108,
                       109, 110, 111, 112, 113, 114, 115, 116 };
    OIIO_CHECK_SIMD_EQUAL (VEC(partial), VEC::Iota(101));
    for (int i = 1; i <= VEC::elements; ++i) {
        VEC a (ELEM(0));
        a.load (partial, i);
        for (int j = 0; j < VEC::elements; ++j)
            OIIO_CHECK_EQUAL (a[j], j<i ? partial[j] : ELEM(0));
        std::cout << "  partial load " << i << " : " << a << "\n";
        ELEM stored[] = { 0, 0, 0, 0, 0, 0, 0, 0,
                          0, 0, 0, 0, 0, 0, 0, 0 };
        C1234.store (stored, i);
        for (int j = 0; j < VEC::elements; ++j)
            OIIO_CHECK_EQUAL (stored[j], j<i ? ELEM(j+1) : ELEM(0));
        std::cout << "  partial store " << i << " :";
        for (int c = 0; c < VEC::elements; ++c)
            std::cout << ' ' << stored[c];
        std::cout << std::endl;
    }

    benchmark ("load/store", loadstore_vec<VEC>, 0, VEC::elements);
    if (VEC::elements == 16) {
        benchmark ("load/store, 16 comps", loadstore_vec_N<VEC, 16>, 0, 16);
        benchmark ("load/store, 13 comps", loadstore_vec_N<VEC, 13>, 0, 13);
        benchmark ("load/store, 9 comps", loadstore_vec_N<VEC, 9>, 0, 9);
    }
    if (VEC::elements > 4) {
        benchmark ("load/store, 8 comps", loadstore_vec_N<VEC, 8>, 0, 8);
        benchmark ("load/store, 7 comps", loadstore_vec_N<VEC, 7>, 0, 7);
        benchmark ("load/store, 6 comps", loadstore_vec_N<VEC, 6>, 0, 6);
        benchmark ("load/store, 5 comps", loadstore_vec_N<VEC, 5>, 0, 5);
    }
    if (VEC::elements >= 4) {
        benchmark ("load/store, 4 comps", loadstore_vec_N<VEC, 4>, 0, 4);
    }
    benchmark ("load/store, 3 comps", loadstore_vec_N<VEC, 3>, 0, 3);
    benchmark ("load/store, 2 comps", loadstore_vec_N<VEC, 2>, 0, 2);
    benchmark ("load/store, 1 comps", loadstore_vec_N<VEC, 1>, 0, 1);
}



template<typename VEC>
void test_conversion_loadstore_float ()
{
    typedef typename VEC::value_t ELEM;
    test_heading ("loadstore with conversion", VEC::type_name());
    VEC C1234 = VEC::Iota(1);
    ELEM partial[] = { 101, 102, 103, 104, 105, 106, 107, 108,
                       109, 110, 111, 112, 113, 114, 115, 116 };
    OIIO_CHECK_SIMD_EQUAL (VEC(partial), VEC::Iota(101));

    // Check load from integers
    unsigned short us1234[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    short s1234[]           = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    unsigned char uc1234[]  = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    char c1234[]            = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    half h1234[]            = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    OIIO_CHECK_SIMD_EQUAL (VEC(us1234), C1234);
    OIIO_CHECK_SIMD_EQUAL (VEC( s1234), C1234);
    OIIO_CHECK_SIMD_EQUAL (VEC(uc1234), C1234);
    OIIO_CHECK_SIMD_EQUAL (VEC( c1234), C1234);

    benchmark ("load from unsigned short[]", [](const unsigned short *d){ return VEC(d); }, us1234);
    benchmark ("load from short[]", [](const short *d){ return VEC(d); }, s1234);
    benchmark ("load from unsigned char[]", [](const unsigned char *d){ return VEC(d); }, uc1234);
    benchmark ("load from char[]", [](const char *d){ return VEC(d); }, c1234);
    benchmark ("load from half[]", [](const half *d){ return VEC(d); }, h1234);
}



template<typename VEC>
void test_conversion_loadstore_int ()
{
    typedef typename VEC::value_t ELEM;
    test_heading ("loadstore with conversion", VEC::type_name());
    VEC C1234 = VEC::Iota(1);
    ELEM partial[] = { 101, 102, 103, 104, 105, 106, 107, 108,
                       109, 110, 111, 112, 113, 114, 115, 116 };
    OIIO_CHECK_SIMD_EQUAL (VEC(partial), VEC::Iota(101));

    // Check load from integers
    int i1234[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    unsigned short us1234[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    short s1234[]           = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    unsigned char uc1234[]  = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    char c1234[]            = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    OIIO_CHECK_SIMD_EQUAL (VEC( i1234), C1234);
    OIIO_CHECK_SIMD_EQUAL (VEC(us1234), C1234);
    OIIO_CHECK_SIMD_EQUAL (VEC( s1234), C1234);
    OIIO_CHECK_SIMD_EQUAL (VEC(uc1234), C1234);
    OIIO_CHECK_SIMD_EQUAL (VEC( c1234), C1234);

    benchmark ("load from int[]", [](const int *d){ return VEC(d); }, i1234);
    benchmark ("load from unsigned short[]", [](const unsigned short *d){ return VEC(d); }, us1234);
    benchmark ("load from short[]", [](const short *d){ return VEC(d); }, s1234);
    benchmark ("load from unsigned char[]", [](const unsigned char *d){ return VEC(d); }, uc1234);
    benchmark ("load from char[]", [](const char *d){ return VEC(d); }, c1234);
}



template<typename VEC>
void test_vint_to_uint16s ()
{
    test_heading (Strutil::format("test converting %s to uint16", VEC::type_name()));
    VEC ival = VEC::Iota (0xffff0000);
    unsigned short buf[VEC::elements];
    ival.store (buf);
    for (int i = 0; i < VEC::elements; ++i)
        OIIO_CHECK_EQUAL (int(buf[i]), i);

    benchmark2 ("load from uint16", [](VEC& a, unsigned short *s){ a.load(s); return 1; }, ival, buf, VEC::elements);
    benchmark2 ("convert to uint16", [](VEC& a, unsigned short *s){ a.store(s); return 1; }, ival, buf, VEC::elements);
}



template<typename VEC>
void test_vint_to_uint8s ()
{
    test_heading (Strutil::format("test converting %s to uint8", VEC::type_name()));
    VEC ival = VEC::Iota (0xffffff00);
    unsigned char buf[VEC::elements];
    ival.store (buf);
    for (int i = 0; i < VEC::elements; ++i)
        OIIO_CHECK_EQUAL (int(buf[i]), i);

    benchmark2 ("load from uint8", [](VEC& a, unsigned char *s){ a.load(s); return 1; }, ival, buf, VEC::elements);
    benchmark2 ("convert to uint16", [](VEC& a, unsigned char *s){ a.store(s); return 1; }, ival, buf, VEC::elements);
}



template<typename VEC>
void test_masked_loadstore ()
{
    typedef typename VEC::value_t ELEM;
    typedef typename VEC::bool_t BOOL;
    test_heading ("masked loadstore ", VEC::type_name());
    ELEM iota[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    BOOL mask1 = mkvec<BOOL> (true, false, true, false);
    BOOL mask2 = mkvec<BOOL> (true, true, false,false);

    VEC v;
    v = -1;
    v.load_mask (mask1, iota);
    ELEM r1[] = { 1, 0, 3, 0, 5, 0, 7, 0, 9, 0, 11, 0, 13, 0, 15, 0 };
    OIIO_CHECK_SIMD_EQUAL (v, VEC(r1));
    ELEM buf[] = { -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2 };
    v.store_mask (mask2, buf);
    ELEM r2[] = { 1, 0, -2, -2, 5, 0, -2, -2, 9, 0, -2, -2, 13, 0, -2, -2 };
    OIIO_CHECK_SIMD_EQUAL (VEC(buf), VEC(r2));

    benchmark ("masked load with int mask", [](const ELEM *d){ VEC v; v.load_mask (0xffff, d); return v; }, iota);
    benchmark ("masked load with bool mask", [](const ELEM *d){ VEC v; v.load_mask (BOOL::True(), d); return v; }, iota);
    benchmark ("masked store with int mask", [&](ELEM *d){ v.store_mask (0xffff, d); return 0; }, r2);
    benchmark ("masked store with bool mask", [&](ELEM *d){ v.store_mask (BOOL::True(), d); return 0; }, r2);
}



template<typename VEC>
void test_component_access ()
{
    typedef typename VEC::value_t ELEM;
    test_heading ("component_access ", VEC::type_name());

    const ELEM vals[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    VEC a = VEC::Iota();
    for (int i = 0; i < VEC::elements; ++i)
        OIIO_CHECK_EQUAL (a[i], vals[i]);

    if (VEC::elements <= 4) {
        OIIO_CHECK_EQUAL (a.x(), 0);
        OIIO_CHECK_EQUAL (a.y(), 1);
        OIIO_CHECK_EQUAL (a.z(), 2);
        if (SimdElements<VEC>::size > 3)
            OIIO_CHECK_EQUAL (a.w(), 3);
        VEC t;
        t = a; t.set_x(42); OIIO_CHECK_SIMD_EQUAL (t, mkvec<VEC>(42,1,2,3,4,5,6,7));
        t = a; t.set_y(42); OIIO_CHECK_SIMD_EQUAL (t, mkvec<VEC>(0,42,2,3,4,5,6,7));
        t = a; t.set_z(42); OIIO_CHECK_SIMD_EQUAL (t, mkvec<VEC>(0,1,42,3,4,5,6,7));
        if (SimdElements<VEC>::size > 3) {
            t = a; t.set_w(42); OIIO_CHECK_SIMD_EQUAL (t, mkvec<VEC>(0,1,2,42,4,5,6,7));
        }
    }

    OIIO_CHECK_EQUAL (extract<0>(a), 0);
    OIIO_CHECK_EQUAL (extract<1>(a), 1);
    OIIO_CHECK_EQUAL (extract<2>(a), 2);
    if (SimdElements<VEC>::size > 3)
        OIIO_CHECK_EQUAL (extract<3>(a), 3);
    OIIO_CHECK_SIMD_EQUAL (insert<0>(a, ELEM(42)), mkvec<VEC>(42,1,2,3,4,5,6,7));
    OIIO_CHECK_SIMD_EQUAL (insert<1>(a, ELEM(42)), mkvec<VEC>(0,42,2,3,4,5,6,7));
    OIIO_CHECK_SIMD_EQUAL (insert<2>(a, ELEM(42)), mkvec<VEC>(0,1,42,3,4,5,6,7));
    if (SimdElements<VEC>::size > 3)
        OIIO_CHECK_SIMD_EQUAL (insert<3>(a, ELEM(42)), mkvec<VEC>(0,1,2,42,4,5,6,7));

    VEC b (vals);
    for (int i = 0; i < VEC::elements; ++i)
        OIIO_CHECK_EQUAL (b[i], vals[i]);
    OIIO_CHECK_EQUAL (extract<0>(b), 0);
    OIIO_CHECK_EQUAL (extract<1>(b), 1);
    OIIO_CHECK_EQUAL (extract<2>(b), 2);
    if (SimdElements<VEC>::size > 3)
        OIIO_CHECK_EQUAL (extract<3>(b), 3);
    if (SimdElements<VEC>::size > 4) {
        OIIO_CHECK_EQUAL (extract<4>(b), 4);
        OIIO_CHECK_EQUAL (extract<5>(b), 5);
        OIIO_CHECK_EQUAL (extract<6>(b), 6);
        OIIO_CHECK_EQUAL (extract<7>(b), 7);
    }
    if (SimdElements<VEC>::size > 8) {
        OIIO_CHECK_EQUAL (extract<8>(b), 8);
        OIIO_CHECK_EQUAL (extract<9>(b), 9);
        OIIO_CHECK_EQUAL (extract<10>(b), 10);
        OIIO_CHECK_EQUAL (extract<11>(b), 11);
        OIIO_CHECK_EQUAL (extract<12>(b), 12);
        OIIO_CHECK_EQUAL (extract<13>(b), 13);
        OIIO_CHECK_EQUAL (extract<14>(b), 14);
        OIIO_CHECK_EQUAL (extract<15>(b), 15);
    }

    benchmark2 ("operator[i]", [&](const VEC& v, int i){ return v[i]; },  b, 2, 1 /*work*/);
    benchmark2 ("operator[2]", [&](const VEC& v, int i){ return v[2]; },  b, 2, 1 /*work*/);
    benchmark2 ("operator[0]", [&](const VEC& v, int i){ return v[0]; },  b, 0, 1 /*work*/);
    benchmark2 ("extract<2> ", [&](const VEC& v, int i){ return extract<2>(v); },  b, 2, 1 /*work*/);
    benchmark2 ("extract<0> ", [&](const VEC& v, int i){ return extract<0>(v); },  b, 0, 1 /*work*/);
    benchmark2 ("insert<2> ", [&](const VEC& v, ELEM i){ return insert<2>(v, i); }, b, ELEM(1), 1 /*work*/);
}



template<>
void test_component_access<bool4> ()
{
    typedef bool4 VEC;
    typedef VEC::value_t ELEM;
    test_heading ("component_access ", VEC::type_name());

    for (int bit = 0; bit < VEC::elements; ++bit) {
        VEC ctr (bit == 0, bit == 1, bit == 2, bit == 3);
        VEC a;
        a.clear ();
        for (int b = 0; b < VEC::elements; ++b)
            a.setcomp (b, b==bit);
        OIIO_CHECK_SIMD_EQUAL (ctr, a);
        for (int b = 0; b < VEC::elements; ++b)
            OIIO_CHECK_EQUAL (bool(a[b]), b == bit);
        OIIO_CHECK_EQUAL (extract<0>(a), bit == 0);
        OIIO_CHECK_EQUAL (extract<1>(a), bit == 1);
        OIIO_CHECK_EQUAL (extract<2>(a), bit == 2);
        OIIO_CHECK_EQUAL (extract<3>(a), bit == 3);
    }

    VEC a;
    a.load (0,0,0,0);
    OIIO_CHECK_SIMD_EQUAL (insert<0>(a, 1), VEC(1,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<1>(a, 1), VEC(0,1,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<2>(a, 1), VEC(0,0,1,0));
    OIIO_CHECK_SIMD_EQUAL (insert<3>(a, 1), VEC(0,0,0,1));
    a.load (1,1,1,1);
    OIIO_CHECK_SIMD_EQUAL (insert<0>(a, 0), VEC(0,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<1>(a, 0), VEC(1,0,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<2>(a, 0), VEC(1,1,0,1));
    OIIO_CHECK_SIMD_EQUAL (insert<3>(a, 0), VEC(1,1,1,0));
}



template<>
void test_component_access<bool8> ()
{
    typedef bool8 VEC;
    typedef VEC::value_t ELEM;
    test_heading ("component_access ", VEC::type_name());

    for (int bit = 0; bit < VEC::elements; ++bit) {
        VEC ctr (bit == 0, bit == 1, bit == 2, bit == 3,
                 bit == 4, bit == 5, bit == 6, bit == 7);
        VEC a;
        a.clear ();
        for (int b = 0; b < VEC::elements; ++b)
            a.setcomp (b, b==bit);
        OIIO_CHECK_SIMD_EQUAL (ctr, a);
        for (int b = 0; b < VEC::elements; ++b)
            OIIO_CHECK_EQUAL (bool(a[b]), b == bit);
        OIIO_CHECK_EQUAL (extract<0>(a), bit == 0);
        OIIO_CHECK_EQUAL (extract<1>(a), bit == 1);
        OIIO_CHECK_EQUAL (extract<2>(a), bit == 2);
        OIIO_CHECK_EQUAL (extract<3>(a), bit == 3);
        OIIO_CHECK_EQUAL (extract<4>(a), bit == 4);
        OIIO_CHECK_EQUAL (extract<5>(a), bit == 5);
        OIIO_CHECK_EQUAL (extract<6>(a), bit == 6);
        OIIO_CHECK_EQUAL (extract<7>(a), bit == 7);
    }

    VEC a;
    a.load (0,0,0,0,0,0,0,0);
    OIIO_CHECK_SIMD_EQUAL (insert<0>(a, 1), VEC(1,0,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<1>(a, 1), VEC(0,1,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<2>(a, 1), VEC(0,0,1,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<3>(a, 1), VEC(0,0,0,1,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<4>(a, 1), VEC(0,0,0,0,1,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<5>(a, 1), VEC(0,0,0,0,0,1,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<6>(a, 1), VEC(0,0,0,0,0,0,1,0));
    OIIO_CHECK_SIMD_EQUAL (insert<7>(a, 1), VEC(0,0,0,0,0,0,0,1));
    a.load (1,1,1,1,1,1,1,1);
    OIIO_CHECK_SIMD_EQUAL (insert<0>(a, 0), VEC(0,1,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<1>(a, 0), VEC(1,0,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<2>(a, 0), VEC(1,1,0,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<3>(a, 0), VEC(1,1,1,0,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<4>(a, 0), VEC(1,1,1,1,0,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<5>(a, 0), VEC(1,1,1,1,1,0,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<6>(a, 0), VEC(1,1,1,1,1,1,0,1));
    OIIO_CHECK_SIMD_EQUAL (insert<7>(a, 0), VEC(1,1,1,1,1,1,1,0));
}



template<>
void test_component_access<bool16> ()
{
    typedef bool16 VEC;
    typedef VEC::value_t ELEM;
    test_heading ("component_access ", VEC::type_name());

    for (int bit = 0; bit < VEC::elements; ++bit) {
        VEC ctr (bit == 0, bit == 1, bit == 2, bit == 3,
                 bit == 4, bit == 5, bit == 6, bit == 7,
                 bit == 8, bit == 9, bit == 10, bit == 11,
                 bit == 12, bit == 13, bit == 14, bit == 15);
        VEC a;
        a.clear ();
        for (int b = 0; b < VEC::elements; ++b)
            a.setcomp (b, b==bit);
        OIIO_CHECK_SIMD_EQUAL (ctr, a);
        for (int b = 0; b < VEC::elements; ++b)
            OIIO_CHECK_EQUAL (bool(a[b]), b == bit);
        OIIO_CHECK_EQUAL (extract<0>(a), bit == 0);
        OIIO_CHECK_EQUAL (extract<1>(a), bit == 1);
        OIIO_CHECK_EQUAL (extract<2>(a), bit == 2);
        OIIO_CHECK_EQUAL (extract<3>(a), bit == 3);
        OIIO_CHECK_EQUAL (extract<4>(a), bit == 4);
        OIIO_CHECK_EQUAL (extract<5>(a), bit == 5);
        OIIO_CHECK_EQUAL (extract<6>(a), bit == 6);
        OIIO_CHECK_EQUAL (extract<7>(a), bit == 7);
        OIIO_CHECK_EQUAL (extract<8>(a), bit == 8);
        OIIO_CHECK_EQUAL (extract<9>(a), bit == 9);
        OIIO_CHECK_EQUAL (extract<10>(a), bit == 10);
        OIIO_CHECK_EQUAL (extract<11>(a), bit == 11);
        OIIO_CHECK_EQUAL (extract<12>(a), bit == 12);
        OIIO_CHECK_EQUAL (extract<13>(a), bit == 13);
        OIIO_CHECK_EQUAL (extract<14>(a), bit == 14);
        OIIO_CHECK_EQUAL (extract<15>(a), bit == 15);
    }

    VEC a;
    a.load (0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
    OIIO_CHECK_SIMD_EQUAL (insert<0> (a, 1), VEC(1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<1> (a, 1), VEC(0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<2> (a, 1), VEC(0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<3> (a, 1), VEC(0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<4> (a, 1), VEC(0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<5> (a, 1), VEC(0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<6> (a, 1), VEC(0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<7> (a, 1), VEC(0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<8> (a, 1), VEC(0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<9> (a, 1), VEC(0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<10>(a, 1), VEC(0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<11>(a, 1), VEC(0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<12>(a, 1), VEC(0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<13>(a, 1), VEC(0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0));
    OIIO_CHECK_SIMD_EQUAL (insert<14>(a, 1), VEC(0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0));
    OIIO_CHECK_SIMD_EQUAL (insert<15>(a, 1), VEC(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1));
    a.load (1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1);
    OIIO_CHECK_SIMD_EQUAL (insert<0> (a, 0), VEC(0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<1> (a, 0), VEC(1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<2> (a, 0), VEC(1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<3> (a, 0), VEC(1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<4> (a, 0), VEC(1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<5> (a, 0), VEC(1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<6> (a, 0), VEC(1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<7> (a, 0), VEC(1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<8> (a, 0), VEC(1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<9> (a, 0), VEC(1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<10>(a, 0), VEC(1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<11>(a, 0), VEC(1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<12>(a, 0), VEC(1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<13>(a, 0), VEC(1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1));
    OIIO_CHECK_SIMD_EQUAL (insert<14>(a, 0), VEC(1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1));
    OIIO_CHECK_SIMD_EQUAL (insert<15>(a, 0), VEC(1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0));
}



template<typename T> inline T do_add (const T &a, const T &b) { return a+b; }
template<typename T> inline T do_sub (const T &a, const T &b) { return a-b; }
template<typename T> inline T do_mul (const T &a, const T &b) { return a*b; }
template<typename T> inline T do_div (const T &a, const T &b) { return a/b; }
template<typename T> inline T do_safe_div (const T &a, const T &b) { return T(safe_div(a,b)); }
inline Imath::V3f add_vec_simd (const Imath::V3f &a, const Imath::V3f &b) {
    return (float3(a)+float3(b)).V3f();
}


template<typename VEC>
void test_arithmetic ()
{
    typedef typename VEC::value_t ELEM;
    test_heading ("arithmetic ", VEC::type_name());

    VEC a = VEC::Iota (1, 3);
    VEC b = VEC::Iota (1, 1);
    VEC add(ELEM(0)), sub(ELEM(0)), mul(ELEM(0)), div(ELEM(0));
    ELEM bsum(ELEM(0));
    for (int i = 0; i < VEC::elements; ++i) {
        add[i] = a[i] + b[i];
        sub[i] = a[i] - b[i];
        mul[i] = a[i] * b[i];
        div[i] = a[i] / b[i];
        bsum += b[i];
    }
    OIIO_CHECK_SIMD_EQUAL (a+b, add);
    OIIO_CHECK_SIMD_EQUAL (a-b, sub);
    OIIO_CHECK_SIMD_EQUAL (a*b, mul);
    OIIO_CHECK_SIMD_EQUAL (a/b, div);
    OIIO_CHECK_SIMD_EQUAL (a*ELEM(2), a*VEC(ELEM(2)));
    { VEC r = a; r += b; OIIO_CHECK_SIMD_EQUAL (r, add); }
    { VEC r = a; r -= b; OIIO_CHECK_SIMD_EQUAL (r, sub); }
    { VEC r = a; r *= b; OIIO_CHECK_SIMD_EQUAL (r, mul); }
    { VEC r = a; r /= b; OIIO_CHECK_SIMD_EQUAL (r, div); }
    { VEC r = a; r *= ELEM(2); OIIO_CHECK_SIMD_EQUAL (r, a*ELEM(2)); }

    OIIO_CHECK_EQUAL (reduce_add(b), bsum);
    OIIO_CHECK_SIMD_EQUAL (vreduce_add(b), VEC(bsum));
    OIIO_CHECK_EQUAL (reduce_add(VEC(1.0f)), SimdElements<VEC>::size);

    benchmark2 ("operator+", do_add<VEC>, a, b);
    benchmark2 ("operator-", do_sub<VEC>, a, b);
    benchmark2 ("operator*", do_mul<VEC>, a, b);
    benchmark2 ("operator/", do_div<VEC>, a, b);
    benchmark  ("reduce_add", [](VEC& a){ return vreduce_add(a); }, a);
    if (is_same<VEC,float3>::value) {  // For float3, compare to Imath
        Imath::V3f a(2.51f,1.0f,1.0f), b(3.1f,1.0f,1.0f);
        benchmark2 ("add Imath::V3f", do_add<Imath::V3f>, a, b, 3 /*work*/);
        benchmark2 ("add Imath::V3f with simd", add_vec_simd, a, b, 3 /*work*/);
        benchmark2 ("sub Imath::V3f", do_sub<Imath::V3f>, a, b, 3 /*work*/);
        benchmark2 ("mul Imath::V3f", do_mul<Imath::V3f>, a, b, 3 /*work*/);
        benchmark2 ("div Imath::V3f", do_div<Imath::V3f>, a, b, 3 /*work*/);
    }
    benchmark2 ("reference: add scalar", do_add<ELEM>, a[2], b[1]);
    benchmark2 ("reference: mul scalar", do_mul<ELEM>, a[2], b[1]);
    benchmark2 ("reference: div scalar", do_div<ELEM>, a[2], b[1]);
}



template<typename VEC>
void test_fused ()
{
    test_heading ("fused ", VEC::type_name());

    VEC a = VEC::Iota (10);
    VEC b = VEC::Iota (1);
    VEC c = VEC::Iota (0.5f);
    OIIO_CHECK_SIMD_EQUAL (madd (a, b, c), a*b+c);
    OIIO_CHECK_SIMD_EQUAL (msub (a, b, c), a*b-c);
    OIIO_CHECK_SIMD_EQUAL (nmadd (a, b, c), -(a*b)+c);
    OIIO_CHECK_SIMD_EQUAL (nmsub (a, b, c), -(a*b)-c);

    benchmark2 ("madd old *+", [&](VEC&a, VEC&b){ return a*b+c; }, a, b);
    benchmark2 ("madd fused", [&](VEC&a, VEC&b){ return madd(a,b,c); }, a, b);
    benchmark2 ("msub old *-", [&](VEC&a, VEC&b){ return a*b-c; }, a, b);
    benchmark2 ("msub fused", [&](VEC&a, VEC&b){ return msub(a,b,c); }, a, b);
    benchmark2 ("nmadd old (-*)+", [&](VEC&a, VEC&b){ return c-(a*b); }, a, b);
    benchmark2 ("nmadd fused", [&](VEC&a, VEC&b){ return nmadd(a,b,c); }, a, b);
    benchmark2 ("nmsub old -(*+)", [&](VEC&a, VEC&b){ return -(a*b)-c; }, a, b);
    benchmark2 ("nmsub fused", [&](VEC&a, VEC&b){ return nmsub(a,b,c); }, a, b);
}



template<typename T> T do_and (const T& a, const T& b) { return a & b; }
template<typename T> T do_or  (const T& a, const T& b) { return a | b; }
template<typename T> T do_xor (const T& a, const T& b) { return a ^ b; }
template<typename T> T do_compl (const T& a) { return ~a; }
template<typename T> T do_andnot (const T& a, const T& b) { return andnot(a,b); }



template<typename VEC>
void test_bitwise_int ()
{
    test_heading ("bitwise ", VEC::type_name());

    VEC a (0x12341234);
    VEC b (0x11111111);
    OIIO_CHECK_SIMD_EQUAL (a & b, VEC(0x10101010));
    OIIO_CHECK_SIMD_EQUAL (a | b, VEC(0x13351335));
    OIIO_CHECK_SIMD_EQUAL (a ^ b, VEC(0x03250325));
    OIIO_CHECK_SIMD_EQUAL (~(a), VEC(0xedcbedcb));
    OIIO_CHECK_SIMD_EQUAL (andnot (b, a), (~(b)) & a);
    OIIO_CHECK_SIMD_EQUAL (andnot (b, a), VEC(0x02240224));

    VEC atest(15);  atest[1] = 7;
    OIIO_CHECK_EQUAL (reduce_and(atest), 7);

    VEC otest(0);  otest[1] = 3;  otest[2] = 4;
    OIIO_CHECK_EQUAL (reduce_or(otest), 7);

    benchmark2 ("operator&", do_and<VEC>, a, b);
    benchmark2 ("operator|", do_or<VEC>, a, b);
    benchmark2 ("operator^", do_xor<VEC>, a, b);
    benchmark  ("operator!", do_compl<VEC>, a);
    benchmark2 ("andnot",    do_andnot<VEC>, a, b);
    benchmark  ("reduce_and", [](VEC& a){ return reduce_and(a); }, a);
    benchmark  ("reduce_or ", [](VEC& a){ return reduce_or(a); }, a);
}



template<typename VEC>
void test_bitwise_bool ()
{
    test_heading ("bitwise ", VEC::type_name());

    bool A[]   = { true,  true,  false, false, false, false, true,  true,
                   true,  true,  false, false, false, false, true,  true  };
    bool B[]   = { true,  false, true,  false, true,  false, true,  false,
                   true,  false, true,  false, true,  false, true,  false };
    bool AND[] = { true,  false, false, false, false, false, true,  false,
                   true,  false, false, false, false, false, true,  false };
    bool OR[]  = { true,  true,  true,  false, true,  false, true,  true,
                   true,  true,  true,  false, true,  false, true,  true  };
    bool XOR[] = { false, true,  true,  false, true,  false, false, true,
                   false, true,  true,  false, true,  false, false, true  };
    bool NOT[] = { false, false, true,  true,  true,  true,  false, false,
                   false, false, true,  true,  true,  true,  false, false  };
    VEC a(A), b(B), rand(AND), ror(OR), rxor(XOR), rnot(NOT);
    OIIO_CHECK_SIMD_EQUAL (a & b, rand);
    OIIO_CHECK_SIMD_EQUAL (a | b, ror);
    OIIO_CHECK_SIMD_EQUAL (a ^ b, rxor);
    OIIO_CHECK_SIMD_EQUAL (~a, rnot);

    VEC onebit(false); onebit.setcomp(3,true);
    OIIO_CHECK_EQUAL (reduce_or(VEC::False()), false);
    OIIO_CHECK_EQUAL (reduce_or(onebit), true);
    OIIO_CHECK_EQUAL (reduce_and(VEC::True()), true);
    OIIO_CHECK_EQUAL (reduce_and(onebit), false);

    benchmark2 ("operator&", do_and<VEC>, a, b);
    benchmark2 ("operator|", do_or<VEC>, a, b);
    benchmark2 ("operator^", do_xor<VEC>, a, b);
    benchmark  ("operator!", do_compl<VEC>, a);
    benchmark  ("reduce_and", [](VEC& a){ return reduce_and(a); }, a);
    benchmark  ("reduce_or ", [](VEC& a){ return reduce_or(a); }, a);
}



template<class T, class B> B do_lt (const T& a, const T& b) { return a < b; }
template<class T, class B> B do_gt (const T& a, const T& b) { return a > b; }
template<class T, class B> B do_le (const T& a, const T& b) { return a <= b; }
template<class T, class B> B do_ge (const T& a, const T& b) { return a >= b; }
template<class T, class B> B do_eq (const T& a, const T& b) { return a == b; }
template<class T, class B> B do_ne (const T& a, const T& b) { return a != b; }



template<typename VEC>
void test_comparisons ()
{
    typedef typename VEC::value_t ELEM;
    typedef typename VEC::bool_t bool_t;
    test_heading ("comparisons ", VEC::type_name());

    VEC a = VEC::Iota();
    bool lt2[] = { 1, 1, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0 };
    bool gt2[] = { 0, 0, 0, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1 };
    bool le2[] = { 1, 1, 1, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0 };
    bool ge2[] = { 0, 0, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1 };
    bool eq2[] = { 0, 0, 1, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0 };
    bool ne2[] = { 1, 1, 0, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1 };
    OIIO_CHECK_SIMD_EQUAL ((a < 2), bool_t(lt2));
    OIIO_CHECK_SIMD_EQUAL ((a > 2), bool_t(gt2));
    OIIO_CHECK_SIMD_EQUAL ((a <= 2), bool_t(le2));
    OIIO_CHECK_SIMD_EQUAL ((a >= 2), bool_t(ge2));
    OIIO_CHECK_SIMD_EQUAL ((a == 2), bool_t(eq2));
    OIIO_CHECK_SIMD_EQUAL ((a != 2), bool_t(ne2));
    VEC b (ELEM(2));
    OIIO_CHECK_SIMD_EQUAL ((a < b), bool_t(lt2));
    OIIO_CHECK_SIMD_EQUAL ((a > b), bool_t(gt2));
    OIIO_CHECK_SIMD_EQUAL ((a <= b), bool_t(le2));
    OIIO_CHECK_SIMD_EQUAL ((a >= b), bool_t(ge2));
    OIIO_CHECK_SIMD_EQUAL ((a == b), bool_t(eq2));
    OIIO_CHECK_SIMD_EQUAL ((a != b), bool_t(ne2));

    benchmark2 ("operator< ", do_lt<VEC,bool_t>, a, b);
    benchmark2 ("operator> ", do_gt<VEC,bool_t>, a, b);
    benchmark2 ("operator<=", do_le<VEC,bool_t>, a, b);
    benchmark2 ("operator>=", do_ge<VEC,bool_t>, a, b);
    benchmark2 ("operator==", do_eq<VEC,bool_t>, a, b);
    benchmark2 ("operator!=", do_ne<VEC,bool_t>, a, b);
}



template<typename VEC>
void test_shuffle4 ()
{
    test_heading ("shuffle ", VEC::type_name());

    VEC a (0, 1, 2, 3);
    OIIO_CHECK_SIMD_EQUAL ((shuffle<3,2,1,0>(a)), VEC(3,2,1,0));
    OIIO_CHECK_SIMD_EQUAL ((shuffle<0,0,2,2>(a)), VEC(0,0,2,2));
    OIIO_CHECK_SIMD_EQUAL ((shuffle<1,1,3,3>(a)), VEC(1,1,3,3));
    OIIO_CHECK_SIMD_EQUAL ((shuffle<0,1,0,1>(a)), VEC(0,1,0,1));
    OIIO_CHECK_SIMD_EQUAL ((shuffle<2>(a)), VEC(2));

    benchmark ("shuffle<...> ", [&](VEC& v){ return shuffle<3,2,1,0>(v); }, a);
    benchmark ("shuffle<0> ", [&](VEC& v){ return shuffle<0>(v); }, a);
    benchmark ("shuffle<1> ", [&](VEC& v){ return shuffle<1>(v); }, a);
    benchmark ("shuffle<2> ", [&](VEC& v){ return shuffle<2>(v); }, a);
    benchmark ("shuffle<3> ", [&](VEC& v){ return shuffle<3>(v); }, a);
}



template<typename VEC>
void test_shuffle8 ()
{
    test_heading ("shuffle ", VEC::type_name());
    VEC a (0, 1, 2, 3, 4, 5, 6, 7);
    OIIO_CHECK_SIMD_EQUAL ((shuffle<3,2,1,0,3,2,1,0>(a)), VEC(3,2,1,0,3,2,1,0));
    OIIO_CHECK_SIMD_EQUAL ((shuffle<0,0,2,2,0,0,2,2>(a)), VEC(0,0,2,2,0,0,2,2));
    OIIO_CHECK_SIMD_EQUAL ((shuffle<1,1,3,3,1,1,3,3>(a)), VEC(1,1,3,3,1,1,3,3));
    OIIO_CHECK_SIMD_EQUAL ((shuffle<0,1,0,1,0,1,0,1>(a)), VEC(0,1,0,1,0,1,0,1));
    OIIO_CHECK_SIMD_EQUAL ((shuffle<2>(a)), VEC(2));

    benchmark ("shuffle<...> ", [&](VEC& v){ return shuffle<7,6,5,4,3,2,1,0>(v); }, a);
    benchmark ("shuffle<0> ", [&](VEC& v){ return shuffle<0>(v); }, a);
    benchmark ("shuffle<1> ", [&](VEC& v){ return shuffle<1>(v); }, a);
    benchmark ("shuffle<2> ", [&](VEC& v){ return shuffle<2>(v); }, a);
    benchmark ("shuffle<3> ", [&](VEC& v){ return shuffle<3>(v); }, a);
    benchmark ("shuffle<4> ", [&](VEC& v){ return shuffle<4>(v); }, a);
    benchmark ("shuffle<5> ", [&](VEC& v){ return shuffle<5>(v); }, a);
    benchmark ("shuffle<6> ", [&](VEC& v){ return shuffle<6>(v); }, a);
    benchmark ("shuffle<7> ", [&](VEC& v){ return shuffle<7>(v); }, a);
}



template<typename VEC>
void test_shuffle16 ()
{
    test_heading ("shuffle ", VEC::type_name());
    VEC a (0, 1, 2, 3, 4, 5, 6, 7,  8, 9, 10, 11, 12, 13, 14, 15);

    // Shuffle groups of 4
    OIIO_CHECK_SIMD_EQUAL ((shuffle4<3,2,1,0>(a)),
                           VEC(12,13,14,15,8,9,10,11,4,5,6,7,0,1,2,3));
    OIIO_CHECK_SIMD_EQUAL ((shuffle4<3>(a)),
                           VEC(12,13,14,15,12,13,14,15,12,13,14,15,12,13,14,15));

    // Shuffle within groups of 4
    OIIO_CHECK_SIMD_EQUAL ((shuffle<3,2,1,0>(a)),
                           VEC(3,2,1,0,7,6,5,4,11,10,9,8,15,14,13,12));
    OIIO_CHECK_SIMD_EQUAL ((shuffle<3>(a)),
                           VEC(3,3,3,3,7,7,7,7,11,11,11,11,15,15,15,15));

    benchmark ("shuffle4<> ", [&](VEC& v){ return shuffle<3,2,1,0>(v); }, a);
    benchmark ("shuffle<> ",  [&](VEC& v){ return shuffle<3,2,1,0>(v); }, a);
}



template<typename VEC>
void test_swizzle ()
{
    test_heading ("swizzle ", VEC::type_name());

    VEC a = VEC::Iota(0);
    VEC b = VEC::Iota(10);
    OIIO_CHECK_SIMD_EQUAL (AxyBxy(a,b), VEC(0,1,10,11));
    OIIO_CHECK_SIMD_EQUAL (AxBxAyBy(a,b), VEC(0,10,1,11));
    OIIO_CHECK_SIMD_EQUAL (b.xyz0(), VEC(10,11,12,0));
    OIIO_CHECK_SIMD_EQUAL (b.xyz1(), VEC(10,11,12,1));
}



template<typename VEC>
void test_blend ()
{
    test_heading ("blend ", VEC::type_name());
    typedef typename VEC::value_t ELEM;
    typedef typename VEC::bool_t bool_t;

    VEC a = VEC::Iota (1);
    VEC b = VEC::Iota (10);
    bool_t f(false), t(true);
    bool tf_values[] = { true, false, true, false, true, false, true, false,
                         true, false, true, false, true, false, true, false };
    bool_t tf ((bool *)tf_values);

    OIIO_CHECK_SIMD_EQUAL (blend (a, b, f), a);
    OIIO_CHECK_SIMD_EQUAL (blend (a, b, t), b);

    ELEM r1[] = { 10, 2, 12, 4, 14, 6, 16, 8,  18, 10, 20, 12, 22, 14, 24, 16 };
    OIIO_CHECK_SIMD_EQUAL (blend (a, b, tf), VEC(r1));

    OIIO_CHECK_SIMD_EQUAL (blend0 (a, f), VEC::Zero());
    OIIO_CHECK_SIMD_EQUAL (blend0 (a, t), a);
    ELEM r2[] = { 1, 0, 3, 0, 5, 0, 7, 0,  9, 0, 11, 0, 13, 0, 15, 0 };
    OIIO_CHECK_SIMD_EQUAL (blend0 (a, tf), VEC(r2));

    OIIO_CHECK_SIMD_EQUAL (blend0not (a, f), a);
    OIIO_CHECK_SIMD_EQUAL (blend0not (a, t), VEC::Zero());
    ELEM r3[] = { 0, 2, 0, 4, 0, 6, 0, 8,  0, 10, 0, 12, 0, 14, 0, 16 };
    OIIO_CHECK_SIMD_EQUAL (blend0not (a, tf), VEC(r3));

    benchmark2 ("blend", [&](VEC& a, VEC& b){ return blend(a,b,tf); }, a, b);
    benchmark2 ("blend0", [](VEC& a, bool_t& b){ return blend0(a,b); }, a, tf);
    benchmark2 ("blend0not", [](VEC& a, bool_t& b){ return blend0not(a,b); }, a, tf);
}



template<typename VEC>
void test_transpose4 ()
{
    test_heading ("transpose ", VEC::type_name());

    VEC a (0, 1, 2, 3);
    VEC b (4, 5, 6, 7);
    VEC c (8, 9, 10, 11);
    VEC d (12, 13, 14, 15);

    OIIO_CHECK_SIMD_EQUAL (AxBxCxDx(a,b,c,d), VEC(0,4,8,12));

    std::cout << " before transpose:\n";
    std::cout << "\t" << a << "\n";
    std::cout << "\t" << b << "\n";
    std::cout << "\t" << c << "\n";
    std::cout << "\t" << d << "\n";
    transpose (a, b, c, d);
    std::cout << " after transpose:\n";
    std::cout << "\t" << a << "\n";
    std::cout << "\t" << b << "\n";
    std::cout << "\t" << c << "\n";
    std::cout << "\t" << d << "\n";
    OIIO_CHECK_SIMD_EQUAL (a, VEC(0,4,8,12));
    OIIO_CHECK_SIMD_EQUAL (b, VEC(1,5,9,13));
    OIIO_CHECK_SIMD_EQUAL (c, VEC(2,6,10,14));
    OIIO_CHECK_SIMD_EQUAL (d, VEC(3,7,11,15));
}



template<typename T> inline T do_shl (const T &a, int b) { return a<<b; }
template<typename T> inline T do_shr (const T &a, int b) { return a>>b; }
template<typename T> inline T do_srl (const T &a, int b) { return srl(a,b); }


template<typename VEC>
void test_shift ()
{
    test_heading ("shift ", VEC::type_name());

    // Basics of << and >>
    VEC i = VEC::Iota (10, 10);   // 10, 20, 30 ...
    OIIO_CHECK_SIMD_EQUAL (i << 2, VEC::Iota(40, 40));
    OIIO_CHECK_SIMD_EQUAL (i >> 1, VEC::Iota(5, 5));

    // Tricky cases with high bits, and the difference between >> and srl
    int vals[4] = { 1<<31, -1, 0xffff, 3 };
    for (auto hard : vals) {
        VEC vhard(hard);
        OIIO_CHECK_SIMD_EQUAL (vhard >> 1, VEC(hard>>1));
        OIIO_CHECK_SIMD_EQUAL (srl(vhard,1), VEC(unsigned(hard)>>1));
        std::cout << Strutil::format ("  [%x] >>  1 == [%x]\n", vhard, vhard>>1);
        std::cout << Strutil::format ("  [%x] srl 1 == [%x]\n", vhard, srl(vhard,1));
        OIIO_CHECK_SIMD_EQUAL (srl(vhard,4), VEC(unsigned(hard)>>4));
        std::cout << Strutil::format ("  [%x] >>  4 == [%x]\n", vhard, vhard>>4);
        std::cout << Strutil::format ("  [%x] srl 4 == [%x]\n", vhard, srl(vhard,4));
    }

    // Test <<= and >>=
    i = VEC::Iota (10, 10);   i <<= 2;
    OIIO_CHECK_SIMD_EQUAL (i, VEC::Iota(40, 40));
    i = VEC::Iota (10, 10);   i >>= 1;
    OIIO_CHECK_SIMD_EQUAL (i, VEC::Iota(5, 5));

    // Benchmark
    benchmark2 ("operator<<", do_shl<VEC>, i, 2);
    benchmark2 ("operator>>", do_shr<VEC>, i, 2);
    benchmark2 ("srl       ", do_srl<VEC>, i, 2);
}



void test_vectorops_float4 ()
{
    typedef float4 VEC;
    typedef VEC::value_t ELEM;
    test_heading ("vectorops ", VEC::type_name());

    VEC a = mkvec<VEC> (10, 11, 12, 13);
    VEC b = mkvec<VEC> (1, 2, 3, 4);
    OIIO_CHECK_EQUAL (dot(a,b), ELEM(10+22+36+52));
    OIIO_CHECK_EQUAL (dot3(a,b), ELEM(10+22+36));
    OIIO_CHECK_SIMD_EQUAL (vdot(a,b), VEC(10+22+36+52));
    OIIO_CHECK_SIMD_EQUAL (vdot3(a,b), VEC(10+22+36));
    OIIO_CHECK_SIMD_EQUAL (hdiv(float4(1.0f,2.0f,3.0f,2.0f)), float3(0.5f,1.0f,1.5f));

    benchmark2 ("vdot", [](VEC& a, VEC& b){ return vdot(a,b); }, a, b);
    benchmark2 ("dot", [](VEC& a, VEC& b){ return dot(a,b); }, a, b);
    benchmark2 ("vdot3", [](VEC& a, VEC& b){ return vdot3(a,b); }, a, b);
    benchmark2 ("dot3", [](VEC& a, VEC& b){ return dot3(a,b); }, a, b);
}



void test_vectorops_float3 ()
{
    typedef float3 VEC;
    typedef VEC::value_t ELEM;
    test_heading ("vectorops ", VEC::type_name());

    VEC a = mkvec<VEC> (10, 11, 12);
    VEC b = mkvec<VEC> (1, 2, 3);
    OIIO_CHECK_EQUAL (dot(a,b), ELEM(10+22+36));
    OIIO_CHECK_EQUAL (dot3(a,b), ELEM(10+22+36));
    OIIO_CHECK_SIMD_EQUAL (vdot(a,b), VEC(10+22+36));
    OIIO_CHECK_SIMD_EQUAL (vdot3(a,b), VEC(10+22+36));
    OIIO_CHECK_SIMD_EQUAL (float3(1.0f,2.0f,3.0f).normalized(),
                           float3(norm_imath(Imath::V3f(1.0f,2.0f,3.0f))));
    OIIO_CHECK_SIMD_EQUAL_THRESH (float3(1.0f,2.0f,3.0f).normalized_fast(),
                                  float3(norm_imath(Imath::V3f(1.0f,2.0f,3.0f))), 0.0005);

    benchmark2 ("vdot", [](VEC& a, VEC& b){ return vdot(a,b); }, a, b);
    benchmark2 ("dot", [](VEC& a, VEC& b){ return dot(a,b); }, a, b);
    benchmark ("dot float3", dot_simd, float3(2.0f,1.0f,0.0f), 1);
    // benchmark2 ("dot Imath::V3f", [](Imath::V3f& a, Imath::V3f& b){ return a.dot(b); }, a.V3f(), b.V3f());
    benchmark ("dot Imath::V3f", dot_imath, Imath::V3f(2.0f,1.0f,0.0f), 1);
    benchmark ("dot Imath::V3f with simd", dot_imath_simd, Imath::V3f(2.0f,1.0f,0.0f), 1);
    benchmark ("normalize Imath", norm_imath, Imath::V3f(1.0f,4.0f,9.0f));
    benchmark ("normalize Imath with simd", norm_imath_simd, Imath::V3f(1.0f,4.0f,9.0f));
    benchmark ("normalize Imath with simd fast", norm_imath_simd_fast, Imath::V3f(1.0f,4.0f,9.0f));
    benchmark ("normalize simd", norm_simd, float3(1.0f,4.0f,9.0f));
    benchmark ("normalize simd fast", norm_simd_fast, float3(1.0f,4.0f,9.0f));
}



void test_constants ()
{
    test_heading ("constants");

    OIIO_CHECK_SIMD_EQUAL (bool4::False(), bool4(false));
    OIIO_CHECK_SIMD_EQUAL (bool4::True(), bool4(true));

    OIIO_CHECK_SIMD_EQUAL (bool8::False(), bool8(false));
    OIIO_CHECK_SIMD_EQUAL (bool8::True(), bool8(true));

    OIIO_CHECK_SIMD_EQUAL (bool16::False(), bool16(false));
    OIIO_CHECK_SIMD_EQUAL (bool16::True(), bool16(true));
    OIIO_CHECK_SIMD_EQUAL (bool16::False(), bool16(false));
    OIIO_CHECK_SIMD_EQUAL (bool16::True(), bool16(true));

    OIIO_CHECK_SIMD_EQUAL (int4::Zero(), int4(0));
    OIIO_CHECK_SIMD_EQUAL (int4::One(), int4(1));
    OIIO_CHECK_SIMD_EQUAL (int4::NegOne(), int4(-1));
    OIIO_CHECK_SIMD_EQUAL (int4::Iota(), int4(0,1,2,3));
    OIIO_CHECK_SIMD_EQUAL (int4::Iota(3), int4(3,4,5,6));
    OIIO_CHECK_SIMD_EQUAL (int4::Iota(3,2), int4(3,5,7,9));
    OIIO_CHECK_SIMD_EQUAL (int4::Giota(), int4(1,2,4,8));

    OIIO_CHECK_SIMD_EQUAL (int8::Zero(), int8(0));
    OIIO_CHECK_SIMD_EQUAL (int8::One(), int8(1));
    OIIO_CHECK_SIMD_EQUAL (int8::NegOne(), int8(-1));
    OIIO_CHECK_SIMD_EQUAL (int8::Iota(), int8(0,1,2,3, 4,5,6,7));
    OIIO_CHECK_SIMD_EQUAL (int8::Iota(3), int8(3,4,5,6, 7,8,9,10));
    OIIO_CHECK_SIMD_EQUAL (int8::Iota(3,2), int8(3,5,7,9, 11,13,15,17));
    OIIO_CHECK_SIMD_EQUAL (int8::Giota(), int8(1,2,4,8, 16,32,64,128));

    OIIO_CHECK_SIMD_EQUAL (int16::Zero(), int16(0));
    OIIO_CHECK_SIMD_EQUAL (int16::One(), int16(1));
    OIIO_CHECK_SIMD_EQUAL (int16::NegOne(), int16(-1));
    OIIO_CHECK_SIMD_EQUAL (int16::Iota(), int16(0,1,2,3, 4,5,6,7, 8,9,10,11, 12,13,14,15));
    OIIO_CHECK_SIMD_EQUAL (int16::Iota(3), int16(3,4,5,6, 7,8,9,10, 11,12,13,14, 15,16,17,18));
    OIIO_CHECK_SIMD_EQUAL (int16::Iota(3,2), int16(3,5,7,9, 11,13,15,17, 19,21,23,25, 27,29,31,33));
    OIIO_CHECK_SIMD_EQUAL (int16::Giota(), int16(1,2,4,8, 16,32,64,128, 256,512,1024,2048, 4096,8192,16384,32768));

    OIIO_CHECK_SIMD_EQUAL (float4::Zero(), float4(0.0f));
    OIIO_CHECK_SIMD_EQUAL (float4::One(), float4(1.0f));
    OIIO_CHECK_SIMD_EQUAL (float4::Iota(), float4(0,1,2,3));
    OIIO_CHECK_SIMD_EQUAL (float4::Iota(3.0f), float4(3,4,5,6));
    OIIO_CHECK_SIMD_EQUAL (float4::Iota(3.0f,2.0f), float4(3,5,7,9));

    OIIO_CHECK_SIMD_EQUAL (float3::Zero(), float3(0.0f));
    OIIO_CHECK_SIMD_EQUAL (float3::One(), float3(1.0f));
    OIIO_CHECK_SIMD_EQUAL (float3::Iota(), float3(0,1,2));
    OIIO_CHECK_SIMD_EQUAL (float3::Iota(3.0f), float3(3,4,5));
    OIIO_CHECK_SIMD_EQUAL (float3::Iota(3.0f,2.0f), float3(3,5,7));

    OIIO_CHECK_SIMD_EQUAL (float8::Zero(), float8(0.0f));
    OIIO_CHECK_SIMD_EQUAL (float8::One(), float8(1.0f));
    OIIO_CHECK_SIMD_EQUAL (float8::Iota(), float8(0,1,2,3,4,5,6,7));
    OIIO_CHECK_SIMD_EQUAL (float8::Iota(3.0f), float8(3,4,5,6,7,8,9,10));
    OIIO_CHECK_SIMD_EQUAL (float8::Iota(3.0f,2.0f), float8(3,5,7,9,11,13,15,17));

    OIIO_CHECK_SIMD_EQUAL (float16::Zero(), float16(0.0f));
    OIIO_CHECK_SIMD_EQUAL (float16::One(), float16(1.0f));
    OIIO_CHECK_SIMD_EQUAL (float16::Iota(), float16(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15));
    OIIO_CHECK_SIMD_EQUAL (float16::Iota(3.0f), float16(3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18));
    OIIO_CHECK_SIMD_EQUAL (float16::Iota(3.0f,2.0f), float16(3,5,7,9,11,13,15,17,19,21,23,25,27,29,31,33));

    benchmark ("float4 = float(const)", [](float f){ return float4(f); }, 1.0f);
    benchmark ("float4 = Zero()", [](int){ return float4::Zero(); }, 0);
    benchmark ("float4 = One()", [](int){ return float4::One(); }, 0);
    benchmark ("float4 = Iota()", [](int){ return float4::Iota(); }, 0);

    benchmark ("float8 = float(const)", [](float f){ return float8(f); }, 1.0f);
    benchmark ("float8 = Zero()", [](int){ return float8::Zero(); }, 0);
    benchmark ("float8 = One()", [](int){ return float8::One(); }, 0);
    benchmark ("float8 = Iota()", [](int){ return float8::Iota(); }, 0);

    benchmark ("float16 = float(const)", [](float f){ return float16(f); }, 1.0f);
    benchmark ("float16 = Zero()", [](int){ return float16::Zero(); }, 0);
    benchmark ("float16 = One()", [](int){ return float16::One(); }, 0);
    benchmark ("float16 = Iota()", [](int){ return float16::Iota(); }, 0);
}



// Miscellaneous one-off stuff not caught by other tests
void test_special ()
{
    test_heading ("special");
    {
        // Make sure a float4 constructed from saturated unsigned short,
        // short, unsigned char, or char values, then divided by the float
        // max, exactly equals 1.0.
        short s32767[] = {32767, 32767, 32767, 32767};
        unsigned short us65535[] = {65535, 65535, 65535, 65535};
        char c127[] = {127, 127, 127, 127};
        unsigned char uc255[] = {255, 255, 255, 255};
        OIIO_CHECK_SIMD_EQUAL (float4(us65535)/float4(65535.0), float4(1.0f));
        OIIO_CHECK_SIMD_EQUAL (float4(us65535)*float4(1.0f/65535.0), float4(1.0f));
        OIIO_CHECK_SIMD_EQUAL (float4(s32767)/float4(32767.0), float4(1.0f));
        OIIO_CHECK_SIMD_EQUAL (float4(s32767)*float4(1.0f/32767.0), float4(1.0f));
        OIIO_CHECK_SIMD_EQUAL (float4(uc255)/float4(255.0), float4(1.0f));
        OIIO_CHECK_SIMD_EQUAL (float4(uc255)*float4(1.0f/255.0), float4(1.0f));
        OIIO_CHECK_SIMD_EQUAL (float4(c127)/float4(127.0), float4(1.0f));
        OIIO_CHECK_SIMD_EQUAL (float4(c127)*float4(1.0f/127.0), float4(1.0f));
    }
}



// Wrappers to resolve the return type ambiguity
inline float fast_exp_float (float x) { return fast_exp(x); }
inline float4 fast_exp_float4 (const float4& x) { return fast_exp(x); }
inline float fast_log_float (float x) { return fast_log(x); }
//inline float4 fast_log_float (const float4& x) { return fast_log(x); }
inline float rsqrtf (float f) { return 1.0f / sqrtf(f); }
inline float rcp (float f) { return 1.0f / f; }



template<typename VEC>
void test_mathfuncs ()
{
    test_heading ("mathfuncs", VEC::type_name());

    VEC A = mkvec<VEC> (-1.0f, 0.0f, 1.0f, 4.5f);
    VEC expA = mkvec<VEC> (0.367879441171442f, 1.0f, 2.718281828459045f, 90.0171313005218f);
    OIIO_CHECK_SIMD_EQUAL (exp(A), expA);
    OIIO_CHECK_SIMD_EQUAL_THRESH (log(expA), A, 1e-6f);
    OIIO_CHECK_SIMD_EQUAL (fast_exp(A),
                mkvec<VEC>(fast_exp(A[0]), fast_exp(A[1]), fast_exp(A[2]), fast_exp(A[3])));
    OIIO_CHECK_SIMD_EQUAL (fast_log(expA),
                mkvec<VEC>(fast_log(expA[0]), fast_log(expA[1]), fast_log(expA[2]), fast_log(expA[3])));
    OIIO_CHECK_SIMD_EQUAL_THRESH (fast_pow_pos(VEC(2.0f), A),
                           mkvec<VEC>(0.5f, 1.0f, 2.0f, 22.62741699796952f), 0.0001f);

    OIIO_CHECK_SIMD_EQUAL (safe_div(mkvec<VEC>(1.0f,2.0f,3.0f,4.0f), mkvec<VEC>(2.0f,0.0f,2.0f,0.0f)),
                           mkvec<VEC>(0.5f,0.0f,1.5f,0.0f));
    OIIO_CHECK_SIMD_EQUAL (sqrt(mkvec<VEC>(1.0f,4.0f,9.0f,16.0f)), mkvec<VEC>(1.0f,2.0f,3.0f,4.0f));
    OIIO_CHECK_SIMD_EQUAL (rsqrt(mkvec<VEC>(1.0f,4.0f,9.0f,16.0f)), VEC(1.0f)/mkvec<VEC>(1.0f,2.0f,3.0f,4.0f));
    OIIO_CHECK_SIMD_EQUAL_THRESH (rsqrt_fast(mkvec<VEC>(1.0f,4.0f,9.0f,16.0f)),
                                  VEC(1.0f)/mkvec<VEC>(1.0f,2.0f,3.0f,4.0f), 0.0005f);
    OIIO_CHECK_SIMD_EQUAL_THRESH (rcp_fast(VEC::Iota(1.0f)),
                                  VEC(1.0f)/VEC::Iota(1.0f), 0.0005f);

    benchmark2 ("simd operator/", do_div<VEC>, A, A);
    benchmark2 ("simd safe_div", do_safe_div<VEC>, A, A);
    benchmark ("simd rcp_fast", [](VEC& v){ return rcp_fast(v); }, mkvec<VEC>(1.0f,4.0f,9.0f,16.0f));
    benchmark ("float expf", expf, 0.67f);
    benchmark ("float fast_exp", fast_exp_float, 0.67f);
    benchmark ("simd exp", [](VEC& v){ return simd::exp(v); }, VEC(0.67f));
    benchmark ("simd fast_exp", [](VEC& v){ return fast_exp(v); }, VEC(0.67f));

    benchmark ("float logf", logf, 0.67f);
    benchmark ("fast_log", fast_log_float, 0.67f);
    benchmark ("simd log", [](VEC& v){ return simd::log(v); }, VEC(0.67f));
    benchmark ("simd fast_log", fast_log<VEC>, VEC(0.67f));
    benchmark2 ("float powf", powf, 0.67f, 0.67f);
    benchmark2 ("simd fast_pow_pos", [](VEC& x,VEC& y){ return fast_pow_pos(x,y); }, VEC(0.67f), VEC(0.67f));
    benchmark ("float sqrt", sqrtf, 4.0f);
    benchmark ("simd sqrt", [](VEC& v){ return sqrt(v); }, mkvec<VEC>(1.0f,4.0f,9.0f,16.0f));
    benchmark ("float rsqrt", rsqrtf, 4.0f);
    benchmark ("simd rsqrt", [](VEC& v){ return rsqrt(v); }, mkvec<VEC>(1.0f,4.0f,9.0f,16.0f));
    benchmark ("simd rsqrt_fast", [](VEC& v){ return rsqrt_fast(v); }, mkvec<VEC>(1.0f,4.0f,9.0f,16.0f));
}



void test_metaprogramming ()
{
    test_heading ("metaprogramming");
    OIIO_CHECK_EQUAL (SimdSize<float4>::size, 4);
    OIIO_CHECK_EQUAL (SimdSize<float3>::size, 4);
    OIIO_CHECK_EQUAL (SimdSize<int4>::size, 4);
    OIIO_CHECK_EQUAL (SimdSize<bool4>::size, 4);
    OIIO_CHECK_EQUAL (SimdSize<float8>::size, 8);
    OIIO_CHECK_EQUAL (SimdSize<int8>::size, 8);
    OIIO_CHECK_EQUAL (SimdSize<bool8>::size, 8);
    OIIO_CHECK_EQUAL (SimdSize<float16>::size, 16);
    OIIO_CHECK_EQUAL (SimdSize<int16>::size, 16);
    OIIO_CHECK_EQUAL (SimdSize<bool16>::size, 16);
    OIIO_CHECK_EQUAL (SimdSize<float>::size, 1);
    OIIO_CHECK_EQUAL (SimdSize<int>::size, 1);
    OIIO_CHECK_EQUAL (SimdSize<bool>::size, 1);

    OIIO_CHECK_EQUAL (SimdElements<float4>::size, 4);
    OIIO_CHECK_EQUAL (SimdElements<float3>::size, 3);
    OIIO_CHECK_EQUAL (SimdElements<int4>::size, 4);
    OIIO_CHECK_EQUAL (SimdElements<bool4>::size, 4);
    OIIO_CHECK_EQUAL (SimdElements<float8>::size, 8);
    OIIO_CHECK_EQUAL (SimdElements<int8>::size, 8);
    OIIO_CHECK_EQUAL (SimdElements<bool8>::size, 8);
    OIIO_CHECK_EQUAL (SimdElements<float16>::size, 16);
    OIIO_CHECK_EQUAL (SimdElements<int16>::size, 16);
    OIIO_CHECK_EQUAL (SimdElements<bool16>::size, 16);
    OIIO_CHECK_EQUAL (SimdElements<float>::size, 1);
    OIIO_CHECK_EQUAL (SimdElements<int>::size, 1);
    OIIO_CHECK_EQUAL (SimdElements<bool>::size, 1);

    OIIO_CHECK_EQUAL (float4::elements, 4);
    OIIO_CHECK_EQUAL (float3::elements, 3);
    OIIO_CHECK_EQUAL (int4::elements, 4);
    OIIO_CHECK_EQUAL (bool4::elements, 4);
    OIIO_CHECK_EQUAL (float8::elements, 8);
    OIIO_CHECK_EQUAL (int8::elements, 8);
    OIIO_CHECK_EQUAL (bool8::elements, 8);
    OIIO_CHECK_EQUAL (float16::elements, 16);
    OIIO_CHECK_EQUAL (int16::elements, 16);
    OIIO_CHECK_EQUAL (bool16::elements, 16);

    // OIIO_CHECK_EQUAL (is_same<float4::value_t,float>::value, true);
    // OIIO_CHECK_EQUAL (is_same<float3::value_t,float>::value, true);
    // OIIO_CHECK_EQUAL (is_same<int4::value_t,int>::value, true);
    // OIIO_CHECK_EQUAL (is_same<bool4::value_t,int>::value, true);
}



// Transform a point by a matrix using regular Imath
inline Imath::V3f
transformp_imath (const Imath::V3f &v, const Imath::M44f &m)
{
    Imath::V3f r;
    m.multVecMatrix (v, r);
    return r;
}

// Transform a point by a matrix using simd ops on Imath types.
inline Imath::V3f
transformp_imath_simd (const Imath::V3f &v, const Imath::M44f &m)
{
    return simd::transformp(m,v).V3f();
}

// Transform a simd point by an Imath matrix using SIMD
inline float3
transformp_simd (const float3 &v, const Imath::M44f &m)
{
    return simd::transformp (m, v);
}

// Transform a point by a matrix using regular Imath
inline Imath::V3f
transformv_imath (const Imath::V3f &v, const Imath::M44f &m)
{
    Imath::V3f r;
    m.multDirMatrix (v, r);
    return r;
}



inline bool
mx_equal_thresh (const matrix44 &a, const matrix44 &b, float thresh)
{
    for (int j = 0; j < 4; ++j)
        for (int i = 0; i < 4; ++i)
            if (fabsf(a[j][i] - b[j][i]) > thresh)
                return false;
    return true;
}



inline Imath::M44f mat_transpose (const Imath::M44f &m) {
    return m.transposed();
}

inline Imath::M44f mat_transpose_simd (const Imath::M44f &m) {
    return matrix44(m).transposed().M44f();
}


void test_matrix ()
{
    Imath::V3f P (1.0f, 0.0f, 0.0f);
    Imath::M44f Mtrans (1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  10, 11, 12, 1);
    Imath::M44f Mrot = Imath::M44f().rotate(Imath::V3f(0.0f, M_PI_2, 0.0f));

    test_heading ("Testing matrix ops:");
    std::cout << "  P = " << P << "\n";
    std::cout << "  Mtrans = " << Mtrans << "\n";
    std::cout << "  Mrot   = " << Mrot << "\n";
    OIIO_CHECK_EQUAL (simd::transformp(Mtrans, P).V3f(),
                      transformp_imath(P, Mtrans));
    std::cout << "  P translated = " << simd::transformp(Mtrans,P) << "\n";
    OIIO_CHECK_EQUAL (simd::transformv(Mtrans,P).V3f(), P);
    OIIO_CHECK_EQUAL (simd::transformp(Mrot, P).V3f(),
                      transformp_imath(P, Mrot));
    std::cout << "  P rotated = " << simd::transformp(Mrot,P) << "\n";
    OIIO_CHECK_EQUAL (simd::transformvT(Mrot, P).V3f(),
                      transformv_imath(P, Mrot.transposed()));
    std::cout << "  P rotated by the transpose = " << simd::transformv(Mrot,P) << "\n";
    OIIO_CHECK_EQUAL (matrix44(Mrot).transposed().M44f(),
                      Mrot.transposed());
    std::cout << "  Mrot transposed = " << matrix44(Mrot).transposed().M44f() << "\n";
    {
        matrix44 mt (Mtrans), mr (Mrot);
        OIIO_CHECK_EQUAL (mt, mt);
        OIIO_CHECK_EQUAL (mt, Mtrans);
        OIIO_CHECK_EQUAL (Mtrans, mt);
        OIIO_CHECK_NE (mt, mr);
        OIIO_CHECK_NE (mr, Mtrans);
        OIIO_CHECK_NE (Mtrans, mr);
    }
    OIIO_CHECK_ASSERT (mx_equal_thresh (Mtrans.inverse(),
                       matrix44(Mtrans).inverse(), 1.0e-6f));
    OIIO_CHECK_ASSERT (mx_equal_thresh (Mrot.inverse(),
                       matrix44(Mrot).inverse(), 1.0e-6f));
    OIIO_CHECK_EQUAL (matrix44(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15),
                      Imath::M44f(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15));

    Imath::V3f vx (2.51f,1.0f,1.0f);
    Imath::M44f mx (1,0,0,0, 0,1,0,0, 0,0,1,0, 10,11,12,1);
    benchmark2 ("transformp Imath", transformp_imath, vx, mx, 1);
    benchmark2 ("transformp Imath with simd", transformp_imath_simd, vx, mx, 1);
    benchmark2 ("transformp simd", transformp_simd, float3(vx), mx, 1);
    benchmark ("transpose m44", mat_transpose, mx, 1);
    benchmark ("transpose m44 with simd", mat_transpose_simd, mx, 1);
    // Reduce the iterations of the ones below, if we can
    iterations /= 2;
    benchmark ("m44 inverse Imath", inverse_imath, mx, 1);
    // std::cout << "inv " << matrix44(inverse_imath(mx)) << "\n";
    benchmark ("m44 inverse_simd", inverse_simd, mx, 1);
    // std::cout << "inv " << inverse_simd(mx) << "\n";
    benchmark ("m44 inverse_simd native simd", inverse_simd, matrix44(mx), 1);
    // std::cout << "inv " << inverse_simd(mx) << "\n";
    iterations *= 2;  // put things the way they were
}





int
main (int argc, char *argv[])
{
#if !defined(NDEBUG) || defined(OIIO_CI) || defined(OIIO_CODE_COVERAGE)
    // For the sake of test time, reduce the default iterations for DEBUG,
    // CI, and code coverage builds. Explicit use of --iters or --trials
    // will override this, since it comes before the getargs() call.
    iterations /= 10;
    ntrials = 1;
#endif
    for (int i = 0; i < 16; ++i) {
        dummy_float[i] = 1.0f;
        dummy_int[i] = 1;
    }

    getargs (argc, argv);

    std::string oiiosimd = OIIO::get_string_attribute ("oiio:simd");
    std::string hwsimd = OIIO::get_string_attribute ("hw:simd");
    std::cout << "OIIO SIMD support is: "
              << (oiiosimd.size() ? oiiosimd : "") << "\n";
    std::cout << "Hardware SIMD support is: "
              << (hwsimd.size() ? hwsimd : "") << "\n";
    std::cout << "\n";

    Timer timer;

    int4 dummy4(0);
    int8 dummy8(0);
    benchmark ("null benchmark 4", [](const int4&){ return int(0); }, dummy4);
    benchmark ("null benchmark 8", [](const int8&){ return int(0); }, dummy8);

    category_heading ("float4");
    test_partial_loadstore<float4> ();
    test_conversion_loadstore_float<float4> ();
    test_component_access<float4> ();
    test_arithmetic<float4> ();
    test_comparisons<float4> ();
    test_shuffle4<float4> ();
    test_swizzle<float4> ();
    test_blend<float4> ();
    test_transpose4<float4> ();
    test_vectorops_float4 ();
    test_fused<float4> ();
    test_mathfuncs<float4>();

    category_heading ("float3");
    test_partial_loadstore<float3> ();
    test_conversion_loadstore_float<float3> ();
    test_component_access<float3> ();
    test_arithmetic<float3> ();
    // Unnecessary to test these, they just use the float4 ops.
    // test_comparisons<float3> ();
    // test_shuffle4<float3> ();
    // test_swizzle<float3> ();
    // test_blend<float3> ();
    // test_transpose4<float3> ();
    test_vectorops_float3 ();
    test_fused<float3> ();
    // test_mathfuncs<float3>();

    category_heading ("float8");
    test_partial_loadstore<float8> ();
    test_conversion_loadstore_float<float8> ();
    test_masked_loadstore<float8> ();
    test_component_access<float8> ();
    test_arithmetic<float8> ();
    test_comparisons<float8> ();
    test_shuffle8<float8> ();
    test_blend<float8> ();
    test_fused<float8> ();
    test_mathfuncs<float8>();

    category_heading ("float16");
    test_partial_loadstore<float16> ();
    test_conversion_loadstore_float<float16> ();
    test_masked_loadstore<float16> ();
    test_component_access<float16> ();
    test_arithmetic<float16> ();
    test_comparisons<float16> ();
    test_shuffle16<float16> ();
    test_blend<float16> ();
    test_fused<float16> ();
    test_mathfuncs<float16>();

    category_heading ("int4");
    test_partial_loadstore<int4> ();
    test_conversion_loadstore_int<int4> ();
    test_component_access<int4> ();
    test_arithmetic<int4> ();
    test_bitwise_int<int4> ();
    test_comparisons<int4> ();
    test_shuffle4<int4> ();
    test_blend<int4> ();
    test_vint_to_uint16s<int4> ();
    test_vint_to_uint8s<int4> ();
    test_shift<int4> ();
    test_transpose4<int4> ();

    category_heading ("int8");
    test_partial_loadstore<int8> ();
    test_conversion_loadstore_int<int8> ();
    test_masked_loadstore<int8> ();
    test_component_access<int8> ();
    test_arithmetic<int8> ();
    test_bitwise_int<int8> ();
    test_comparisons<int8> ();
    test_shuffle8<int8> ();
    test_blend<int8> ();
    test_vint_to_uint16s<int8> ();
    test_vint_to_uint8s<int8> ();
    test_shift<int8> ();

    category_heading ("int16");
    test_partial_loadstore<int16> ();
    test_conversion_loadstore_int<int16> ();
    test_masked_loadstore<int16> ();
    test_component_access<int16> ();
    test_arithmetic<int16> ();
    test_bitwise_int<int16> ();
    test_comparisons<int16> ();
    test_shuffle16<int16> ();
    test_blend<int16> ();
    test_vint_to_uint16s<int16> ();
    test_vint_to_uint16s<int16> ();
    test_shift<int16> ();

    category_heading ("bool4");
    test_shuffle4<bool4> ();
    test_component_access<bool4> ();
    test_bitwise_bool<bool4> ();

    category_heading ("bool8");
    test_shuffle8<bool8> ();
    test_component_access<bool8> ();
    test_bitwise_bool<bool8> ();

    category_heading ("bool16");
    // test_shuffle16<bool16> ();
    test_component_access<bool16> ();
    test_bitwise_bool<bool16> ();

    category_heading ("Odds and ends");
    test_constants();
    test_special();
    test_metaprogramming();
    test_matrix();

    std::cout << "\nTotal time: " << Strutil::timeintervalformat(timer()) << "\n";

    if (unit_test_failures)
        std::cout << term.ansi ("red", "ERRORS!\n");
    else
        std::cout << term.ansi ("green", "OK\n");
    return unit_test_failures;
}

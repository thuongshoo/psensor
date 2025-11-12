#ifndef PSENSOR_COMMON_H
#define PSENSOR_COMMON_H

#if defined(_MSC_VER)
    // MSVC specific pragmas
    #define PACK_STRUCT(n) __pragma(pack(push, n))
    #define UNPACK_STRUCT() __pragma(pack(pop))
    #define ALIGNED(n)
#elif defined(__clang__)
    // Clang specific attributes (must be checked first)
    #define PACK_STRUCT(n)
    #define UNPACK_STRUCT()
    #define ALIGNED(n) __attribute__((aligned(n)))
#elif defined(__GNUC__)
    // GCC specific attributes
    // #define PACK_STRUCT(n)
    // #define UNPACK_STRUCT()
    // #define ALIGNED(n) __attribute__((aligned(n)))

    #define PACK_STRUCT(n)
    #define UNPACK_STRUCT()
    #define ALIGNED(n) __attribute__((aligned(n)))
#else
    //#error "Compiler not supported for structure packing"
    #define PACK_STRUCT(n)
    #define UNPACK_STRUCT()
    #define ALIGNED(n) __attribute__((aligned(n)))
#endif

#endif /* PSENSOR_COMMON_H */
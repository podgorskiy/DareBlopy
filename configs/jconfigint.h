/* libjpeg-turbo build number */
#define BUILD  "static"

/* Compiler's inline keyword */
#undef inline

/* How to obtain function inlining. */
#define INLINE  __inline__ __attribute__((always_inline))

/* Define to the full name of this package. */
#define PACKAGE_NAME  "libjpeg-turbo"

/* Version number of package */
#define VERSION  "2.0.3"

/* The size of `size_t', as computed by sizeof. */
#if defined(__x86_64__) || defined(_M_X64)
/* x86 64-bit ----------------------------------------------- */
#define SIZEOF_SIZE_T  8
#elif defined(__i386) || defined(_M_IX86)
/* x86 32-bit ----------------------------------------------- */
#define SIZEOF_SIZE_T  4
#endif

/* Define if your compiler has __builtin_ctzl() and sizeof(unsigned long) == sizeof(size_t). */
#define HAVE_BUILTIN_CTZL

/* Define to 1 if you have the <intrin.h> header file. */
/* #undef HAVE_INTRIN_H */

#if defined(_MSC_VER) && defined(HAVE_INTRIN_H)
#if (SIZEOF_SIZE_T == 8)
#define HAVE_BITSCANFORWARD64
#elif (SIZEOF_SIZE_T == 4)
#define HAVE_BITSCANFORWARD
#endif
#endif

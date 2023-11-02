#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MIN(a, b) ({__typeof__(a) _a = a, _b = b; (_a) < (_b) ? (_a) : (_b);})
#define MAX(a, b) ({__typeof__(a) _a = a, _b = b; (_a) > (_b) ? (_a) : (_b);})
#define SCOPED(Type) __attribute__((__cleanup__(delete##Type))) Type
#define UNUSED(param) (void)param
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#define FALLTHROUGH __attribute__((fallthrough))
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(*array))
#define LIKELY(cond) __builtin_expect(!!(cond), 1)
#define UNLIKELY(cond) __builtin_expect(!!(cond), 0)
#define VERBATIM __attribute__((optimize(0)))
#define SWAP(a, b) {__typeof__(a) c = a; a = b; b = c;}

#ifdef TEST
    #define TURBO
#else
//    #define TURBO
#endif
#define FPS 60

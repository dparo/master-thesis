#pragma once

#if __cplusplus
extern "C" {
#endif

#ifdef __GNUC__
#define ATTRIB_MAYBE_UNUSED __attribute__((unused))
#elif defined _MSC_VER
#if __cplusplus
#define ATTRIB_MAYBE_UNUSED [[maybe_unused]]
#endif
#define ATTRIB_MAYBE_UNUSED
#endif

#if __cplusplus
}
#endif

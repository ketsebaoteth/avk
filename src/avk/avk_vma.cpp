#if defined(_MSC_VER)
#pragma warning(push, 0)
#pragma warning(disable : 4100) // unreferenced formal parameter
#pragma warning(disable : 4189) // local variable is initialized but not referenced
#pragma warning(disable : 4324) // structure was padded due to alignment specifier
#endif

#define VMA_IMPLEMENTATION
#include "avk/avk_allocator.h" // Automatically includes VMA with the dynamic macros set

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
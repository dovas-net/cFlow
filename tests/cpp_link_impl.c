/* cpp_link_impl — flow's implementation compiled as C (cc), producing unmangled C
   symbols. Linked against the C++ caller cpp_link_main.cpp to prove the header's
   extern "C" lets a C-built flow.o satisfy a C++ translation unit's references.
   No main(): this TU provides only the library. */
#define FLOW_IMPLEMENTATION
#include "../flow.h"

#ifndef KTABLEFS_DEBUG_H_
#define KTABLEFS_DEBUG_H_

#ifdef DEBUG
#include <assert.h>
#define Assert(expr)  assert(expr)
#else // DEBUG
#define Assert(expr)
#endif

#endif // KTABLEFS_DEBUG_H_
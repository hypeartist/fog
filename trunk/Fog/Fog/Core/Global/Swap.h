// [Fog-Core]
//
// [License]
// MIT, See COPYING file in package

// [Guard]
#ifndef _FOG_CORE_GLOBAL_SWAP_H
#define _FOG_CORE_GLOBAL_SWAP_H

// [Dependencies]
#include <Fog/Core/Config/Config.h>

namespace Fog {

//! @addtogroup Fog_Core_Memory
//! @{

// ============================================================================
// [Fog::swap]
// ============================================================================

//! @brief Swap @a a and @a b.
template <typename T>
FOG_INLINE void swap(T& a, T& b)
{
  T c = a;
  a = b;
  b = c;
}

//! @}

} // Fog namespace

// [Guard]
#endif // _FOG_CORE_GLOBAL_SWAP_H

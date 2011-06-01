// [Fog-Core]
//
// [License]
// MIT, See COPYING file in package

// [Guard]
#ifndef _FOG_CORE_MAC_MACSCOPEDAUTORELEASEPOOL_H
#define _FOG_CORE_MAC_MACSCOPEDAUTORELEASEPOOL_H

// [Dependencies]
#include <Fog/Core/Global/Class.h>
#include <Fog/Core/Mac/MacDefs.h>

namespace Fog {

// ============================================================================
// [Fog::MacScopedAutoReleasePool]
// ============================================================================

//! @brief Allocates a @c NSAutoreleasePool when constructed and releases it on
//! destruction.
struct FOG_API MacScopedAutoReleasePool
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MacScopedAutoReleasePool();
  ~MacScopedAutoReleasePool();

  // --------------------------------------------------------------------------
  // [Methods]
  // --------------------------------------------------------------------------

  //! @brief Get cleaned up and start new.
  void recycle();

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  NSAutoreleasePool* _pool;

private:
  _FOG_CLASS_NO_COPY(MacScopedAutoReleasePool)
};

} // Fog namespace

// [Guard]
#endif // _FOG_CORE_MAC_MACSCOPEDAUTORELEASEPOOL_H

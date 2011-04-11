// [Fog-G2d]
//
// [License]
// MIT, See COPYING file in package

// [Guard]
#ifndef _FOG_G2D_SOURCE_COLORSTOPCACHE_H
#define _FOG_G2D_SOURCE_COLORSTOPCACHE_H

// [Dependencies]
#include <Fog/Core/Global/TypeInfo.h>
#include <Fog/Core/Math/Fuzzy.h>
#include <Fog/Core/Memory/Memory.h>
#include <Fog/G2d/Source/Color.h>
#include <Fog/G2d/Source/ColorStop.h>

namespace Fog {

//! @addtogroup Fog_G2d_Source
//! @{

// ============================================================================
// [Fog::ColorStopCache]
// ============================================================================

struct FOG_NO_EXPORT ColorStopCache
{
  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! @brief Get the format of color-stop cache.
  FOG_INLINE uint32_t getFormat() const { return format; }
  //! @brief Get the count of elements in the data[] array.
  FOG_INLINE uint32_t getLength() const { return length; }

  //! @brief Get the data array.
  FOG_INLINE uint8_t* getData()
  { return reinterpret_cast<uint8_t*>(this) + sizeof(ColorStopCache); }

  //! @overload.
  FOG_INLINE const uint8_t* getData() const
  { return reinterpret_cast<const uint8_t*>(this) + sizeof(ColorStopCache); }

  // --------------------------------------------------------------------------
  // [Ref / Deref]
  // --------------------------------------------------------------------------

  FOG_INLINE ColorStopCache* ref() const
  {
    refCount.inc();
    return const_cast<ColorStopCache*>(this);
  }

  FOG_INLINE void deref()
  {
    if (refCount.deref()) destroy(this);
  }

  // --------------------------------------------------------------------------
  // [Statics]
  // --------------------------------------------------------------------------

  static FOG_INLINE ColorStopCache* create32(uint32_t format, uint32_t length)
  {
    ColorStopCache* cache = reinterpret_cast<ColorStopCache*>(
      Memory::alloc(sizeof(ColorStopCache) + (length + 1) * 4));
    if (FOG_UNLIKELY(cache == NULL)) return NULL;

    cache->refCount.init(1);
    cache->format = format;
    cache->length = length;

    return cache;
  }

  static FOG_INLINE ColorStopCache* create64(uint32_t format, uint32_t length)
  {
    ColorStopCache* cache = reinterpret_cast<ColorStopCache*>(
      Memory::alloc(sizeof(ColorStopCache) + (length + 1) * 8));
    if (FOG_UNLIKELY(cache == NULL)) return NULL;

    cache->refCount.init(1);
    cache->format = format;
    cache->length = length;

    return cache;
  }

  static FOG_INLINE void destroy(ColorStopCache* cache)
  {
    Memory::free(cache);
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! @brief Reference count.
  mutable Atomic<sysuint_t> refCount;

  //! @brief Format of color-stop cache.
  uint32_t format;
  //! @brief Count of elements in the data[] array.
  uint32_t length;
};

//! @}

} // Fog namespace

// [Guard]
#endif // _FOG_G2D_SOURCE_COLORSTOPCACHE_H

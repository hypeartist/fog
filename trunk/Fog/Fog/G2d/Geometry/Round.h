// [Fog-G2d]
//
// [License]
// MIT, See COPYING file in package

// [Guard]
#ifndef _FOG_G2D_GEOMETRY_ROUND_H
#define _FOG_G2D_GEOMETRY_ROUND_H

// [Dependencies]
#include <Fog/Core/Global/TypeInfo.h>
#include <Fog/Core/Global/TypeVariant.h>
#include <Fog/Core/Global/Uninitialized.h>
#include <Fog/Core/Math/Fuzzy.h>
#include <Fog/Core/Math/Math.h>
#include <Fog/Core/Memory/Memory.h>
#include <Fog/G2d/Geometry/Point.h>
#include <Fog/G2d/Geometry/Rect.h>
#include <Fog/G2d/Global/Api.h>

namespace Fog {

//! @addtogroup Fog_G2d_Geometry
//! @{

// ============================================================================
// [Forward Declarations]
// ============================================================================

struct RoundF;
struct RoundD;

// ============================================================================
// [Fog::RoundF]
// ============================================================================

//! @brief Rounded rectangle (float).
struct FOG_NO_EXPORT RoundF
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  FOG_INLINE RoundF() { reset(); }
  FOG_INLINE RoundF(_Uninitialized) {}

  FOG_INLINE RoundF(const RoundF& other) { rect = other.rect; radius = other.radius; }

  FOG_INLINE RoundF(const RectF& r, const PointF& rp) { rect = r; radius = rp; }
  FOG_INLINE RoundF(const RectF& r, float rad) { rect = r; radius.set(rad, rad); }

  FOG_INLINE RoundF(float rx, float ry, float rw, float rh, float rad) { rect.set(rx, ry, rw, rh); radius.set(rad, rad); }
  FOG_INLINE RoundF(float rx, float ry, float rw, float rh, float radx, float rady) { rect.set(rx, ry, rw, rh); radius.set(radx, rady); }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  FOG_INLINE const RectF& getRect() const { return rect; }
  FOG_INLINE const PointF& getRadius() const { return radius; }

  FOG_INLINE void setRect(const RectF& r) { rect = r; }
  FOG_INLINE void setRadius(const PointF& rp) { radius = rp; }
  FOG_INLINE void setRadius(float rad) { radius.set(rad, rad); }

  FOG_INLINE void setRound(const RoundF& other) { rect = other.rect; radius = other.radius; }

  FOG_INLINE void setRound(const RectF& r, const PointF& rp) { rect = r; radius = rp; }
  FOG_INLINE void setRound(const RectF& r, float rad) { rect = r; radius.set(rad, rad); }

  FOG_INLINE void setRound(float rx, float ry, float rw, float rh, float rad) { rect.set(rx, ry, rw, rh); radius.set(rad, rad); }
  FOG_INLINE void setRound(float rx, float ry, float rw, float rh, float radx, float rady) { rect.set(rx, ry, rw, rh); radius.set(radx, rady); }

  // --------------------------------------------------------------------------
  // [Reset]
  // --------------------------------------------------------------------------

  FOG_INLINE void reset()
  {
    rect.reset();
    radius.reset();
  }

  // --------------------------------------------------------------------------
  // [BoundingRect]
  // --------------------------------------------------------------------------

  FOG_INLINE RectF getBoundingRect() const
  {
    return rect;
  }

  // --------------------------------------------------------------------------
  // [Transform]
  // --------------------------------------------------------------------------

  FOG_INLINE void translate(const PointF& pt)
  {
    rect.translate(pt);
  }

  // --------------------------------------------------------------------------
  // [Convert]
  // --------------------------------------------------------------------------

  FOG_INLINE RoundD toRoundD() const;

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

  FOG_INLINE RoundF& operator=(const RoundF& other)
  {
    setRound(other);
    return *this;
  }

  FOG_INLINE bool operator==(const RoundF& other)
  {
    return rect == other.rect && radius == other.radius;
  }

  FOG_INLINE bool operator!=(const RoundF& other)
  {
    return rect != other.rect || radius != other.radius;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  RectF rect;
  PointF radius;
};

// ============================================================================
// [Fog::RoundD]
// ============================================================================

//! @brief Rounded rectangle (double).
struct FOG_NO_EXPORT RoundD
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  FOG_INLINE RoundD() { reset(); }
  FOG_INLINE RoundD(_Uninitialized) {}

  FOG_INLINE RoundD(const RoundD& other) { rect = other.rect; radius = other.radius; }

  FOG_INLINE RoundD(const RectD& r, const PointD& rp) { rect = r; radius = rp; }
  FOG_INLINE RoundD(const RectD& r, double rad) { rect = r; radius.set(rad, rad); }

  FOG_INLINE RoundD(double rx, double ry, double rw, double rh, double rad) { rect.set(rx, ry, rw, rh); radius.set(rad, rad); }
  FOG_INLINE RoundD(double rx, double ry, double rw, double rh, double radx, double rady) { rect.set(rx, ry, rw, rh); radius.set(radx, rady); }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  FOG_INLINE const RectD& getRect() const { return rect; }
  FOG_INLINE const PointD& getRadius() const { return radius; }

  FOG_INLINE void setRect(const RectD& r) { rect = r; }
  FOG_INLINE void setRadius(const PointD& rp) { radius = rp; }
  FOG_INLINE void setRadius(double rad) { radius.set(rad, rad); }

  FOG_INLINE void setRound(const RoundD& other) { rect = other.rect; radius = other.radius; }

  FOG_INLINE void setRound(const RectD& r, const PointD& rp) { rect = r; radius = rp; }
  FOG_INLINE void setRound(const RectD& r, double rad) { rect = r; radius.set(rad, rad); }

  FOG_INLINE void setRound(double rx, double ry, double rw, double rh, double rad) { rect.set(rx, ry, rw, rh); radius.set(rad, rad); }
  FOG_INLINE void setRound(double rx, double ry, double rw, double rh, double radx, double rady) { rect.set(rx, ry, rw, rh); radius.set(radx, rady); }

  // --------------------------------------------------------------------------
  // [Reset]
  // --------------------------------------------------------------------------

  FOG_INLINE void reset()
  {
    rect.reset();
    radius.reset();
  }

  // --------------------------------------------------------------------------
  // [BoundingRect]
  // --------------------------------------------------------------------------

  FOG_INLINE RectD getBoundingRect() const
  {
    return rect;
  }

  // --------------------------------------------------------------------------
  // [Transform]
  // --------------------------------------------------------------------------

  FOG_INLINE void translate(const PointD& pt)
  {
    rect.translate(pt);
  }

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

  FOG_INLINE RoundD& operator=(const RoundD& other)
  {
    setRound(other);
    return *this;
  }

  FOG_INLINE bool operator==(const RoundD& other)
  {
    return rect == other.rect && radius == other.radius;
  }

  FOG_INLINE bool operator!=(const RoundD& other)
  {
    return rect != other.rect || radius != other.radius;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  RectD rect;
  PointD radius;
};

// ============================================================================
// [Implemented-Later]
// ============================================================================

FOG_INLINE RoundD RoundF::toRoundD() const { return RoundD(rect.toRectD(), radius.toPointD()); }

// ============================================================================
// [Fog::RoundT<>]
// ============================================================================

FOG_TYPEVARIANT_DECLARE_F_D(Round)

//! @}

} // Fog namespace

// ============================================================================
// [Fog::TypeInfo<>]
// ============================================================================

FOG_DECLARE_TYPEINFO(Fog::RoundF, Fog::TYPEINFO_PRIMITIVE)
FOG_DECLARE_TYPEINFO(Fog::RoundD, Fog::TYPEINFO_PRIMITIVE)

// ============================================================================
// [Fog::Fuzzy<>]
// ============================================================================

FOG_DECLARE_FUZZY(Fog::RoundF, Math::feqv((const float *)&a, (const float *)&b, 6))
FOG_DECLARE_FUZZY(Fog::RoundD, Math::feqv((const double*)&a, (const double*)&b, 6))

// [Guard]
#endif // _FOG_G2D_GEOMETRY_ROUND_H

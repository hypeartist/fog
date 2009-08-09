// [Fog/Svg Library - C++ API]
//
// [Licence]
// MIT, See COPYING file in package

// [Guard]
#ifndef _FOG_SVG_SVGUTIL_H
#define _FOG_SVG_SVGUTIL_H

// [Dependencies]
#include <Fog/Core/Char.h>
#include <Fog/Core/Vector.h>
#include <Fog/Graphics/Rgba.h>
#include <Fog/Graphics/Path.h>
#include <Fog/Svg/SvgDom.h>

namespace Fog {

//! @addtogroup Fog_Svg
//! @{

// ============================================================================
// [Svg::Util]
// ============================================================================

namespace SvgUtil {

FOG_API Rgba parseColor(const String32& str);
FOG_API SvgCoord parseCoord(const String32& str);

FOG_API Vector<SvgStyleItem> parseStyles(const String32& str);
FOG_API String32 joinStyles(const Vector<SvgStyleItem>& items);

FOG_API Path parsePoints(const String32& str);
FOG_API Path parsePath(const String32& str);

} // SvgUtil namespace

//! @}

} // Fog namespace

// [Guard]
#endif // _FOG_SVG_SVGUTIL_H
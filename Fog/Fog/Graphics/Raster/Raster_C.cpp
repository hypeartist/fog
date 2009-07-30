// [Fog/Graphics Library - C++ API]
//
// [Licence] 
// MIT, See COPYING file in package

// [Precompiled headers]
#ifdef FOG_PRECOMP
#include FOG_PRECOMP
#endif

// [Dependencies]
#include <Fog/Core/Math.h>
#include <Fog/Core/Misc.h>
#include <Fog/Graphics/ColorLut.h>
#include <Fog/Graphics/ColorMatrix.h>
#include <Fog/Graphics/DitherMatrix.h>
#include <Fog/Graphics/Error.h>
#include <Fog/Graphics/Image.h>
#include <Fog/Graphics/ImageFilter.h>
#include <Fog/Graphics/Pattern.h>
#include <Fog/Graphics/Raster.h>
#include <Fog/Graphics/Raster/Raster_C.h>
#include <Fog/Graphics/RgbaAnalyzer.h>

// [Raster_C]
#include <Fog/Graphics/Raster/Raster_C_base.cpp.h>
#include <Fog/Graphics/Raster/Raster_C_convert.cpp.h>
#include <Fog/Graphics/Raster/Raster_C_composite.cpp.h>
#include <Fog/Graphics/Raster/Raster_C_gradient.cpp.h>
#include <Fog/Graphics/Raster/Raster_C_pattern.cpp.h>
#include <Fog/Graphics/Raster/Raster_C_filters.cpp.h>

// ============================================================================
// [Library Initializers]
// ============================================================================

#define TODO_NOT_IMPLEMENTED NULL

static void fog_raster_set_nops(Fog::Raster::FunctionMap::RasterFuncs* ops)
{
  using namespace Fog;
  using namespace Fog::Raster;

  ops->pixel = raster_pixel_nop;
  ops->pixel_a8 = raster_pixel_a8_nop;
  ops->span_solid = raster_span_solid_nop;
  ops->span_solid_a8 = raster_span_solid_a8_nop;

  ops->span_composite[Image::FormatARGB32] = raster_span_composite_nop;
  ops->span_composite[Image::FormatPRGB32] = raster_span_composite_nop;
  ops->span_composite[Image::FormatRGB32] = raster_span_composite_nop;
  ops->span_composite[Image::FormatRGB24] = raster_span_composite_nop;
  ops->span_composite[Image::FormatA8] = raster_span_composite_nop;
  ops->span_composite_indexed[Image::FormatI8] = raster_span_composite_indexed_nop;

  ops->span_composite_a8[Image::FormatARGB32] = raster_span_composite_a8_nop;
  ops->span_composite_a8[Image::FormatPRGB32] = raster_span_composite_a8_nop;
  ops->span_composite_a8[Image::FormatRGB32] = raster_span_composite_a8_nop;
  ops->span_composite_a8[Image::FormatRGB24] = raster_span_composite_a8_nop;
  ops->span_composite_a8[Image::FormatA8] = raster_span_composite_a8_nop;
  ops->span_composite_indexed_a8[Image::FormatI8] = raster_span_composite_indexed_a8_nop;
}

FOG_INIT_DECLARE void fog_raster_init_c(void)
{
  using namespace Fog;
  using namespace Fog::Raster;

  FunctionMap* m = functionMap;

  // [Convert - ByteSwap]

  m->convert.bswap16 = convert_bswap16;
  m->convert.bswap24 = convert_bswap24;
  m->convert.bswap32 = convert_bswap32;

  // [Convert - MemCpy]

  m->convert.memcpy8 = convert_memcpy8;
  m->convert.memcpy16 = convert_memcpy16;
  m->convert.memcpy24 = convert_memcpy24;
  m->convert.memcpy32 = convert_memcpy32;

  // [Convert - Argb32 Dest]

  m->convert.argb32_from_prgb32 = convert_argb32_from_prgb32;
  m->convert.argb32_from_prgb32_bs = convert_argb32_from_prgb32_bs;
  m->convert.argb32_from_rgb32 = convert_argb32_from_rgb32;
  m->convert.argb32_from_rgb32_bs = convert_argb32_from_rgb32_bs;

  m->convert.argb32_bs_from_rgb32 = convert_argb32_bs_from_rgb32;
  m->convert.argb32_bs_from_prgb32 = convert_argb32_bs_from_prgb32;

  // [Convert - Prgb32 Dest]

  m->convert.prgb32_from_argb32 = convert_prgb32_from_argb32;
  m->convert.prgb32_from_argb32_bs = convert_prgb32_from_argb32_bs;
  m->convert.prgb32_from_i8 = convert_prgb32_from_i8;

  m->convert.prgb32_bs_from_argb32 = convert_prgb32_bs_from_argb32;
  m->convert.prgb32_bs_from_i8 = convert_prgb32_bs_from_i8;

  // [Convert - Rgb32 Dest]

  m->convert.rgb32_from_argb32 = convert_rgb32_from_argb32;
  m->convert.rgb32_from_argb32_bs = convert_rgb32_from_argb32_bs;
  m->convert.rgb32_from_rgb24 = convert_rgb32_from_rgb24;
  m->convert.rgb32_from_bgr24 = convert_rgb32_from_bgr24;
  m->convert.rgb32_from_rgb16_5550 = convert_rgb32_from_rgb16_5550;
  m->convert.rgb32_from_rgb16_5550_bs = convert_rgb32_from_rgb16_5550_bs;
  m->convert.rgb32_from_rgb16_5650 = convert_rgb32_from_rgb16_5650;
  m->convert.rgb32_from_rgb16_5650_bs = convert_rgb32_from_rgb16_5650_bs;
  m->convert.rgb32_from_i8 = convert_rgb32_from_i8;

  m->convert.rgb32_bs_from_rgb24 = convert_rgb32_bs_from_rgb24;

  // [Convert - Rgb24/Bgr24 Dest]

  m->convert.rgb24_from_rgb32 = convert_rgb24_from_rgb32;
  m->convert.rgb24_from_rgb32_bs = convert_rgb24_from_rgb32_bs;
  m->convert.rgb24_from_rgb16_5550 = convert_rgb24_from_rgb16_5550;
  m->convert.rgb24_from_rgb16_5550_bs = convert_rgb24_from_rgb16_5550_bs;
  m->convert.rgb24_from_rgb16_5650 = convert_rgb24_from_rgb16_5650;
  m->convert.rgb24_from_rgb16_5650_bs = convert_rgb24_from_rgb16_5650_bs;
  m->convert.rgb24_from_i8 = convert_rgb24_from_i8;

  m->convert.bgr24_from_rgb32 = convert_bgr24_from_rgb32;
  m->convert.bgr24_from_i8 = convert_bgr24_from_i8;

  // [Convert - Rgb16 Dest]

  m->convert.rgb16_5550_from_rgb32 = convert_rgb16_5550_from_rgb32;
  m->convert.rgb16_5550_from_rgb24 = convert_rgb16_5550_from_rgb24;
  m->convert.rgb16_5550_from_i8 = convert_rgb16_5550_from_i8;

  m->convert.rgb16_5650_from_rgb32 = convert_rgb16_5650_from_rgb32;
  m->convert.rgb16_5650_from_rgb24 = convert_rgb16_5650_from_rgb24;
  m->convert.rgb16_5650_from_i8 = convert_rgb16_5650_from_i8;

  m->convert.rgb16_5550_bs_from_rgb32 = convert_rgb16_5550_bs_from_rgb32;
  m->convert.rgb16_5550_bs_from_rgb24 = convert_rgb16_5550_bs_from_rgb24;
  m->convert.rgb16_5550_bs_from_i8 = convert_rgb16_5550_bs_from_i8;

  m->convert.rgb16_5650_bs_from_rgb32 = convert_rgb16_5650_bs_from_rgb32;
  m->convert.rgb16_5650_bs_from_rgb24 = convert_rgb16_5650_bs_from_rgb24;
  m->convert.rgb16_5650_bs_from_i8 = convert_rgb16_5650_bs_from_i8;

  // [Convert - Greyscale]

  m->convert.greyscale8_from_rgb32 = convert_greyscale8_from_rgb32;
  m->convert.greyscale8_from_rgb24 = convert_greyscale8_from_rgb24;
  m->convert.greyscale8_from_i8 = convert_greyscale8_from_i8;

  m->convert.rgb32_from_greyscale8 = convert_rgb32_from_greyscale8;
  m->convert.rgb24_from_greyscale8 = convert_rgb24_from_greyscale8;

  // [Convert - Dithering]

  m->convert.i8rgb232_from_rgb32_dither = convert_i8rgb232_from_rgb32_dither;
  m->convert.i8rgb222_from_rgb32_dither = convert_i8rgb222_from_rgb32_dither;
  m->convert.i8rgb111_from_rgb32_dither = convert_i8rgb111_from_rgb32_dither;

  m->convert.i8rgb232_from_rgb24_dither = convert_i8rgb232_from_rgb24_dither;
  m->convert.i8rgb222_from_rgb24_dither = convert_i8rgb222_from_rgb24_dither;
  m->convert.i8rgb111_from_rgb24_dither = convert_i8rgb111_from_rgb24_dither;

  m->convert.rgb16_5550_from_rgb32_dither = convert_rgb16_5550_from_rgb32_dither;
  m->convert.rgb16_5550_from_rgb24_dither = convert_rgb16_5550_from_rgb24_dither;

  m->convert.rgb16_5650_from_rgb32_dither = convert_rgb16_5650_from_rgb32_dither;
  m->convert.rgb16_5650_from_rgb24_dither = convert_rgb16_5650_from_rgb24_dither;

  m->convert.rgb16_5550_bs_from_rgb32_dither = convert_rgb16_5550_bs_from_rgb32_dither;
  m->convert.rgb16_5550_bs_from_rgb24_dither = convert_rgb16_5550_bs_from_rgb24_dither;

  m->convert.rgb16_5650_bs_from_rgb32_dither = convert_rgb16_5650_bs_from_rgb32_dither;
  m->convert.rgb16_5650_bs_from_rgb24_dither = convert_rgb16_5650_bs_from_rgb24_dither;

  // [Gradient - Gradient]

  m->gradient.gradient_argb32 = gradient_gradient_argb32;
  m->gradient.gradient_prgb32 = gradient_gradient_prgb32;
  m->gradient.gradient_rgb32 = gradient_gradient_rgb32;
  m->gradient.gradient_rgb24 = gradient_gradient_rgb24;
  m->gradient.gradient_a8 = gradient_gradient_a8;

  // [Pattern - Texture]

  m->pattern.texture_init = pattern_texture_init;
  m->pattern.texture_fetch_repeat = pattern_texture_fetch_repeat;
  m->pattern.texture_fetch_reflect = pattern_texture_fetch_reflect;

  // [Pattern - Linear Gradient]

  m->pattern.linear_gradient_init = pattern_linear_gradient_init;
  m->pattern.linear_gradient_fetch_pad = pattern_linear_gradient_fetch_pad;
  m->pattern.linear_gradient_fetch_repeat = pattern_linear_gradient_fetch_repeat;

  // [Pattern - Radial Gradient]

  m->pattern.radial_gradient_init = pattern_radial_gradient_init;
  m->pattern.radial_gradient_fetch_pad = pattern_radial_gradient_fetch_pad;
  m->pattern.radial_gradient_fetch_repeat = pattern_radial_gradient_fetch_repeat;

  // [Pattern - Conical Gradient]

  m->pattern.conical_gradient_init = pattern_conical_gradient_init;
  m->pattern.conical_gradient_fetch = pattern_conical_gradient_fetch;

  // [Raster - CompositeSrc]

  // TODO

  // [Raster - CompositeDest (NOP)]

  fog_raster_set_nops(&m->raster[Image::FormatARGB32][CompositeDest]);
  fog_raster_set_nops(&m->raster[Image::FormatPRGB32][CompositeDest]);
  fog_raster_set_nops(&m->raster[Image::FormatRGB32][CompositeDest]);
  fog_raster_set_nops(&m->raster[Image::FormatRGB24][CompositeDest]);
  fog_raster_set_nops(&m->raster[Image::FormatA8][CompositeDest]);
  fog_raster_set_nops(&m->raster[Image::FormatI8][CompositeDest]);

  // [Raster - CompositeDest]

  // TODO

  // [Raster - CompositeSrcOver]

  // TODO

  // [Raster - CompositeDestOver]

  fog_raster_set_nops(&m->raster[Image::FormatRGB32][CompositeDest]);
  fog_raster_set_nops(&m->raster[Image::FormatRGB24][CompositeDest]);

  // TODO

  // [Raster - CompositeSrcIn]

  // TODO

  // [Raster - CompositeDestIn]

  // TODO

  // [Raster - CompositeSrcOut]

  // TODO

  // [Raster - CompositeDestOut]

  // TODO

  // [Raster - CompositeSrcAtop]

  // TODO

  // [Raster - CompositeDestAtop]

  // TODO

  // [Raster - CompositeXor]

  // TODO

  // [Raster - CompositeClear]

  // TODO

  // [Raster - CompositeAdd]

  // TODO

  // [Raster - CompositeSubtract]

  // TODO

  // [Raster - CompositeMultiply]

  // TODO

  // [Raster - CompositeScreen]

  // TODO

  // [Raster - CompositeDarken]

  // TODO

  // [Raster - CompositeLighten]

  // TODO

  // [Raster - CompositeDifference]

  // TODO

  // [Raster - CompositeExclusion]

  // TODO

  // [Raster - CompositeInvert]

  // TODO

  // [Raster - CompositeInvertRgb]

  // TODO

#if 0
  // [Raster]

  // TODO: Write correct versions and remove

#define SET_RASTER_ARGB32(DST_ID, DST_CLASS, SRC_SOLID_CLASS, OP_ID, OP_CLASS) \
  m->raster_argb32[DST_ID][OP_ID].pixel = raster_pixel< OP_CLASS<DST_CLASS, SRC_SOLID_CLASS> >; \
  m->raster_argb32[DST_ID][OP_ID].pixel_a8 = raster_pixel_a8< OP_CLASS<DST_CLASS, SRC_SOLID_CLASS> >; \
  \
  m->raster_argb32[DST_ID][OP_ID].span_solid = raster_span_solid< OP_CLASS<DST_CLASS, SRC_SOLID_CLASS> >; \
  m->raster_argb32[DST_ID][OP_ID].span_solid_a8 = raster_span_solid_a8< OP_CLASS<DST_CLASS, SRC_SOLID_CLASS> >; \
  \
  m->raster_argb32[DST_ID][OP_ID].span_composite   [Image::FormatARGB32] = raster_span_composite   < OP_CLASS<DST_CLASS, PixFmt_ARGB32> >; \
  m->raster_argb32[DST_ID][OP_ID].span_composite_a8[Image::FormatARGB32] = raster_span_composite_a8< OP_CLASS<DST_CLASS, PixFmt_ARGB32> >; \
  \
  m->raster_argb32[DST_ID][OP_ID].span_composite   [Image::FormatPRGB32] = raster_span_composite   < OP_CLASS<DST_CLASS, PixFmt_PRGB32> >; \
  m->raster_argb32[DST_ID][OP_ID].span_composite_a8[Image::FormatPRGB32] = raster_span_composite_a8< OP_CLASS<DST_CLASS, PixFmt_PRGB32> >; \
  \
  m->raster_argb32[DST_ID][OP_ID].span_composite   [Image::FormatRGB32] = raster_span_composite   < OP_CLASS<DST_CLASS, PixFmt_RGB32> >; \
  m->raster_argb32[DST_ID][OP_ID].span_composite_a8[Image::FormatRGB32] = raster_span_composite_a8< OP_CLASS<DST_CLASS, PixFmt_RGB32> >; \
  \
  m->raster_argb32[DST_ID][OP_ID].span_composite   [Image::FormatRGB24] = raster_span_composite   < OP_CLASS<DST_CLASS, PixFmt_RGB24> >; \
  m->raster_argb32[DST_ID][OP_ID].span_composite_a8[Image::FormatRGB24] = raster_span_composite_a8< OP_CLASS<DST_CLASS, PixFmt_RGB24> >

  // [Raster - Argb32 / Prgb32]
#if 0
  SET_RASTER_ARGB32(0, PixFmt_ARGB32, PixFmt_ARGB32, CompositeSrc, Operator_Src);
  SET_RASTER_ARGB32(0, PixFmt_ARGB32, PixFmt_ARGB32, CompositeSrcOver, Operator_Over);
  SET_RASTER_ARGB32(0, PixFmt_ARGB32, PixFmt_ARGB32, CompositeDestOver, Operator_OverReverse);
  SET_RASTER_ARGB32(0, PixFmt_ARGB32, PixFmt_ARGB32, CompositeSrcIn, Operator_In);
  SET_RASTER_ARGB32(0, PixFmt_ARGB32, PixFmt_ARGB32, CompositeDestIn, Operator_InReverse);
  SET_RASTER_ARGB32(0, PixFmt_ARGB32, PixFmt_ARGB32, CompositeSrcOut, Operator_Out);
  SET_RASTER_ARGB32(0, PixFmt_ARGB32, PixFmt_ARGB32, CompositeDestOut, Operator_OutReverse);
  SET_RASTER_ARGB32(0, PixFmt_ARGB32, PixFmt_ARGB32, CompositeSrcAtop, Operator_Atop);
  SET_RASTER_ARGB32(0, PixFmt_ARGB32, PixFmt_ARGB32, CompositeDestAtop, Operator_AtopReverse);
  SET_RASTER_ARGB32(0, PixFmt_ARGB32, PixFmt_ARGB32, CompositeXor, Operator_Xor);
  SET_RASTER_ARGB32(0, PixFmt_ARGB32, PixFmt_ARGB32, CompositeClear, Operator_Clear);

  SET_RASTER_ARGB32(1, PixFmt_PRGB32, PixFmt_PRGB32, CompositeSrc, Operator_Src);
  SET_RASTER_ARGB32(1, PixFmt_PRGB32, PixFmt_PRGB32, CompositeSrcOver, Operator_Over);
  SET_RASTER_ARGB32(1, PixFmt_PRGB32, PixFmt_PRGB32, CompositeDestOver, Operator_OverReverse);
  SET_RASTER_ARGB32(1, PixFmt_PRGB32, PixFmt_PRGB32, CompositeSrcIn, Operator_In);
  SET_RASTER_ARGB32(1, PixFmt_PRGB32, PixFmt_PRGB32, CompositeDestIn, Operator_InReverse);
  SET_RASTER_ARGB32(1, PixFmt_PRGB32, PixFmt_PRGB32, CompositeSrcOut, Operator_Out);
  SET_RASTER_ARGB32(1, PixFmt_PRGB32, PixFmt_PRGB32, CompositeDestOut, Operator_OutReverse);
  SET_RASTER_ARGB32(1, PixFmt_PRGB32, PixFmt_PRGB32, CompositeSrcAtop, Operator_Atop);
  SET_RASTER_ARGB32(1, PixFmt_PRGB32, PixFmt_PRGB32, CompositeDestAtop, Operator_AtopReverse);
  SET_RASTER_ARGB32(1, PixFmt_PRGB32, PixFmt_PRGB32, CompositeXor, Operator_Xor);
  SET_RASTER_ARGB32(1, PixFmt_PRGB32, PixFmt_PRGB32, CompositeClear, Operator_Clear);

  SET_RASTER_ARGB32(0, PixFmt_ARGB32, PixFmt_ARGB32, CompositeAdd, Operator_Add);
  SET_RASTER_ARGB32(0, PixFmt_ARGB32, PixFmt_ARGB32, CompositeSubtract, Operator_Subtract);
  SET_RASTER_ARGB32(0, PixFmt_ARGB32, PixFmt_ARGB32, CompositeMultiply, Operator_Multiply);
  SET_RASTER_ARGB32(0, PixFmt_ARGB32, PixFmt_ARGB32, CompositeScreen, Operator_Screen);
  SET_RASTER_ARGB32(0, PixFmt_ARGB32, PixFmt_ARGB32, CompositeDarken, Operator_Darken);
  SET_RASTER_ARGB32(0, PixFmt_ARGB32, PixFmt_ARGB32, CompositeLighten, Operator_Lighten);
  SET_RASTER_ARGB32(0, PixFmt_ARGB32, PixFmt_ARGB32, CompositeDifference, Operator_Difference);
  SET_RASTER_ARGB32(0, PixFmt_ARGB32, PixFmt_ARGB32, CompositeExclusion, Operator_Exclusion);
  SET_RASTER_ARGB32(0, PixFmt_ARGB32, PixFmt_ARGB32, CompositeInvert, Operator_Invert);
  SET_RASTER_ARGB32(0, PixFmt_ARGB32, PixFmt_ARGB32, CompositeInvertRgb, Operator_InvertRgb);

  SET_RASTER_ARGB32(1, PixFmt_PRGB32, PixFmt_PRGB32, CompositeAdd, Operator_Add);
  SET_RASTER_ARGB32(1, PixFmt_PRGB32, PixFmt_PRGB32, CompositeSubtract, Operator_Subtract);
  SET_RASTER_ARGB32(1, PixFmt_PRGB32, PixFmt_PRGB32, CompositeMultiply, Operator_Multiply);
  SET_RASTER_ARGB32(1, PixFmt_PRGB32, PixFmt_PRGB32, CompositeScreen, Operator_Screen);
  SET_RASTER_ARGB32(1, PixFmt_PRGB32, PixFmt_PRGB32, CompositeDarken, Operator_Darken);
  SET_RASTER_ARGB32(1, PixFmt_PRGB32, PixFmt_PRGB32, CompositeLighten, Operator_Lighten);
  SET_RASTER_ARGB32(1, PixFmt_PRGB32, PixFmt_PRGB32, CompositeDifference, Operator_Difference);
  SET_RASTER_ARGB32(1, PixFmt_PRGB32, PixFmt_PRGB32, CompositeExclusion, Operator_Exclusion);
  SET_RASTER_ARGB32(1, PixFmt_PRGB32, PixFmt_PRGB32, CompositeInvert, Operator_Invert);
  SET_RASTER_ARGB32(1, PixFmt_PRGB32, PixFmt_PRGB32, CompositeInvertRgb, Operator_InvertRgb);
#endif
#undef SET_RASTER_ARGB32

  // [Raster - Argb32 / Prgb32 - SrcOver]

  // [Raster - Argb32 / Prgb32 - NOP]

  for (i = 0; i < 2; i++)
  {
    m->raster_argb32[i][CompositeDest].pixel = raster_pixel_nop;
    m->raster_argb32[i][CompositeDest].pixel_a8 = raster_pixel_a8_nop;
    m->raster_argb32[i][CompositeDest].span_solid = raster_span_solid_nop;
    m->raster_argb32[i][CompositeDest].span_solid_a8 = raster_span_solid_a8_nop;

    m->raster_argb32[i][CompositeDest].span_composite[Image::FormatARGB32] = raster_span_composite_nop;
    m->raster_argb32[i][CompositeDest].span_composite[Image::FormatPRGB32] = raster_span_composite_nop;
    m->raster_argb32[i][CompositeDest].span_composite[Image::FormatRGB32] = raster_span_composite_nop;
    m->raster_argb32[i][CompositeDest].span_composite[Image::FormatRGB24] = raster_span_composite_nop;
    m->raster_argb32[i][CompositeDest].span_composite[Image::FormatA8] = raster_span_composite_nop;
    m->raster_argb32[i][CompositeDest].span_composite_indexed[Image::FormatI8] = raster_span_composite_indexed_nop;

    m->raster_argb32[i][CompositeDest].span_composite_a8[Image::FormatARGB32] = raster_span_composite_a8_nop;
    m->raster_argb32[i][CompositeDest].span_composite_a8[Image::FormatPRGB32] = raster_span_composite_a8_nop;
    m->raster_argb32[i][CompositeDest].span_composite_a8[Image::FormatRGB32] = raster_span_composite_a8_nop;
    m->raster_argb32[i][CompositeDest].span_composite_a8[Image::FormatRGB24] = raster_span_composite_a8_nop;
    m->raster_argb32[i][CompositeDest].span_composite_a8[Image::FormatA8] = raster_span_composite_a8_nop;
    m->raster_argb32[i][CompositeDest].span_composite_indexed_a8[Image::FormatI8] = raster_span_composite_indexed_a8_nop;
  }

  // [Raster - Rgb32]
#if 0
  m->raster_rgb32.pixel = raster_rgb32_pixel;
  m->raster_rgb32.pixel_a8 = raster_rgb32_pixel_a8;
  m->raster_rgb32.span_solid = raster_rgb32_span_solid;
  m->raster_rgb32.span_solid_a8 = raster_rgb32_span_solid_a8;

  m->raster_rgb32.span_composite[Image::FormatARGB32] = raster_rgb32_span_composite_argb32;
  m->raster_rgb32.span_composite[Image::FormatPRGB32] = raster_rgb32_span_composite_prgb32;
  m->raster_rgb32.span_composite[Image::FormatRGB32] = convert_memcpy32;
  m->raster_rgb32.span_composite[Image::FormatRGB24] = convert_rgb32_from_rgb24;
  m->raster_rgb32.span_composite[Image::FormatA8] = raster_span_composite_nop;
  m->raster_rgb32.span_composite_indexed[Image::FormatI8] = raster_rgb32_span_composite_indexed;

  m->raster_rgb32.span_composite_a8[Image::FormatARGB32] = raster_rgb32_span_composite_argb32_a8;
  m->raster_rgb32.span_composite_a8[Image::FormatPRGB32] = raster_rgb32_span_composite_prgb32_a8;
  m->raster_rgb32.span_composite_a8[Image::FormatRGB32] = raster_rgb32_span_composite_rgb32_a8;
  m->raster_rgb32.span_composite_a8[Image::FormatRGB24] = raster_rgb32_span_composite_rgb24_a8;
  m->raster_rgb32.span_composite_a8[Image::FormatA8] = raster_span_composite_a8_nop;
  m->raster_rgb32.span_composite_indexed_a8[Image::FormatI8] = raster_rgb32_span_composite_indexed_a8;
#endif
  // [Raster - Rgb24]
#if 0
  m->raster_rgb24.pixel = raster_rgb24_pixel;
  m->raster_rgb24.pixel_a8 = raster_rgb24_pixel_a8;
  m->raster_rgb24.span_solid = raster_rgb24_span_solid;
  m->raster_rgb24.span_solid_a8 = raster_rgb24_span_solid_a8;

  m->raster_rgb24.span_composite[Image::FormatARGB32] = raster_rgb24_span_composite_argb32;
  m->raster_rgb24.span_composite[Image::FormatPRGB32] = raster_rgb24_span_composite_prgb32;
  m->raster_rgb24.span_composite[Image::FormatRGB32] = convert_rgb24_from_rgb32;
  m->raster_rgb24.span_composite[Image::FormatRGB24] = convert_memcpy24;
  m->raster_rgb24.span_composite[Image::FormatA8] = raster_span_composite_nop;
  m->raster_rgb24.span_composite_indexed[Image::FormatI8] = TODO_NOT_IMPLEMENTED;

  m->raster_rgb24.span_composite_a8[Image::FormatARGB32] = raster_rgb24_span_composite_argb32_a8;
  m->raster_rgb24.span_composite_a8[Image::FormatPRGB32] = raster_rgb24_span_composite_prgb32_a8;
  m->raster_rgb24.span_composite_a8[Image::FormatRGB32] = raster_rgb24_span_composite_rgb32_a8;
  m->raster_rgb24.span_composite_a8[Image::FormatRGB24] = raster_rgb24_span_composite_rgb24_a8;
  m->raster_rgb24.span_composite_a8[Image::FormatA8] = raster_span_composite_a8_nop;
  m->raster_rgb24.span_composite_indexed_a8[Image::FormatI8] = TODO_NOT_IMPLEMENTED;
#endif

#endif

  // [Filters - ColorLut]

  m->filters.colorLut[Image::FormatARGB32] = colorLut_argb32;
  m->filters.colorLut[Image::FormatPRGB32] = colorLut_prgb32;
  m->filters.colorLut[Image::FormatRGB32] = colorLut_rgb32;
  m->filters.colorLut[Image::FormatRGB24] = colorLut_rgb24;
  m->filters.colorLut[Image::FormatA8] = colorLut_a8;

  // [Filters - ColorMatrix]

  m->filters.colorMatrix[Image::FormatARGB32] = colorMatrix_argb32;
  m->filters.colorMatrix[Image::FormatPRGB32] = colorMatrix_prgb32;
  m->filters.colorMatrix[Image::FormatRGB32] = colorMatrix_rgb32;
  m->filters.colorMatrix[Image::FormatRGB24] = colorMatrix_rgb24;
  m->filters.colorMatrix[Image::FormatA8] = colorMatrix_a8;

  // [Filters - CopyArea]

  m->filters.copyArea[Image::FormatARGB32] = copyArea_32;
  m->filters.copyArea[Image::FormatPRGB32] = copyArea_32;
  m->filters.copyArea[Image::FormatRGB32] = copyArea_32;
  m->filters.copyArea[Image::FormatRGB24] = copyArea_24;
  m->filters.copyArea[Image::FormatA8] = copyArea_8;
  m->filters.copyArea[Image::FormatI8] = copyArea_8;

  // [Filters - FloatScanlineConvolve]

  m->filters.floatScanlineConvolveH[Image::FormatARGB32] = floatScanlineConvolveH_argb32;
  m->filters.floatScanlineConvolveV[Image::FormatARGB32] = floatScanlineConvolveV_argb32;
  m->filters.floatScanlineConvolveH[Image::FormatRGB32] = floatScanlineConvolveH_argb32;
  m->filters.floatScanlineConvolveV[Image::FormatRGB32] = floatScanlineConvolveV_argb32;
  m->filters.floatScanlineConvolveH[Image::FormatRGB24] = floatScanlineConvolveH_rgb24;
  m->filters.floatScanlineConvolveV[Image::FormatRGB24] = floatScanlineConvolveV_rgb24;
  m->filters.floatScanlineConvolveH[Image::FormatA8] = floatScanlineConvolveH_a8;
  m->filters.floatScanlineConvolveV[Image::FormatA8] = floatScanlineConvolveV_a8;

  // [Filters - BoxBlurConvolve]

  m->filters.boxBlurConvolveH[Image::FormatARGB32] = boxBlurConvolveH_argb32;
  m->filters.boxBlurConvolveV[Image::FormatARGB32] = boxBlurConvolveV_argb32;
  m->filters.boxBlurConvolveH[Image::FormatRGB32] = boxBlurConvolveH_argb32;
  m->filters.boxBlurConvolveV[Image::FormatRGB32] = boxBlurConvolveV_argb32;
  m->filters.boxBlurConvolveH[Image::FormatRGB24] = boxBlurConvolveH_rgb24;
  m->filters.boxBlurConvolveV[Image::FormatRGB24] = boxBlurConvolveV_rgb24;
  m->filters.boxBlurConvolveH[Image::FormatA8] = boxBlurConvolveH_a8;
  m->filters.boxBlurConvolveV[Image::FormatA8] = boxBlurConvolveV_a8;

  // [Filters - StackBlurConvolve]

  m->filters.stackBlur8Mul = stackBlur8Mul;
  m->filters.stackBlur8Shr = stackBlur8Shr;

  m->filters.stackBlurConvolveH[Image::FormatARGB32] = stackBlurConvolveH_argb32;
  m->filters.stackBlurConvolveV[Image::FormatARGB32] = stackBlurConvolveV_argb32;
  m->filters.stackBlurConvolveH[Image::FormatRGB32] = stackBlurConvolveH_argb32;
  m->filters.stackBlurConvolveV[Image::FormatRGB32] = stackBlurConvolveV_argb32;
  m->filters.stackBlurConvolveH[Image::FormatRGB24] = stackBlurConvolveH_rgb24;
  m->filters.stackBlurConvolveV[Image::FormatRGB24] = stackBlurConvolveV_rgb24;
  m->filters.stackBlurConvolveH[Image::FormatA8] = stackBlurConvolveH_a8;
  m->filters.stackBlurConvolveV[Image::FormatA8] = stackBlurConvolveV_a8;
}
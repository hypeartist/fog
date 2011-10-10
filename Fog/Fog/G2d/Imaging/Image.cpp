// [Fog-G2d]
//
// [License]
// MIT, See COPYING file in package

// [Precompiled Headers]
#if defined(FOG_PRECOMP)
#include FOG_PRECOMP
#endif // FOG_PRECOMP

// [Dependencies]
#include <Fog/Core/Global/Global.h>
#include <Fog/Core/Global/Init_p.h>
#include <Fog/Core/Global/Private.h>
#include <Fog/Core/Face/FaceC.h>
#include <Fog/Core/Math/FloatBits.h>
#include <Fog/Core/Math/Math.h>
#include <Fog/Core/Memory/BSwap.h>
#include <Fog/Core/Memory/MemBufferTmp_p.h>
#include <Fog/Core/Memory/MemMgr.h>
#include <Fog/Core/Memory/MemOps.h>
#include <Fog/Core/OS/FilePath.h>
#include <Fog/Core/OS/OSUtil.h>
#include <Fog/Core/Tools/ManagedString.h>
#include <Fog/Core/Tools/Stream.h>
#include <Fog/Core/Tools/StringTmp_p.h>
#include <Fog/G2d/Face/Face_Raster_C.h>
#include <Fog/G2d/Imaging/Image.h>
#include <Fog/G2d/Imaging/ImageBits.h>
#include <Fog/G2d/Imaging/ImageCodec.h>
#include <Fog/G2d/Imaging/ImageCodecProvider.h>
#include <Fog/G2d/Imaging/ImageConverter.h>
#include <Fog/G2d/Imaging/ImageDecoder.h>
#include <Fog/G2d/Imaging/ImageEncoder.h>
#include <Fog/G2d/Imaging/ImageFilter.h>
#include <Fog/G2d/Painting/RasterScanline_p.h>
#include <Fog/G2d/Painting/Rasterizer_p.h>
#include <Fog/G2d/Painting/RasterApi_p.h>
#include <Fog/G2d/Painting/RasterConstants_p.h>
#include <Fog/G2d/Painting/RasterStructs_p.h>
#include <Fog/G2d/Painting/RasterUtil_p.h>
#include <Fog/G2d/Tools/ColorAnalyzer_p.h>
#include <Fog/G2d/Tools/Reduce_p.h>

namespace Fog {

// ============================================================================
// [Fog::Image - Global]
// ============================================================================

static Static<ImageData> Image_dEmpty;
static Static<Image> Image_oEmpty;

// ============================================================================
// [Fog::Image - VTable]
// ============================================================================

static err_t FOG_CDECL Image_Buffer_create(ImageData** pd, const SizeI* size, uint32_t format);
static void  FOG_CDECL Image_Buffer_destroy(ImageData* d);
static void* FOG_CDECL Image_Buffer_getHandle(const ImageData* d);
static err_t FOG_CDECL Image_Buffer_updatePalette(ImageData* d, const Range* range);

static const ImageVTable Image_Buffer_vTable = 
{
  Image_Buffer_create,
  Image_Buffer_destroy,
  Image_Buffer_getHandle,
  Image_Buffer_updatePalette
};

static err_t FOG_CDECL Image_Buffer_create(ImageData** pd, const SizeI* size, uint32_t format)
{
  const ImageFormatDescription& desc = ImageFormatDescription::getByFormat(format);
  ssize_t stride = (size_t)_api.image_getStrideFromWidth(size->w, desc.getDepth());

  if (stride == 0)
    return ERR_RT_INVALID_ARGUMENT;

  if ((uint)size->h > (SIZE_MAX - sizeof(ImageData) - 16) / stride)
    return ERR_RT_OUT_OF_MEMORY;

  size_t dSize = sizeof(ImageData) + 16 + (size_t)stride * (uint)size->h;
  ImageData* d = reinterpret_cast<ImageData*>(MemMgr::alloc(dSize));

  if (FOG_IS_NULL(d))
    return ERR_RT_OUT_OF_MEMORY;

  d->reference.init(1);
  d->vType = VAR_TYPE_IMAGE | VAR_FLAG_NONE;
  d->locked = 0;

  d->vtable = &Image_Buffer_vTable;
  d->size = *size;
  d->format = format;
  d->type = IMAGE_TYPE_BUFFER;
  d->adopted = 0;
  d->colorKey = IMAGE_COLOR_KEY_NONE;
  d->bytesPerPixel = desc.getBytesPerPixel();
  FOG_PADDING_ZERO_64(d->padding);

  d->data = (uint8_t*)( ((size_t)d + sizeof(ImageData) + 15) & ~(size_t)15 );
  d->first = d->data;
  d->stride = stride;

  d->palette.init();

  *pd = d;
  return ERR_OK;
}

static void FOG_CDECL Image_Buffer_destroy(ImageData* d)
{
  d->palette.destroy();
  MemMgr::free(d);
}

static void* FOG_CDECL Image_Buffer_getHandle(const ImageData* d)
{
  FOG_UNUSED(d);
  return NULL;
}

static err_t FOG_CDECL Image_Buffer_updatePalette(ImageData* d, const Range* range)
{
  // Default updatePalette() handler is NOP, because the d->palette is the
  // only palette used by mem-buffered images.
  return ERR_OK;
}

// ============================================================================
// [Fog::Image - Construction / Destruction]
// ============================================================================

static void FOG_CDECL Image_ctor(Image* self)
{
  ImageData* dEmpty = &Image_dEmpty;
  dEmpty->reference.inc();

  self->_d = dEmpty;
}

static void FOG_CDECL Image_ctorCopy(Image* self, const Image* other)
{
  self->_d = other->_d->addRef();
}

static void FOG_CDECL Image_ctorCreate(Image* self, const SizeI* size, uint32_t format, uint32_t type)
{
  ImageData* dEmpty = &Image_dEmpty;
  dEmpty->reference.inc();

  self->_d = dEmpty;
  _api.image_create(self, size, format, type);
}

static void FOG_CDECL Image_dtor(Image* self)
{
  return self->_d->release();
}

// ============================================================================
// [Fog::Image - Sharing]
// ============================================================================

static err_t FOG_CDECL Image_detach(Image* self)
{
  ImageData* d = self->_d;

  if (d == &Image_dEmpty)
    return ERR_OK;

  // Should be catched by Image_dEmpty.
  FOG_ASSERT(d->size.w && d->size.h);
  FOG_ASSERT(d->stride != 0);

  if (self->isDetached())
    return ERR_OK;

  uint32_t type = d->type;
  ImageData* newd;

  FOG_RETURN_ON_ERROR(_api.image_vTable[type]->create(&newd, &d->size, d->format));

  newd->colorKey = d->colorKey;
  newd->palette->setData(d->palette);

  _api_raster.getCopyRectFunc(newd->format)(
    newd->first, newd->stride,
    d->first, d->stride,
    newd->size.w, newd->size.h, NULL);

  atomicPtrXchg(&self->_d, newd)->release();
  return ERR_OK;
}

// ============================================================================
// [Fog::Image - Reset]
// ============================================================================

static void FOG_CDECL Image_reset(Image* self)
{
  ImageData* dEmpty = &Image_dEmpty;
  dEmpty->reference.inc();

  atomicPtrXchg(&self->_d, dEmpty)->release();
}

// ============================================================================
// [Fog::Image - Create / Adopt]
// ============================================================================

static err_t FOG_CDECL Image_create(Image* self, const SizeI* size, uint32_t format, uint32_t type)
{
  err_t err = ERR_OK;
  ImageData* d = self->_d;

  // Detect invalid arguments.
  if (format >= IMAGE_FORMAT_COUNT)
  {
    err = ERR_RT_INVALID_ARGUMENT;
    goto _Fail;
  }

  if (type >= IMAGE_TYPE_COUNT && type != IMAGE_TYPE_IGNORE)
  {
    err = ERR_RT_INVALID_ARGUMENT;
    goto _Fail;
  }

  // Don't create an image with invalid size.
  if ((uint)size->w >= IMAGE_MAX_WIDTH || (uint)size->h >= IMAGE_MAX_HEIGHT)
  {
    err = ERR_IMAGE_INVALID_SIZE;
    goto _Fail;
  }

  // Optimize the case when we do not need to leave the current buffer. In case
  // that the size of this image is the same as the given size, and all other
  // members are equal the the @c Image::create() arguments, then we can simply
  // use the existing buffer instead of performing destroy() / create() call.
  if (d->reference.get() == 1 && 
      d->type == IMAGE_TYPE_BUFFER &&
      d->adopted == 0 &&
      d->stride > 0 &&
      (d->vType & (VAR_FLAG_STATIC | VAR_FLAG_READ_ONLY)) == 0 &&
      (type == IMAGE_TYPE_BUFFER || type == IMAGE_TYPE_IGNORE))
  {
    const ImageFormatDescription& desc = ImageFormatDescription::getByFormat(format);
    size_t newStride = Image::getStrideFromWidth(size->w, desc.getDepth());

    size_t oldSize = (size_t)d->size.w * (size_t)d->size.h * d->stride;
    size_t newSize = (size_t)size->w   * (size_t)size->h   * newStride;

    if (oldSize == newSize)
    {
      d->size = *size;
      d->format = format;
      d->colorKey = IMAGE_COLOR_KEY_NONE;
      d->stride = newStride;
      d->palette->reset();
      return ERR_OK;
    }
  }

  if (type == IMAGE_TYPE_IGNORE)
    type = IMAGE_TYPE_BUFFER;

  if (_api.image_vTable[type] == NULL)
  {
    err = ERR_IMAGE_INVALID_TYPE;
    goto _Fail;
  }

  err = _api.image_vTable[type]->create(&d, size, format);
  if (FOG_IS_ERROR(err))
    goto _Fail;

  atomicPtrXchg(&self->_d, d)->release();
  return ERR_OK;

_Fail:
  Image_dEmpty->reference.inc();
  atomicPtrXchg(&self->_d, &Image_dEmpty)->release();

  return err;
}

static err_t FOG_CDECL Image_adopt(Image* self, const ImageBits* imageBits, uint32_t adoptFlags)
{
  ImageData* d = self->_d;

  if (!imageBits->isValid())
  {
    self->reset();
    return ERR_RT_INVALID_ARGUMENT;
  }

  const ImageFormatDescription& desc = ImageFormatDescription::getByFormat(imageBits->format);

  if (d->reference.get() == 1 &&
      d->type == IMAGE_TYPE_BUFFER &&
      d->adopted == 1 &&
      (d->vType & (VAR_FLAG_STATIC | VAR_FLAG_READ_ONLY)) == 0)
  {
    d->vType &= ~VAR_FLAG_READ_ONLY;
  }
  else
  {
    d = reinterpret_cast<ImageData*>(MemMgr::alloc(sizeof(ImageData)));

    if (FOG_IS_NULL(d))
    {
      self->reset();
      return ERR_RT_OUT_OF_MEMORY;
    }

    d->reference.init(1);
    d->vType = VAR_TYPE_IMAGE | VAR_FLAG_NONE;
    d->locked = 0;

    d->vtable = &Image_Buffer_vTable;
    d->size = imageBits->size;
    d->format = imageBits->format;
    d->type = IMAGE_TYPE_BUFFER;
    d->adopted = 1;
    d->colorKey = IMAGE_COLOR_KEY_NONE;
    d->bytesPerPixel = desc.getBytesPerPixel();
    FOG_PADDING_ZERO_64(d->padding);

    d->palette.init();
    atomicPtrXchg(&self->_d, d)->release();
  }

  d->data = imageBits->data;
  d->first = imageBits->data;
  d->stride = imageBits->stride;

  // Bottom-to-top?
  if (adoptFlags & IMAGE_ADOPT_REVERSED)
  {
    d->stride = -d->stride;
    d->first += (ssize_t)(d->size.h - 1) * d->stride;
  }

  // Read-only?
  if ((adoptFlags & IMAGE_ATOPT_READ_ONLY) != 0)
    d->vType |= VAR_FLAG_READ_ONLY;

  return ERR_OK;
}

// ============================================================================
// [Fog::Image - Alpha Distribution]
// ============================================================================

static uint32_t FOG_CDECL Image_getAlphaDistribution(const Image* self)
{
  ImageData* d = self->_d;

  ColorAnalyzer::AnalyzerFunc analyzer = NULL;
  int aPos = 0;
  int inc = 0;

  switch (d->format)
  {
    case IMAGE_FORMAT_PRGB32:
      analyzer = ColorAnalyzer::analyzeAlpha32; aPos = PIXEL_ARGB32_POS_A; inc = 4;
      break;

    case IMAGE_FORMAT_A8:
      analyzer = ColorAnalyzer::analyzeAlpha32; aPos = 0; inc = 1;
      break;

    case IMAGE_FORMAT_PRGB64:
      analyzer = ColorAnalyzer::analyzeAlpha64; aPos = PIXEL_ARGB64_POS_A; inc = 8;
      break;

    case IMAGE_FORMAT_A16:
      analyzer = ColorAnalyzer::analyzeAlpha64; aPos = 0; inc = 2;
      break;
  }

  if (analyzer != NULL)
    return analyzer(d->first, d->stride, d->size.w, d->size.h, aPos, inc);
  else
    return ALPHA_DISTRIBUTION_FULL;
}

// ============================================================================
// [Fog::Image - Modified]
// ============================================================================

static void FOG_CDECL Image_modified(Image* self)
{
  // TODO:
}

// ============================================================================
// [Fog::Image - Copy / SetImage]
// ============================================================================

static err_t FOG_CDECL Image_copy(Image* self, const Image* other)
{
  ImageData* d = other->_d;

  if (self->_d == d)
    return ERR_OK;

  if (d == &Image_dEmpty)
  {
    d = &Image_dEmpty;
    d->reference.inc();
  }
  else
  {
    d = d->addRef();
    if (d == NULL)
      return ERR_RT_BUSY;
  }

  atomicPtrXchg(&self->_d, d)->release();
  return ERR_OK;
}

static err_t FOG_CDECL Image_copyDeep(Image* self, const Image* other)
{
  ImageData* d = other->_d;

  if (self->_d == d)
    return ERR_OK;

  if (d == &Image_dEmpty)
  {
    d = &Image_dEmpty;
    d->reference.inc();

    atomicPtrXchg(&self->_d, d)->release();
    return ERR_OK;
  }

  uint32_t type = d->type;
  FOG_RETURN_ON_ERROR(self->create(d->size, d->format, IMAGE_TYPE_BUFFER));

  ImageData* newd = self->_d;
  _api_raster.getCopyRectFunc(newd->format)(
    newd->first, newd->stride,
    d->first, d->stride,
    d->size.w, d->size.h, NULL);

  newd->colorKey = d->colorKey;
  newd->palette->setData(d->palette);

  atomicPtrXchg(&self->_d, newd)->release();

  self->_modified();
  return ERR_OK;
}

static err_t FOG_CDECL Image_setImage(Image* self, const Image* other, const RectI* area)
{
  ImageData* s_d = self->_d;
  ImageData* o_d = other->_d;

  if (other->isEmpty())
  {
    self->reset();
    return ERR_OK;
  }

  int x0 = area->x;
  int y0 = area->y;
  int x1 = x0 + area->w;
  int y1 = y0 + area->h;

  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > o_d->size.w) x1 = o_d->size.w;
  if (y1 > o_d->size.h) y1 = o_d->size.h;

  if (x0 >= x1 || y0 >= y1)
  {
    self->reset();
    return ERR_OK;
  }

  if (x0 == 0 && y0 == 0 && x1 == s_d->size.w && y1 == s_d->size.h)
    return _api.image_copy(self, other);

  int w = x1 - x0;
  int h = y1 - y0;

  if (s_d == o_d) o_d->reference.inc();
  err_t err = self->create(SizeI(w, h), other->getFormat());

  if (err != ERR_OK)
  {
    self->reset();
  }
  else
  {
    uint8_t* dData = self->_d->first;
    uint8_t* sData = o_d->first;

    ssize_t dStride = self->_d->stride;
    ssize_t sStride = o_d->stride;

    sData += (uint)y0 * sStride + (uint)x0 * o_d->bytesPerPixel;
    size_t bpl = w * o_d->bytesPerPixel;

    for (ssize_t i = 0; i < h; i++, dData += dStride, sData += sStride)
    {
      MemOps::copy(dData, sData, bpl);
    }

    self->_modified();
  }

  if (s_d == o_d) o_d->release();
  return err;
}

// ============================================================================
// [Fog::Image - Conversion]
// ============================================================================

static err_t FOG_CDECL Image_convert(Image* self, uint32_t format)
{
  ImageData* d = self->_d;

  if (d->stride == 0)
    return ERR_RT_INVALID_STATE;

  uint32_t sourceFormat = d->format;
  uint32_t targetFormat = format;

  if (targetFormat >= IMAGE_FORMAT_COUNT)
    return ERR_RT_INVALID_ARGUMENT;

  if (targetFormat == sourceFormat)
    return ERR_OK;

  if (targetFormat == IMAGE_FORMAT_I8)
    return self->convertTo8BitDepth();

  int w = d->size.w;
  int h = d->size.h;
  int y;

  const RasterCompositeCoreFuncs* funcs = _api_raster.getCompositeCore(targetFormat, COMPOSITE_SRC);
  RasterVBlitLineFunc blitLine = funcs->vblit_line[sourceFormat];

  RasterClosure closure;
  closure.ditherOrigin.reset();
  closure.palette = d->palette->_d;
  closure.data = NULL;

  // If the image target and source depths are the same, we do not need to
  // create the different image data (image have not to be shared and read-only).
  if (d->isDetached() && d->bytesPerPixel == ImageFormatDescription::getByFormat(format).getBytesPerPixel())
  {
    uint8_t* dstCur = d->first;
    ssize_t dstStride = d->stride;

    for (y = 0; y < h; y++, dstCur += dstStride)
      blitLine(dstCur, dstCur, w, &closure);

    d->format = targetFormat;
    return ERR_OK;
  }
  else
  {
    ImageData* newd;
    FOG_RETURN_ON_ERROR(_api.image_vTable[IMAGE_TYPE_BUFFER]->create(&newd, &d->size, targetFormat));

    uint8_t* dData = newd->first;
    uint8_t* sData = d->first;

    ssize_t dStride = newd->stride;
    ssize_t sStride = d->stride;

    for (y = 0; y < h; y++, dData += dStride, sData += sStride)
      blitLine(dData, sData, w, &closure);

    atomicPtrXchg(&self->_d, newd)->release();
    return ERR_OK;
  }
}

static err_t FOG_CDECL Image_forceFormat(Image* self, uint32_t format)
{
  ImageData* d = self->_d;

  if (d->stride == 0)
    return ERR_RT_INVALID_STATE;

  if (FOG_UNLIKELY(format >= (uint)IMAGE_FORMAT_COUNT))
    return ERR_RT_INVALID_ARGUMENT;

  if (d->bytesPerPixel != ImageFormatDescription::getByFormat(format).getBytesPerPixel())
    return ERR_RT_INVALID_ARGUMENT;

  if (d->format == format)
    return ERR_OK;

  FOG_RETURN_ON_ERROR(self->detach());
  d = self->_d;
  d->format = format;

  self->_modified();
  return ERR_OK;
}

// ============================================================================
// [Fog::Image - Quantization]
// ============================================================================

static err_t FOG_CDECL Image_convertTo8BPC(Image* self)
{
  uint32_t srcFormat = self->getFormat();
  uint32_t dstFormat = srcFormat;

  // ${IMAGE_FORMAT:BEGIN}
  switch (srcFormat)
  {
    case IMAGE_FORMAT_PRGB32:
    case IMAGE_FORMAT_PRGB64:
      dstFormat = IMAGE_FORMAT_PRGB32;
      break;

    case IMAGE_FORMAT_XRGB32:
    case IMAGE_FORMAT_RGB24:
      break;

    case IMAGE_FORMAT_RGB48:
      dstFormat = IMAGE_FORMAT_XRGB32;
      break;

    case IMAGE_FORMAT_A8:
    case IMAGE_FORMAT_A16:
      dstFormat = IMAGE_FORMAT_A8;
      break;

    case IMAGE_FORMAT_I8:
      break;
  }
  // ${IMAGE_FORMAT:END}

  return (dstFormat != srcFormat) ? self->convert(dstFormat) : (err_t)ERR_OK;
}

static err_t FOG_CDECL Image_convertTo8BitDepth(Image* self)
{
  ImageData* d = self->_d;

  if (d->stride == 0)
    return ERR_RT_INVALID_STATE;

  uint32_t format = d->format;
  if (format == IMAGE_FORMAT_I8 || format == IMAGE_FORMAT_A8)
    return ERR_OK;

  FOG_RETURN_ON_ERROR(Image_convertTo8BPC(self));
  d = self->_d;

  ImageData* newd;
  FOG_RETURN_ON_ERROR(_api.image_vTable[IMAGE_TYPE_BUFFER]->create(&newd, &d->size, IMAGE_FORMAT_I8));

  ssize_t dStride = newd->stride;
  uint8_t* dPtr = newd->first;

  ssize_t sStride = d->stride;
  const uint8_t* sPtr = d->first;

  int w = d->size.w;
  int h = d->size.h;

  int x;
  int y;

  err_t err = ERR_OK;
  Reduce reduce;

  if (reduce.analyze(*self, true))
  {
    err = newd->palette->setData(reduce.getPalette());
    if (FOG_IS_ERROR(err))
      goto _Fail;

    dStride -= w * 1;
    sStride -= w * d->bytesPerPixel;
    uint32_t mask = reduce.getMask();

    for (y = 0; y < h; y++, dPtr += dStride, sPtr += sStride)
    {
      switch (d->format)
      {
        case IMAGE_FORMAT_PRGB32:
        case IMAGE_FORMAT_XRGB32:
          for (x = 0; x < w; x++, dPtr += 1, sPtr += 4)
          {
            Face::p32 pix0p;

            Face::p32Load4a(pix0p, sPtr);
            Face::p32And(pix0p, pix0p, mask);
            dPtr[0] = reduce.traslate(pix0p);
          }
          break;

        case IMAGE_FORMAT_RGB24:
          for (x = 0; x < w; x++, dPtr += 1, sPtr += 3)
          {
            Face::p32 pix0p;

            Face::p32Load3b(pix0p, sPtr);
            Face::p32And(pix0p, pix0p, mask);
            dPtr[0] = reduce.traslate(pix0p);
          }
          break;

        default:
          FOG_ASSERT_NOT_REACHED();
      }
    }
  }
  else
  {
    ImagePalette palette = ImagePalette::fromColorCube(6, 6, 6);
    FOG_RETURN_ON_ERROR(newd->palette->setData(palette));

    ImageDither8Params params;
    params.rCount = 6;
    params.gCount = 6;
    params.bCount = 6;
    params.transposeTable = NULL;
    params.transposeTableLength = 0;

    ImageConverter converter;
    FOG_RETURN_ON_ERROR(converter.createDithered8(params,
      ImageFormatDescription::getByFormat(d->format), NULL, NULL));

    converter.blitRect(dPtr, dStride, sPtr, sStride, w, h, PointI(0, 0));
  }

  atomicPtrXchg(&self->_d, newd)->release();
  newd->updatePalette(Range(0, 256));

  self->_modified();
  return ERR_OK;

_Fail:
  newd->release();
  return err;
}

static err_t FOG_CDECL Image_convertTo8BitDepthPalette(Image* self, const ImagePalette* palette)
{
  ImageData* d = self->_d;

  if (d->stride == 0)
    return ERR_RT_INVALID_STATE;

  FOG_RETURN_ON_ERROR(Image_convertTo8BPC(self));
  d = self->_d;

  ImageData* newd;
  FOG_RETURN_ON_ERROR(_api.image_vTable[IMAGE_TYPE_BUFFER]->create(&newd, &d->size, IMAGE_FORMAT_I8));
  newd->palette->setData(*palette);

  int w = d->size.w;
  int h = d->size.h;

  int x;
  int y;

  uint8_t* dPtr = newd->first;
  ssize_t dStride = newd->stride - w * newd->bytesPerPixel;

  const uint8_t* sPtr = d->first;
  ssize_t sStride = d->stride - w * d->bytesPerPixel;

  // ${IMAGE_FORMAT:BEGIN}
  switch (d->format)
  {
    case IMAGE_FORMAT_PRGB32:
    case IMAGE_FORMAT_XRGB32:
    {
      for (y = 0; y < h; y++, dPtr += dStride, sPtr += sStride)
      {
        for (x = 0; x < w; x++, dPtr += 1, sPtr += 4)
        {
          Face::p32 pix0p;
          Face::p32Load4a(pix0p, sPtr);

          dPtr[0] = palette->findRgb(Face::p32PBB2AsU32(pix0p),
                                     Face::p32PBB1AsU32(pix0p),
                                     Face::p32PBB0AsU32(pix0p));
        }
      }
      break;
    }

    case IMAGE_FORMAT_RGB24:
    {
      for (y = 0; y < h; y++, dPtr += dStride, sPtr += sStride)
      {
        for (x = 0; x < w; x++, dPtr += 1, sPtr += 3)
        {
          dPtr[0] = palette->findRgb(sPtr[PIXEL_RGB24_POS_R],
                                     sPtr[PIXEL_RGB24_POS_G],
                                     sPtr[PIXEL_RGB24_POS_B]);
        }
      }
      break;
    }

    case IMAGE_FORMAT_I8:
    {
      // Build a lookup table.
      uint8_t table[256];

      uint srcPalLength = (uint)d->palette->getLength();
      const Argb32* srcPalData = d->palette->getData();

      y = 0;

      while (y < (int)srcPalLength)
      {
        Argb32 c0 = srcPalData[y];
        table[y] = palette->findRgb(c0.getRed(), c0.getGreen(), c0.getBlue());
        y++;
      }

      while (y < 256)
      {
        table[y] = (uint8_t)(uint)y;
        y++;
      }

      for (y = 0; y < h; y++, dPtr += dStride, sPtr += sStride)
      {
        for (x = 0; x < w; x++, dPtr += 1, sPtr += 1)
        {
          dPtr[0] = table[sPtr[0]];
        }
      }
      break;
    }

    default:
      FOG_ASSERT_NOT_REACHED();
  }
  // ${IMAGE_FORMAT:END}

  atomicPtrXchg(&self->_d, newd)->release();

  self->_modified();
  return ERR_OK;
}

// ============================================================================
// [Fog::Image - Palette]
// ============================================================================

static err_t FOG_CDECL Image_setPalette(Image* self, const ImagePalette* palette)
{
  if (self->isEmpty())
    return ERR_RT_INVALID_STATE;

  FOG_RETURN_ON_ERROR(self->detach());
  ImageData* d = self->_d;

  FOG_RETURN_ON_ERROR(d->palette->setData(*palette));
  d->updatePalette(Range(0, palette->getLength()));

  self->_modified();
  return ERR_OK;
}

static err_t FOG_CDECL Image_setPaletteData(Image* self, const Range* range, const Argb32* pal)
{
  if (self->isEmpty())
    return ERR_RT_INVALID_STATE;

  FOG_RETURN_ON_ERROR(self->detach());
  ImageData* d = self->_d;

  size_t rStart, rEnd;
  if (!Range::fit(rStart, rEnd, 256, range))
    return ERR_OK;

  FOG_RETURN_ON_ERROR(d->palette->setData(Range(rStart, rEnd), pal));
  d->updatePalette(Range(rStart, rEnd));

  self->_modified();
  return ERR_OK;
}

// ============================================================================
// [Fog::Image - Clear]
// ============================================================================

static err_t FOG_CDECL Image_clearArgb32(Image* self, const Argb32* c0)
{
  ImageData* d = self->_d;

  if (d->stride == 0)
    return ERR_RT_INVALID_STATE;

  if (d->format  == IMAGE_FORMAT_I8)
    return ERR_IMAGE_INVALID_FORMAT;

  if (!d->isDetached())
  {
    FOG_RETURN_ON_ERROR(self->create(d->size, d->format, d->type));
    d = self->_d;
  }

  RectI screen(0, 0, d->size.w, d->size.h);
  return _api.image_fillRectArgb32(self, &screen, c0, COMPOSITE_SRC, 1.0f);
}

static err_t FOG_CDECL Image_clearColor(Image* self, const Color* c0)
{
  ImageData* d = self->_d;

  if (d->stride == 0)
    return ERR_RT_INVALID_STATE;

  if (d->format  == IMAGE_FORMAT_I8)
    return ERR_IMAGE_INVALID_FORMAT;

  if (!d->isDetached())
  {
    FOG_RETURN_ON_ERROR(self->create(d->size, d->format, d->type));
    d = self->_d;
  }

  RectI screen(0, 0, d->size.w, d->size.h);
  return _api.image_fillRectColor(self, &screen, c0, COMPOSITE_SRC, 1.0f);
}

// ============================================================================
// [Fog::Image - FillRect]
// ============================================================================

static err_t Image_fillRectPrivate(Image* self, const RectI* r, uint32_t compositingOperator, uint32_t spanOpacity, const RasterSolid* solid, bool isPRGBPixel)
{
  ImageData* d = self->_d;

  int x0 = r->x;
  int y0 = r->y;
  int x1 = x0 + r->w;
  int y1 = y0 + r->h;

  int w = d->size.w;
  int h = d->size.h;

  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > w) x1 = w;
  if (y1 > h) y1 = h;

  if ((w = x1 - x0) <= 0 || (h = y1 - y0) == 0)
    return ERR_OK;

  if (!d->isDetached())
  {
    FOG_RETURN_ON_ERROR(self->_detach());
    d = self->_d;
  }

  uint32_t format = d->format;
  ssize_t stride = d->stride;

  uint8_t* p = d->first + y0 * stride;

  RasterClosure closure;
  closure.ditherOrigin.set(x0, y0);
  closure.palette = NULL;
  closure.data = NULL;

  if (spanOpacity == 0xFFFFFFFF)
  {
    RasterCBlitLineFunc blitLine = _api_raster.getCBlitLine(format, compositingOperator, isPRGBPixel);
    p += (uint)x0 * d->bytesPerPixel;

    for (int i = 0; i < h; i++, p += stride)
    {
      blitLine(p, solid, w, &closure);
    }
  }
  else
  {
    RasterSpan span;
    RasterCBlitSpanFunc blitSpan = _api_raster.getCBlitSpan(format, compositingOperator, isPRGBPixel);

    span.setPositionAndType(x0, x1, RASTER_SPAN_C);
    span.setNext(NULL);
    reinterpret_cast<RasterSpan16*>(&span)->setConstMask(spanOpacity);

    for (int i = 0; i < h; i++, p += stride)
    {
      blitSpan(p, solid, &span, &closure);
    }
  }

  return ERR_OK;
}

static err_t FOG_CDECL Image_fillRectArgb32(Image* self, const RectI* r, const Argb32* c0, uint32_t compositingOperator, float opacity)
{
  ImageData* d = self->_d;

  if ((uint)compositingOperator >= COMPOSITE_COUNT)
    return ERR_RT_INVALID_ARGUMENT;

  if (d->stride == 0)
    return ERR_RT_INVALID_STATE;

  uint32_t format = d->format;
  if (format  == IMAGE_FORMAT_I8)
    return ERR_IMAGE_INVALID_FORMAT;

  if (opacity <= 0.0f)
    return ERR_OK;

  if (opacity > 1.0f)
    opacity = 1.0f;

  uint32_t spanOpacity;

  RasterSolid solid;
  bool isPRGBPixel;

  switch (ImageFormatDescription::getByFormat(format).getPrecision())
  {
    case IMAGE_PRECISION_BYTE:
    {
      Face::p32 pix0 = c0->p32;
      Face::p32PRGB32FromARGB32(solid.prgb32.p32, pix0);

      Face::p32 opacity_8 = Math::uroundToByte256(opacity);
      if (opacity_8 == 0) return ERR_OK;

      spanOpacity = opacity_8 < 0x100 ? opacity_8 : 0xFFFFFFFF;
      isPRGBPixel = Face::p32PRGB32IsAlphaFF(pix0);
      break;
    }

    case IMAGE_PRECISION_WORD:
    {
      Face::p64 pix0;
      Face::p64ARGB64FromARGB32(solid.prgb64.p64, c0->p32);
      Face::p64PRGB64FromARGB64(solid.prgb64.p64, pix0);

      Face::p32 opacity_16 = Math::uroundToWord65536(opacity);
      if (opacity_16 == 0) return ERR_OK;

      spanOpacity = opacity_16 < 0x10000 ? opacity_16 : 0xFFFFFFFF;
      isPRGBPixel = Face::p64PRGB64IsAlphaFFFF(pix0);
      break;
    }

    default:
      FOG_ASSERT_NOT_REACHED();
  }

  return Image_fillRectPrivate(self, r, compositingOperator, spanOpacity, &solid, isPRGBPixel);
}

static err_t FOG_CDECL Image_fillRectColor(Image* self, const RectI* r, const Color* c0,  uint32_t compositingOperator, float opacity)
{
  ImageData* d = self->_d;

  if ((uint)compositingOperator >= COMPOSITE_COUNT)
    return ERR_RT_INVALID_ARGUMENT;

  if (d->stride == 0)
    return ERR_RT_INVALID_STATE;

  uint32_t format = d->format;
  if (format  == IMAGE_FORMAT_I8)
    return ERR_IMAGE_INVALID_FORMAT;

  if (opacity <= 0.0f)
    return ERR_OK;

  if (opacity > 1.0f)
    opacity = 1.0f;

  uint32_t spanOpacity;

  RasterSolid solid;
  bool isPRGBPixel;

  switch (ImageFormatDescription::getByFormat(format).getPrecision())
  {
    case IMAGE_PRECISION_BYTE:
    {
      Face::p32 pix0 = c0->getArgb32();
      Face::p32PRGB32FromARGB32(solid.prgb32.p32, pix0);

      Face::p32 opacity_8 = Math::uroundToByte256(opacity);
      if (opacity_8 == 0) return ERR_OK;

      spanOpacity = opacity_8 < 0x100 ? opacity_8 : 0xFFFFFFFF;
      isPRGBPixel = Face::p32PRGB32IsAlphaFF(pix0);
      break;
    }

    case IMAGE_PRECISION_WORD:
    {
      Face::p64 pix0 = Face::p64FromU64(c0->getArgb64());
      Face::p64PRGB64FromARGB64(solid.prgb64.p64, pix0);

      Face::p32 opacity_16 = Math::uroundToWord65536(opacity);
      if (opacity_16 == 0) return ERR_OK;

      spanOpacity = opacity_16 < 0x10000 ? opacity_16 : 0xFFFFFFFF;
      isPRGBPixel = Face::p64PRGB64IsAlphaFFFF(pix0);
      break;
    }

    default:
      FOG_ASSERT_NOT_REACHED();
  }

  return Image_fillRectPrivate(self, r, compositingOperator, spanOpacity, &solid, isPRGBPixel);
}

// ============================================================================
// [Fog::Image - Blit]
// ============================================================================

static err_t Image_blitImagePrivate(
  Image* dst, int dstX, int dstY,
  const Image* src, int srcX, int srcY,
  int w, int h,
  uint32_t compositingOperator, float opacity)
{
  ImageData* dst_d = dst->_d;
  ImageData* src_d = src->_d;

  uint32_t dFormat = dst_d->format;
  uint32_t sFormat = src_d->format;

  bool isOpaque;
  RasterSpanExt8 span;

  if (opacity <= 0.0f)
    return ERR_OK;

  if (opacity > 1.0f)
    opacity = 1.0f;

  switch (ImageFormatDescription::getByFormat(dFormat).getPrecision())
  {
    case IMAGE_PRECISION_BYTE:
    {
      Face::p32 opacity_8 = Math::uroundToByte256(opacity);
      if (opacity_8 == 0) return ERR_OK;

      isOpaque = (opacity_8 == 0x100);
      reinterpret_cast<RasterSpan8*>(&span)->setConstMask(opacity_8);
      break;
    }

    case IMAGE_PRECISION_WORD:
    {
      Face::p32 opacity_16 = Math::uroundToWord65536(opacity);
      if (opacity_16 == 0) return ERR_OK;

      isOpaque = (opacity_16 == 0x10000);
      reinterpret_cast<RasterSpan16*>(&span)->setConstMask(opacity_16);
      break;
    }

    default:
      FOG_ASSERT_NOT_REACHED();
  }

  if (!dst_d->isDetached())
  {
    FOG_RETURN_ON_ERROR(dst->_detach());

    // We need to update src_d too to handle that case that 'dst == src'.
    dst_d = dst->_d;
    src_d = src->_d; 
  }

  uint8_t* dPixels = dst_d->first;
  ssize_t dStride = dst_d->stride;

  const uint8_t* sPixels = src_d->first;
  ssize_t sStride = src_d->stride;

  // Special case if dst and src overlaps.
  if (dst_d == src_d && dstY >= srcY && (dstY - srcY) <= h)
  {
    dPixels += (dstY + h - 1) * dStride;
    sPixels += (srcY + h - 1) * sStride;

    dStride = -dStride;
    sStride = -sStride;
  }
  else
  {
    dPixels += dstY * dStride;
    sPixels += srcY * sStride;
  }

  dPixels += dstX * dst_d->bytesPerPixel;
  sPixels += srcX * src_d->bytesPerPixel;

  RasterClosure closure;
  closure.ditherOrigin.reset();
  closure.palette = src_d->palette->_d;
  closure.data = NULL;

  MemBufferTmp<2048> buffer;

  if (isOpaque)
  {
    RasterVBlitLineFunc blitLine;
    RasterVBlitLineFunc converter = NULL;

    if (RasterUtil::isCompositeCoreOp(compositingOperator))
    {
      blitLine = _api_raster.getCompositeCore(dFormat, compositingOperator)->vblit_line[sFormat];
    }
    else
    {
      uint32_t compat  = RasterUtil::getCompositeCompatFormat(dFormat, sFormat);
      uint32_t vBlitId = RasterUtil::getCompositeCompatVBlitId(dFormat, sFormat);

      blitLine = _api_raster.getCompositeExt(dFormat, compositingOperator)->vblit_line[compat];

      if (compat != sFormat)
      {
        converter = _api_raster.getCompositeCore(compat, COMPOSITE_SRC)->vblit_line[sFormat];

        if (FOG_IS_NULL(buffer.alloc(w * ImageFormatDescription::getByFormat(compat).getBytesPerPixel())))
          return ERR_RT_OUT_OF_MEMORY;
      }
    }

    // Overlapped.
    if (converter == NULL && dst_d == src_d && dstX >= srcX && (dstX - srcX) <= w)
    {
      converter = _api_raster.getCompositeCore(sFormat, COMPOSITE_SRC)->vblit_line[sFormat];

      if (FOG_IS_NULL(buffer.alloc(w * src_d->bytesPerPixel)))
        return ERR_RT_OUT_OF_MEMORY;
    }

    if (converter == NULL)
    {
      for (int i = 0; i < h; i++, dPixels += dStride, sPixels += sStride)
      {
        blitLine(dPixels, sPixels, w, &closure);
      }
    }
    else
    {
      for (int i = 0; i < h; i++, dPixels += dStride, sPixels += sStride)
      {
        converter(reinterpret_cast<uint8_t*>(buffer.getMem()), sPixels, w, NULL);
        blitLine(dPixels, reinterpret_cast<uint8_t*>(buffer.getMem()), w, &closure);
      }
    }
  }
  else
  {
    RasterVBlitSpanFunc blitLine;
    RasterVBlitLineFunc converter = NULL;

    if (RasterUtil::isCompositeCoreOp(compositingOperator))
    {
      blitLine = _api_raster.getCompositeCore(dFormat, compositingOperator)->vblit_span[sFormat];
    }
    else
    {
      uint32_t compat  = RasterUtil::getCompositeCompatFormat(dFormat, sFormat);
      uint32_t vBlitId = RasterUtil::getCompositeCompatVBlitId(dFormat, sFormat);

      blitLine = _api_raster.getCompositeExt(dFormat, compositingOperator)->vblit_span[compat];

      if (compat != sFormat)
      {
        converter = _api_raster.getCompositeCore(compat, COMPOSITE_SRC)->vblit_line[sFormat];

        if (FOG_IS_NULL(buffer.alloc(w * ImageFormatDescription::getByFormat(compat).getBytesPerPixel())))
          return ERR_RT_OUT_OF_MEMORY;
      }
    }

    // Overlapped.
    if (converter == NULL && dst_d == src_d && dstX >= srcX && (dstX - srcX) <= w)
    {
      converter = _api_raster.getCompositeCore(sFormat, COMPOSITE_SRC)->vblit_line[sFormat];

      if (FOG_IS_NULL(buffer.alloc(w * src_d->bytesPerPixel)))
        return ERR_RT_OUT_OF_MEMORY;
    }

    span.setPositionAndType(0, w, RASTER_SPAN_C);
    span.setNext(NULL);

    if (converter == NULL)
    {
      for (int i = 0; i < h; i++, dPixels += dStride, sPixels += sStride)
      {
        span.setData(const_cast<uint8_t*>(sPixels));
        blitLine(dPixels, &span, &closure);
      }
    }
    else
    {
      span.setData(reinterpret_cast<uint8_t*>(buffer.getMem()));

      for (int i = 0; i < h; i++, dPixels += dStride, sPixels += sStride)
      {
        converter(reinterpret_cast<uint8_t*>(buffer.getMem()), sPixels, w, NULL);
        blitLine(dPixels, &span, &closure);
      }
    }
  }

  return ERR_OK;
}

static err_t FOG_CDECL Image_blitImageAt(Image* self, const PointI* pt, const Image* i, const RectI* iFragment, uint32_t compositingOperator, float opacity)
{
  if (FOG_UNLIKELY(compositingOperator >= COMPOSITE_COUNT))
    return ERR_RT_INVALID_ARGUMENT;

  ImageData* dst_d = self->_d;
  ImageData* src_d = i->_d;

  if (iFragment == NULL)
  {
    int w = dst_d->size.w;
    int h = dst_d->size.h;

    int x0 = pt->x;
    int y0 = pt->y;
    int x1 = x0 + src_d->size.w;
    int y1 = y0 + src_d->size.h;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > w) x1 = w;
    if (y1 > h) y1 = h;

    if (x0 >= x1 || y0 >= y1)
      return ERR_OK;

    return Image_blitImagePrivate(self, x0, y0, i, x0 - pt->x, y0 - pt->y, x1 - x0, y1 - y0, compositingOperator, opacity);
  }
  else
  {
    if (FOG_UNLIKELY(!iFragment->isValid()))
      return ERR_OK;

    int srcX0 = iFragment->x;
    int srcY0 = iFragment->y;
    int srcX1 = srcX0 + iFragment->w;
    int srcY1 = srcY0 + iFragment->h;

    if (srcX0 < 0) srcX0 = 0;
    if (srcY0 < 0) srcY0 = 0;
    if (srcX1 > src_d->size.w) srcX1 = src_d->size.w;
    if (srcY1 > src_d->size.h) srcY1 = src_d->size.h;

    if (srcX0 >= srcX1 || srcY0 >= srcY1)
      return ERR_OK;

    int dstX0 = pt->x + (srcX0 - iFragment->x);
    int dstY0 = pt->y + (srcY0 - iFragment->y);
    int dstX1 = dstX0 + (srcX1 - srcX0);
    int dstY1 = dstY0 + (srcY1 - srcY0);

    if (dstX0 < 0) { srcX0 -= dstX0; dstX0 = 0; }
    if (dstY0 < 0) { srcY0 -= dstY0; dstY0 = 0; }
    if (dstX1 > dst_d->size.w) dstX1 = dst_d->size.w;
    if (dstY1 > dst_d->size.h) dstY1 = dst_d->size.h;

    if (dstX0 >= dstX1 || dstY0 >= dstY1)
      return ERR_OK;

    return Image_blitImagePrivate(self, dstX0, dstY0, i, srcX0, srcY0, dstX1 - dstX0, dstY1 - dstY0, compositingOperator, opacity);
  }
}

// ============================================================================
// [Fog::Image - Scroll]
// ============================================================================

static err_t FOG_CDECL Image_scroll(Image* self, const PointI* pt, const RectI* area)
{
  int scrollX = pt->x;
  int scrollY = pt->y;

  if ((scrollX | scrollY) == 0)
    return ERR_OK;

  ImageData* d = self->_d;

  int x0, y0;
  int x1, y1;

  if (area == NULL)
  {
    x0 = 0;
    y0 = 0;
    x1 = d->size.w;
    y1 = d->size.h;
  }
  else
  {
    x0 = area->x;
    y0 = area->y;
    x1 = x0 + area->w;
    y1 = y0 + area->h;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > d->size.w) x1 = d->size.w;
    if (y1 > d->size.h) y1 = d->size.h;

    if (x0 >= x1 || y0 >= y1)
      return ERR_OK;
  }

  int absX = Math::abs(scrollX);
  int absY = Math::abs(scrollY);

  int scrollW = x1 - x0;
  int scrollH = y1 - y0;

  int srcX, srcY;
  int dstX, dstY;

  if (absX >= scrollW || absY >= scrollH)
    return ERR_OK;

  if (!d->isDetached())
  {
    FOG_RETURN_ON_ERROR(self->_detach());
    d = self->_d;
  }

  if (scrollX < 0) { srcX = absX; dstX = 0; } else { srcX = 0; dstX = absX; }
  if (scrollY < 0) { srcY = absY; dstY = 0; } else { srcY = 0; dstY = absY; }

  scrollW -= absX;
  scrollH -= absY;

  ssize_t stride = d->stride;
  ssize_t size = scrollW * d->bytesPerPixel;

  uint8_t* dstPixels = d->first + dstX * d->bytesPerPixel;
  uint8_t* srcPixels = d->first + srcX * d->bytesPerPixel;

  dstPixels += dstY * stride;
  srcPixels += srcY * stride;

  if (scrollY > 0)
  {
    dstPixels += (scrollH - 1) * stride;
    srcPixels += (scrollH - 1) * stride;

    stride = -stride;
  }

  for (int y = scrollH; y; y--, dstPixels += stride, srcPixels += stride)
  {
    MemOps::move(dstPixels, srcPixels, size);
  }

  return ERR_OK;
}

// ============================================================================
// [Fog::Image - Read]
// ============================================================================

static err_t FOG_CDECL Image_readFromFile(Image* self, const StringW* fileName)
{
  ImageDecoder* decoder = NULL;
  FOG_RETURN_ON_ERROR(ImageCodecProvider::createDecoderForFile(*fileName, &decoder));

  err_t err = decoder->readImage(*self);
  fog_delete(decoder);
  return err;
}

static err_t FOG_CDECL Image_readFromStream(Image* self, Stream* stream, const StringW* ext)
{
  if (ext == NULL)
    ext = _api.stringw_oEmpty;

  ImageDecoder* decoder = NULL;
  FOG_RETURN_ON_ERROR(ImageCodecProvider::createDecoderForStream(*stream, *ext, &decoder));

  err_t err = decoder->readImage(*self);
  fog_delete(decoder);
  return err;
}

static err_t FOG_CDECL Image_readFromBufferStringA(Image* self, const StringA* buffer, const StringW* ext)
{
  return _api.image_readFromBufferRaw(self, buffer->getData(), buffer->getLength(), ext);
}

static err_t FOG_CDECL Image_readFromBufferRaw(Image* self, const void* buffer, size_t size, const StringW* ext)
{
  Stream stream;
  stream.openBuffer((void*)buffer, size, STREAM_OPEN_READ);
  return _api.image_readFromStream(self, &stream, ext);
}

// ============================================================================
// [Fog::Image - Write]
// ============================================================================

static err_t FOG_CDECL Image_writeToFile(const Image* self, const StringW* fileName, const Hash<StringW, Var>* options)
{
  StringTmpW<16> ext;

  FOG_RETURN_ON_ERROR(FilePath::extractExtension(ext, *fileName));
  FOG_RETURN_ON_ERROR(ext.lower());

  Stream stream;
  FOG_RETURN_ON_ERROR(
    stream.openFile(*fileName,
      STREAM_OPEN_CREATE      |
      STREAM_OPEN_CREATE_PATH |
      STREAM_OPEN_WRITE       |
      STREAM_OPEN_TRUNCATE    )
  );

  FOG_RETURN_ON_ERROR(_api.image_writeToStream(self, &stream, &ext, options));
  return ERR_OK;
}

static err_t FOG_CDECL Image_writeToStream(const Image* self, Stream* stream, const StringW* ext, const Hash<StringW, Var>* options)
{
  ImageCodecProvider* provider = ImageCodecProvider::getProviderByExtension(IMAGE_CODEC_ENCODER, *ext);

  if (provider == NULL)
    return ERR_IMAGE_NO_ENCODER;

  ImageEncoder* encoder = NULL;
  FOG_RETURN_ON_ERROR(provider->createCodec(IMAGE_CODEC_ENCODER, reinterpret_cast<ImageCodec**>(&encoder)));

  encoder->attachStream(*stream);

  if (options && options->getLength())
  {
    HashIterator<StringW, Var> it(*options);
    while (it.isValid())
    {
      reinterpret_cast<Object*>(encoder)->setProperty(it.getKey(), it.getItem());
      it.next();
    }
  }

  err_t err = encoder->writeImage(*self);
  fog_delete(encoder);
  return err;
}

static err_t FOG_CDECL Image_writeToBuffer(const Image* self, StringA* buffer, uint32_t cntOp, const StringW* ext, const Hash<StringW, Var>* options)
{
  if (cntOp == CONTAINER_OP_REPLACE)
    buffer->clear();

  Stream stream;
  FOG_RETURN_ON_ERROR(stream.openBuffer(*buffer));

  if (cntOp != CONTAINER_OP_REPLACE)
    stream.seek(buffer->getLength(), STREAM_SEEK_SET);

  return _api.image_writeToStream(self, &stream, ext, options);
}

// ============================================================================
// [Fog::Image - Windows Support]
// ============================================================================

#if defined(FOG_OS_WINDOWS)
// The following code can be used to test the Fog::Image <=> HBITMAP conversion:
//
// HDC hdc = CreateCompatibleDC(NULL);
// HBITMAP hbmp = CreateCompatibleBitmap(hdc, 320, 200);
// RECT r = {0 , 0, 320, 200 };
//
// HGDIOBJ old = SelectObject(hdc, (HGDIOBJ)hbmp);
// FillRect(hdc, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));
// SetBkMode(hdc, TRANSPARENT);
// TextOut(hdc, 0, 0, "Abcdefghijklmnop", 16);
// SelectObject(hdc, old);
// DeleteObject(hdc);
//
// Image i;
// i.fromWinBitmap(hbmp);
// DeleteObject((HGDIOBJ)hbmp);

struct FOG_NO_EXPORT WinDibImageData : public ImageData
{
  HBITMAP hBitmap;
};

static err_t Image_WinDib_createDibSection(HBITMAP* dst, uint8_t** bits, ssize_t* stride, const SizeI* size, uint32_t format)
{
  err_t err = ERR_OK;
  HBITMAP hBitmap = NULL;

  switch (format)
  {
    case IMAGE_FORMAT_PRGB32:
    {
      BITMAPV4HEADER bmi;
      ZeroMemory(&bmi, sizeof(bmi));

      bmi.bV4Size          = sizeof(bmi);
      bmi.bV4Width         = size->w;
      bmi.bV4Height        =-size->h;
      bmi.bV4Planes        = 1;
      bmi.bV4BitCount      = 32;
      bmi.bV4V4Compression = BI_RGB;

      bmi.bV4RedMask       = 0x00FF0000;
      bmi.bV4GreenMask     = 0x0000FF00;
      bmi.bV4BlueMask      = 0x000000FF;
      bmi.bV4AlphaMask     = 0xFF000000;

      hBitmap = ::CreateDIBSection(NULL, reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS, (void**)bits, NULL, 0);

      if (hBitmap == NULL)
        err = ERR_RT_OUT_OF_MEMORY;
      break;
    }

    case IMAGE_FORMAT_XRGB32:
    case IMAGE_FORMAT_RGB24:
    {
      BITMAPINFOHEADER bmi;
      ZeroMemory(&bmi, sizeof(bmi));

      bmi.biSize           = sizeof(bmi);
      bmi.biWidth          = size->w;
      bmi.biHeight         =-size->h;
      bmi.biPlanes         = 1;
      bmi.biBitCount       = ImageFormatDescription::getByFormat(format).getDepth();
      bmi.biCompression    = BI_RGB;

      hBitmap = ::CreateDIBSection(NULL, reinterpret_cast<BITMAPINFO*>(&bmi), DIB_RGB_COLORS, (void**)bits, NULL, 0);

      if (hBitmap == NULL)
        err = ERR_RT_OUT_OF_MEMORY;
      break;
    }

    case IMAGE_FORMAT_I8:
    {
      // TODO: IMAGE_FORMAT_I8 windows support.
    }

    // Other formats are not supported.
    default:
    {
      err = ERR_IMAGE_INVALID_FORMAT;
      break;
    }
  }

  *dst = hBitmap;
  if (hBitmap)
  {
    DIBSECTION info;
    ::GetObjectW(hBitmap, sizeof(DIBSECTION), &info);

    *stride = info.dsBm.bmWidthBytes;
  }

  return err;
}

static err_t FOG_CDECL Image_WinDib_create(ImageData** pd, const SizeI* size, uint32_t format);
static void  FOG_CDECL Image_WinDib_destroy(ImageData* d);
static void* FOG_CDECL Image_WinDib_getHandle(const ImageData* d);
static err_t FOG_CDECL Image_WinDib_updatePalette(ImageData* d, const Range* range);

static const ImageVTable Image_WinDib_vTable = 
{
  Image_WinDib_create,
  Image_WinDib_destroy,
  Image_WinDib_getHandle,
  Image_WinDib_updatePalette
};

static err_t FOG_CDECL Image_WinDib_create(ImageData** pd, const SizeI* size, uint32_t format)
{
  HBITMAP hBitmap;
  uint8_t* bits;
  ssize_t stride;

  FOG_RETURN_ON_ERROR(Image_WinDib_createDibSection(&hBitmap, &bits, &stride, size, format));

  const ImageFormatDescription& desc = ImageFormatDescription::getByFormat(format);
  WinDibImageData* d = reinterpret_cast<WinDibImageData*>(MemMgr::alloc(sizeof(WinDibImageData)));

  if (FOG_IS_NULL(d))
  {
    ::DeleteObject((HGDIOBJ)hBitmap);
    return ERR_RT_OUT_OF_MEMORY;
  }

  d->reference.init(1);
  d->vType = VAR_TYPE_IMAGE | VAR_FLAG_NONE;
  d->locked = 0;

  d->vtable = &Image_WinDib_vTable;
  d->size = *size;
  d->format = format;
  d->type = IMAGE_TYPE_WIN_DIB;
  d->adopted = 0;
  d->colorKey = IMAGE_COLOR_KEY_NONE;
  d->bytesPerPixel = desc.getBytesPerPixel();
  FOG_PADDING_ZERO_64(d->padding);

  d->data = bits;
  d->first = bits;
  d->stride = stride;

  d->palette.init();
  d->hBitmap = hBitmap;

  *pd = d;
  return ERR_OK;
}

static void FOG_CDECL Image_WinDib_destroy(ImageData* _d)
{
  WinDibImageData* d = reinterpret_cast<WinDibImageData*>(_d);

  if (d->hBitmap)
    ::DeleteObject((HGDIOBJ)d->hBitmap);

  d->palette.destroy();
  MemMgr::free(d);
}

static void* FOG_CDECL Image_WinDib_getHandle(const ImageData* _d)
{
  const WinDibImageData* d = reinterpret_cast<const WinDibImageData*>(_d);
  return (void*)d->hBitmap;
}

static err_t FOG_CDECL Image_WinDib_updatePalette(ImageData* _d, const Range* range)
{
  WinDibImageData* d = reinterpret_cast<WinDibImageData*>(_d);
  
  if (d->format == IMAGE_FORMAT_I8)
  {
    // TODO: WinDib update palette.
  }

  return ERR_OK;
}

static err_t FOG_CDECL Image_toWinBitmap(const Image* self, HBITMAP* dst)
{
  ImageData* d = self->_d;

  if (d->stride == 0)
  {
    *dst = NULL;
    return ERR_IMAGE_INVALID_SIZE;
  }

  // If the image format is PRGB32/XRGB32/RGB24 then it's easy, it's only
  // needed to create DIBSECTION and copy bits there. If the image format
  // is A8/I8 then we create 32-bit DIBSECTION and copy there the alphas,
  // this image will be still usable when using functions like AlphaBlend().

  uint8_t* dstBits = NULL;
  ssize_t dstStride = 0;

  uint8_t* srcBits = d->first;
  ssize_t srcStride = d->stride;

  uint32_t dstFormat = IMAGE_FORMAT_NULL;

  switch (d->format)
  {
    case IMAGE_FORMAT_PRGB32:
    case IMAGE_FORMAT_PRGB64:
      dstFormat = IMAGE_FORMAT_PRGB32;
      break;

    case IMAGE_FORMAT_XRGB32:
    case IMAGE_FORMAT_RGB24:
      dstFormat = d->format;
      break;

    // There is no such concept like Alpha-only image in Windows GDI. So we
    // treat this kind of image as premultiplied RGB. It's waste, but I have
    // no other idea.
    case IMAGE_FORMAT_A8:
    case IMAGE_FORMAT_A16:
      dstFormat = IMAGE_FORMAT_PRGB32;
      break;

    case IMAGE_FORMAT_I8:
      dstFormat = IMAGE_FORMAT_XRGB32;
      break;

    default:
      FOG_ASSERT_NOT_REACHED();
  }

  HBITMAP hBitmap;
  FOG_RETURN_ON_ERROR(Image_WinDib_createDibSection(&hBitmap, &dstBits, &dstStride, &d->size, dstFormat));

  RasterVBlitLineFunc blitLine = _api_raster.getCompositeCore(dstFormat, COMPOSITE_SRC)->vblit_line[d->format];

  RasterClosure closure;
  closure.ditherOrigin.reset();
  closure.palette = d->palette->_d;
  closure.data = NULL;

  int w = d->size.w;
  int h = d->size.h;

  for (int y = 0; y < h; y++, dstBits += dstStride, srcBits += srcStride, closure.ditherOrigin.y++)
  {
    blitLine(dstBits, srcBits, w, &closure);
  }

  *dst = hBitmap;
  return ERR_OK;
}

static err_t FOG_CDECL Image_fromWinBitmap(Image* self, HBITMAP hBitmap)
{
  if (hBitmap == NULL)
  {
    self->reset();
    return ERR_RT_INVALID_ARGUMENT;
  }

  uint32_t format = IMAGE_FORMAT_NULL;
  DIBSECTION ds;

  // --------------------------------------------------------------------------
  // [DIBSECTION]
  // --------------------------------------------------------------------------

  if (GetObjectW(hBitmap, sizeof(DIBSECTION), &ds) != 0)
  {
    RasterVBlitLineFunc blitLine = NULL;

    // RGB24.
    if (ds.dsBm.bmBitsPixel == 24)
    {
      format = IMAGE_FORMAT_RGB24;
      blitLine = _api_raster.convert.copy[RASTER_COPY_24];
    }

    // RGB32.
    if (ds.dsBm.bmBitsPixel == 32 && ds.dsBitfields[0] == 0x00FF0000 &&
                                     ds.dsBitfields[1] == 0x0000FF00 &&
                                     ds.dsBitfields[2] == 0x000000FF)
    {
      format = IMAGE_FORMAT_XRGB32;
      blitLine = _api_raster.convert.copy[RASTER_COPY_32];
    }

    if (blitLine)
    {
      int w = (int)ds.dsBm.bmWidth;
      int h = Math::abs(ds.dsBm.bmHeight);

      FOG_RETURN_ON_ERROR(self->create(SizeI(w, h), format));
      ImageData* d = self->_d;

      uint8_t* dstPixels = d->first;
      ssize_t dstStride = d->stride;

      const uint8_t* srcPixels = (const uint8_t*)ds.dsBm.bmBits;
      ssize_t srcStride = ds.dsBm.bmWidthBytes;

      // Bottom-To-Top.
      if (ds.dsBm.bmHeight > 0)
      {
        srcPixels += (ssize_t)(h - 1) * srcStride;
        srcStride = -srcStride;
      }

      for (int y = ds.dsBm.bmHeight - 1; y >= 0; y--, dstPixels += dstStride, srcPixels += srcStride)
      {
        blitLine(dstPixels, srcPixels, w, NULL);
      }
      return ERR_OK;
    }
  }

  // --------------------------------------------------------------------------
  // [BITMAP]
  // --------------------------------------------------------------------------

  if (GetObjectW(hBitmap, sizeof(BITMAP), &ds.dsBm) != 0)
  {
    HDC hdc;
    HBITMAP hOldBitmap;
    BITMAPINFO di;

    switch (ds.dsBm.bmBitsPixel)
    {
      case  1:
      case  4:
      case  8:
      case 16:
      case 24: format = IMAGE_FORMAT_RGB24; break;
      case 32: format = IMAGE_FORMAT_XRGB32; break;

      default:
        return ERR_IMAGE_INVALID_FORMAT;
    }

    FOG_RETURN_ON_ERROR(self->create(SizeI((int)ds.dsBm.bmWidth, (int)ds.dsBm.bmHeight), format));
    ImageData* d = self->_d;

    uint8_t* dstPixels = d->first;
    ssize_t dstStride = d->stride;

    // DDB bitmap.
    if ((hdc = CreateCompatibleDC(NULL)) == NULL)
    {
      self->reset();
      return OSUtil::getErrFromOSLastError();
    }

    if ((hOldBitmap = (HBITMAP)SelectObject(hdc, (HGDIOBJ)hBitmap)) == NULL)
    {
      DeleteDC(hdc);

      self->reset();
      return OSUtil::getErrFromOSLastError();
    }

    ZeroMemory(&di, sizeof(BITMAPINFO));
    di.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    di.bmiHeader.biCompression = BI_RGB;
    di.bmiHeader.biWidth = ds.dsBm.bmWidth;
    di.bmiHeader.biHeight = ds.dsBm.bmHeight;
    di.bmiHeader.biBitCount = d->bytesPerPixel * 8;
    di.bmiHeader.biPlanes = 1;

    for (int y = ds.dsBm.bmHeight - 1; y >= 0; y--, dstPixels += dstStride)
    {
      GetDIBits(hdc, hBitmap, y, 1, (LPVOID)dstPixels, &di, DIB_RGB_COLORS);
    }

    SelectObject(hdc, (HGDIOBJ)hOldBitmap);
    DeleteDC(hdc);
    return ERR_OK;
  }

  self->reset();
  return ERR_RT_INVALID_ARGUMENT;
}

static err_t FOG_CDECL Image_getDC(Image* self, HDC* _hDC)
{
  WinDibImageData* d = reinterpret_cast<WinDibImageData*>(self->_d);

  if (_hDC == NULL)
    return ERR_RT_INVALID_ARGUMENT;

  if (d->type != IMAGE_TYPE_WIN_DIB)
    return ERR_IMAGE_INVALID_TYPE;

  if (!d->isDetached())
  {
    FOG_RETURN_ON_ERROR(self->_detach());
    d = reinterpret_cast<WinDibImageData*>(self->_d);

    // Image::detach() must keep the current image format and type.
    FOG_ASSERT(d->type == IMAGE_TYPE_WIN_DIB);
  }

  FOG_ASSERT(d->hBitmap != NULL);

  HDC hDC = CreateCompatibleDC(NULL);
  if (hDC == NULL)
  {
    return OSUtil::getErrFromOSLastError();
  }

  HGDIOBJ hOldObj = SelectObject(hDC, d->hBitmap);
  if (hOldObj == (HGDIOBJ)GDI_ERROR)
  {
    DeleteDC(hDC);
    return OSUtil::getErrFromOSLastError();
  }

  d->locked++;
  *_hDC = hDC;

  return ERR_OK;
}

static err_t Image_releaseDC(Image* self, HDC hDC)
{
  WinDibImageData* d = reinterpret_cast<WinDibImageData*>(self->_d);

  if (hDC == NULL)
    return ERR_RT_INVALID_ARGUMENT;

  if (d->type != IMAGE_TYPE_WIN_DIB)
    return ERR_IMAGE_INVALID_TYPE;

  if (!DeleteDC(hDC))
    return OSUtil::getErrFromOSLastError();

  if (d->locked == 0)
  {
    // TODO: Logger
    return ERR_RT_INVALID_STATE;
  }
  else
  {
    d->locked--;
    return ERR_OK;
  }
}

#endif // FOG_OS_WINDOWS

// ============================================================================
// [Fog::Image - Equality]
// ============================================================================

static bool FOG_CDECL Image_eq(const Image* a, const Image* b)
{
  const ImageData* a_d = a->_d;
  const ImageData* b_d = b->_d;

  if (a_d == b_d)
    return true;

  uint32_t format = a_d->format;
  if (format != b_d->format)
    return false;
  
  uint32_t colorKey = a_d->colorKey;
  if (colorKey != b_d->colorKey)
    return false;

  int w = a_d->size.w;
  int h = a_d->size.h;

  if (w != b_d->size.w || h != b_d->size.h)
    return false;

  size_t aStride = Math::abs(a_d->stride);
  size_t bStride = Math::abs(b_d->stride);

  const uint8_t* aData = a_d->data;
  const uint8_t* bData = a_d->data;

  size_t bpl = w * a_d->bytesPerPixel;

  // ${IMAGE_FORMAT:BEGIN}
  switch (format)
  {
    case IMAGE_FORMAT_I8:
      if (a_d->palette() != b_d->palette())
        return false;
      // ... Fall through ...

    case IMAGE_FORMAT_PRGB32:
    case IMAGE_FORMAT_RGB24:
    case IMAGE_FORMAT_A8:
    case IMAGE_FORMAT_PRGB64:
    case IMAGE_FORMAT_RGB48:
    case IMAGE_FORMAT_A16:
      // Optimize the case that the both images contains ideally aligned buffer.
      if (aStride == bpl && bStride == bpl)
        return MemOps::eq(a_d->data, b_d->data, aStride * h);

      do {
        if (!MemOps::eq(aData, bData, bpl))
          return false;

        aData += aStride;
        bData += bStride;
      } while (--h);
      break;

    case IMAGE_FORMAT_XRGB32:
      do {
        for (uint i = 0; i < (uint)w; i++)
        {
          if ((reinterpret_cast<const uint32_t*>(aData)[i] | 0xFF000000) != 
              (reinterpret_cast<const uint32_t*>(bData)[i] | 0xFF000000))
            return false;
        }

        aData += aStride;
        bData += bStride;
      } while (--h);
      break;

    default:
      FOG_ASSERT_NOT_REACHED();
  }
  // ${IMAGE_FORMAT:END}

  return true;
}

// ============================================================================
// [Fog::Image - Helpers]
// ============================================================================

static ssize_t FOG_CDECL Image_getStrideFromWidth(int width, uint32_t depth)
{
  ssize_t result = 0;

  if (width <= 0)
    return result;

  switch (depth)
  {
    case  1:
      result = (width + 7) >> 3;
      goto _Align;

    case  4:
      result = (width + 1) >> 1;
      goto _Align;

    case  5:
    case  6:
    case  7:
    case  8:
      result = width;
      break;

    case 15:
    case 16:
      result = width * 2;
      break;

    case 24:
    case 32:
    case 48:
    case 64:
    case 96:
    case 128:
      result = width * (int)(depth >> 3);
      break;

    default:
      return 0;
  }

  // Overflow.
  if (result < width)
    return 0;

  // Align to 32-bit boudary.
_Align:
  result += 3;
  result &= ~(ssize_t)3;

  // Success.
  return result;
}

// ============================================================================
// [Fog::Image - GlyphFromPath]
// ============================================================================

struct FOG_NO_EXPORT GlyphFromPathFiller8 : public RasterFiller
{
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  FOG_INLINE GlyphFromPathFiller8(uint8_t* pixels, ssize_t stride)
  {
    this->_prepare = (RasterFiller::PrepareFunc)advanceFn;
    this->_process = (RasterFiller::ProcessFunc)processFn;
    this->_skip = (RasterFiller::SkipFunc)advanceFn;

    this->pixels = pixels;
    this->stride = stride;
  }

  // --------------------------------------------------------------------------
  // [Callbacks]
  // --------------------------------------------------------------------------

  static void FOG_FASTCALL advanceFn(GlyphFromPathFiller8* self, int y)
  {
    self->pixels += y * self->stride;
  }

  static void FOG_FASTCALL processFn(GlyphFromPathFiller8* self, RasterSpan* _span)
  {
    RasterSpan8* span = reinterpret_cast<RasterSpan8*>(_span);

    do {
      uint8_t* p = self->pixels + (uint)(span->getX0());
      uint8_t* m = reinterpret_cast<uint8_t*>(span->getGenericMask());

      uint i;
      uint len = (uint)span->getLength();

      switch (span->getType())
      {
        case RASTER_SPAN_C:
          memset(p, (0xFF * RasterSpan8::getConstMaskFromPointer(m)) >> 8, len);
          break;
        case RASTER_SPAN_A8_GLYPH:
        case RASTER_SPAN_AX_GLYPH:
          memcpy(p, m, len);
          break;
        case RASTER_SPAN_AX_EXTRA:
          for (i = 0; i < len; i++) p[i] = (uint8_t)((0xFF * ((uint16_t*)m)[i]) >> 8);
          break;
      }

      span = span->getNext();
    } while (span);

    self->pixels += self->stride;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint8_t* pixels;
  ssize_t stride;
};

template<typename NumT>
static err_t FOG_CDECL Image_glyphFromPath(Image* dst, PointI* dstOffset, const NumT_(Path)* path, uint32_t fillRule, uint32_t precision)
{
  err_t err = ERR_OK;
  NumT_(Box) boundingBox(UNINITIALIZED);

  int x0, y0;
  int x1, y1;
  int w, h;

  if (fillRule >= FILL_RULE_COUNT)
  {
    err = ERR_RT_INVALID_ARGUMENT;
    goto _Fail;
  }

  if (precision >= IMAGE_PRECISION_COUNT)
  {
    err = ERR_RT_INVALID_ARGUMENT;
    goto _Fail;
  }

  if (path->isEmpty())
    goto _Fail;

  err = path->getBoundingBox(boundingBox);
  if (FOG_IS_ERROR(err))
  {
    if (err == ERR_GEOMETRY_NONE)
      err = ERR_OK;
    goto _Fail;
  }

  x0 = (boundingBox.x0 >= 0.0) ? Math::ifloor(boundingBox.x0) : -Math::iceil(-boundingBox.x0);
  y0 = (boundingBox.y0 >= 0.0) ? Math::ifloor(boundingBox.y0) : -Math::iceil(-boundingBox.y0);

  x1 = (boundingBox.x1 >= 0.0) ? Math::iceil(boundingBox.x1) : -Math::ifloor(-boundingBox.x1);
  y1 = (boundingBox.y1 >= 0.0) ? Math::iceil(boundingBox.y1) : -Math::ifloor(-boundingBox.y1);

  dstOffset->set(x0, y0);
  w = x1 - x0;
  h = y1 - y0;

  switch (precision)
  {
    case IMAGE_PRECISION_BYTE:
    {
      PathRasterizer8 rasterizer;
      RasterScanline8 scanline;

      rasterizer.setSceneBox(BoxI(0, 0, w, h));
      rasterizer.setFillRule(FILL_RULE_NON_ZERO);
      rasterizer.setOpacity(0x100);

      if (FOG_IS_ERROR(rasterizer.init()))
        return rasterizer.getError();

      rasterizer.addPath(*path, NumT_(Point)(NumT(-x0), NumT(-y0)));
      rasterizer.finalize();

      if (!rasterizer.isValid())
      {
        err = rasterizer.getError();
        goto _Fail;
      }

      err = dst->create(SizeI(w, h), IMAGE_FORMAT_A8);
      if (FOG_IS_ERROR(err)) goto _Fail;

      uint8_t* pixels = dst->getFirstX();
      ssize_t stride = dst->getStride();

      MemOps::zero(pixels, dst->getHeight() * stride);

      GlyphFromPathFiller8 filler(pixels, stride);
      rasterizer.render(&filler, &scanline);
      break;
    }

    case IMAGE_PRECISION_WORD:
    {
      // TODO: Rasterizer precision.
      break;
    }

    default:
      FOG_ASSERT_NOT_REACHED();
  }

  if (FOG_IS_ERROR(err))
  {
_Fail:
    dst->reset();
    dstOffset->reset();
  }

  return err;
}

// ============================================================================
// [Fog::Image - Invert]
// ============================================================================

// Non premultiplied:
//
//   C'  = (1 - C)
//   A'  = (1 - A)
//
// Premultiplied:
//
//   Ca' = (1 - Ca/A) * (1 - A)
//   A'  = (1 - A)
//
// If the image format is premultiplied, but alpha is not modified then the
// formula looks like COMPOSITE_INVERT:
//
//   Ca' = A - Ca
//   A'  = A
static err_t FOG_CDECL Image_invert(Image* dst, const Image* src, const RectI* area, uint32_t channels)
{
  ImageData* dst_d = dst->_d;
  ImageData* src_d = src->_d;

  int x, y;
  int w, h;

  if (channels >= COLOR_CHANNEL_COUNT)
    return ERR_RT_INVALID_ARGUMENT;

  if (src_d->stride == 0)
  {
    dst->reset();
    return ERR_RT_INVALID_STATE;
  }

  if (area == NULL)
  {
    x = 0;
    y = 0;

    w = src_d->size.w;
    h = src_d->size.h;
  }
  else
  {
    x = area->x;
    y = area->y;

    int x1 = x + area->w;
    int y1 = y + area->h;

    if (x < 0) x = 0;
    if (y < 0) y = 0;

    if (x1 > src_d->size.w) x1 = src_d->size.w;
    if (y1 > src_d->size.h) y1 = src_d->size.h;

    if (x >= x1 || y >= y1)
    {
      dst->reset();
      return ERR_OK;
    }

    w = x1 - x;
    h = y1 - y;
  }

  uint32_t format = src_d->format;
  int bytesPerPixel = src_d->bytesPerPixel;

  const ImageFormatDescription& desc = ImageFormatDescription::getByFormat(format);
  if ((desc.getComponentMask() & IMAGE_COMPONENT_ALPHA) == 0) channels &= COLOR_CHANNEL_RGB;
  if ((desc.getComponentMask() & IMAGE_COMPONENT_RGB  ) == 0) channels &= COLOR_CHANNEL_ALPHA;

  if (w == src_d->size.w && h == src_d->size.h)
  {
    if (channels == NO_FLAGS)
      return dst->setImage(*src);

    FOG_RETURN_ON_ERROR(dst != src
      ? dst->create(src_d->size, format, src_d->type)
      : dst->detach());

    dst_d = dst->_d;
    src_d = src->_d;
  }
  else
  {
    if (channels == NO_FLAGS)
      return dst->setImage(*src, RectI(x, y, w, h));

    FOG_RETURN_ON_ERROR(dst->setImage(*src, RectI(x, y, w, h)));

    dst_d = dst->_d;
    src_d = dst->_d;

    x = 0;
    y = 0;
  }

  if (format == IMAGE_FORMAT_I8)
  {
    dst_d->colorKey = src_d->colorKey;
    dst_d->palette->setData(src_d->palette);
  }

  uint8_t* dPixels = dst_d->first;
  ssize_t dStride = dst_d->stride - w * dst_d->bytesPerPixel;

  const uint8_t* sPixels = src_d->first;
  ssize_t sStride = src_d->stride - w * src_d->bytesPerPixel;

  // ${IMAGE_FORMAT:BEGIN}
  switch (format)
  {
    case IMAGE_FORMAT_I8:
    {
      FOG_RETURN_ON_ERROR(dst_d->palette->detach());

      w = (int)dst_d->palette->getLength();
      h = 1;

      dPixels = reinterpret_cast<uint8_t*>(dst_d->palette->getDataX());
      sPixels = reinterpret_cast<const uint8_t*>(src_d->palette->getData());

      dStride = 0;
      sStride = 0;
      // ... Fall through ...
    }

    case IMAGE_FORMAT_PRGB32:
    case IMAGE_FORMAT_XRGB32:
    {
      Face::p32 mask = 0;

      if (channels & COLOR_CHANNEL_ALPHA) Face::p32Or(mask, mask, PIXEL_ARGB32_MASK_A);
      if (channels & COLOR_CHANNEL_RED  ) Face::p32Or(mask, mask, PIXEL_ARGB32_MASK_A);
      if (channels & COLOR_CHANNEL_GREEN) Face::p32Or(mask, mask, PIXEL_ARGB32_MASK_A);
      if (channels & COLOR_CHANNEL_BLUE ) Face::p32Or(mask, mask, PIXEL_ARGB32_MASK_A);

      if (format != IMAGE_FORMAT_PRGB32)
      {
        for (y = 0; y < h; y++, dPixels += dStride, sPixels += sStride)
        {
          for (x = 0; x < w; x++, dPixels += 4, sPixels += 4)
          {
            Face::p32 pix0p;

            Face::p32Load4a(pix0p, sPixels);
            Face::p32Xor(pix0p, pix0p, mask);
            Face::p32Store4a(dPixels, pix0p);
          }
        }

        if (format == IMAGE_FORMAT_I8)
        {
          dst_d->updatePalette(Range(0, dst_d->palette->getLength()));
        }
      }
      else if ((channels & COLOR_CHANNEL_ALPHA) == 0)
      {
        // If alpha is not inverted, the fast-path formula might be used.
        for (y = 0; y < h; y++, dPixels += dStride, sPixels += sStride)
        {
          for (x = 0; x < w; x++, dPixels += 4, sPixels += 4)
          {
            Face::p32 pix0p;
            Face::p32 pix0a;

            Face::p32Load4a(pix0p, sPixels);
            Face::p32ExpandPBBFromPBB3(pix0a, pix0p);
            Face::p32Sub(pix0a, pix0a, pix0p);
            Face::p32And(pix0p, pix0p, ~mask);
            Face::p32And(pix0a, pix0a, mask);
            Face::p32Or(pix0p, pix0p, pix0a);
            Face::p32Store4a(dPixels, pix0p);
          }
        }
      }
      else
      {
        // Alpha is inverted: Demultiply, invert, and premultiply.
        for (y = 0; y < h; y++, dPixels += dStride, sPixels += sStride)
        {
          for (x = 0; x < w; x++, dPixels += 4, sPixels += 4)
          {
            Face::p32 pix0p;

            Face::p32Load4a(pix0p, sPixels);
            Face::p32ARGB32FromPRGB32(pix0p, pix0p);
            Face::p32Xor(pix0p, pix0p, mask);
            Face::p32PRGB32FromARGB32(pix0p, pix0p);
            Face::p32Store4a(dPixels, pix0p);
          }
        }
      }
      break;
    }

    case IMAGE_FORMAT_RGB24:
    {
#if FOG_BYTE_ORDER == FOG_LITTLE_ENDIAN
      uint8_t m0 = (channels & COLOR_CHANNEL_BLUE ) ? 0xFF : 0x00;
      uint8_t m1 = (channels & COLOR_CHANNEL_GREEN) ? 0xFF : 0x00;
      uint8_t m2 = (channels & COLOR_CHANNEL_RED  ) ? 0xFF : 0x00;
#else
      uint8_t m0 = (channels & COLOR_CHANNEL_RED  ) ? 0xFF : 0x00;
      uint8_t m1 = (channels & COLOR_CHANNEL_GREEN) ? 0xFF : 0x00;
      uint8_t m2 = (channels & COLOR_CHANNEL_BLUE ) ? 0xFF : 0x00;
#endif // FOG_BYTE_ORDER

      for (y = 0; y < h; y++, dPixels += dStride, sPixels += sStride)
      {
        for (x = 0; x < w; x++, dPixels += 3, sPixels += 3)
        {
          dPixels[0] = sPixels[0] ^ m0;
          dPixels[1] = sPixels[1] ^ m1;
          dPixels[2] = sPixels[2] ^ m2;
        }
      }
      break;
    }

    case IMAGE_FORMAT_A8:
    {
      for (y = 0; y < h; y++, dPixels += dStride, sPixels += sStride)
      {
        for (x = 0; x < w; x++, dPixels += 1, sPixels += 1)
        {
          uint8_t pix0p = sPixels[0];
          pix0p ^= 0xFF;
          dPixels[0] = pix0p;
        }
      }
      break;
    }

    case IMAGE_FORMAT_PRGB64:
    {
      Face::p64 mask;
      Face::p64Clear(mask);

      if (channels & COLOR_CHANNEL_ALPHA) Face::p64Or(mask, mask, Face::p64FromU64(PIXEL_ARGB64_MASK_A));
      if (channels & COLOR_CHANNEL_RED  ) Face::p64Or(mask, mask, Face::p64FromU64(PIXEL_ARGB64_MASK_R));
      if (channels & COLOR_CHANNEL_GREEN) Face::p64Or(mask, mask, Face::p64FromU64(PIXEL_ARGB64_MASK_G));
      if (channels & COLOR_CHANNEL_BLUE ) Face::p64Or(mask, mask, Face::p64FromU64(PIXEL_ARGB64_MASK_B));

      if ((channels & COLOR_CHANNEL_ALPHA) == 0)
      {
        // If alpha is not inverted, the fast-path formula might be used.
        Face::p64 maski;
        Face::p64Neg(maski, mask);

        for (y = 0; y < h; y++, dPixels += dStride, sPixels += sStride)
        {
          for (x = 0; x < w; x++, dPixels += 8, sPixels += 8)
          {
            Face::p64 pix0p;
            Face::p64 pix0a;

            Face::p64Load8a(pix0p, sPixels);
            Face::p64ExpandPWWFromPWW3(pix0p, pix0p);

            Face::p64Sub(pix0a, pix0a, pix0p);
            Face::p64And(pix0p, pix0p, maski);
            Face::p64And(pix0a, pix0a, mask);
            Face::p64Or(pix0p, pix0p, pix0a);
            Face::p64Store8a(dPixels, pix0p);
          }
        }
      }
      else
      {
        // Alpha is inverted: Demultiply, invert, and premultiply.
        for (y = 0; y < h; y++, dPixels += dStride, sPixels += sStride)
        {
          for (x = 0; x < w; x++, dPixels += 8, sPixels += 8)
          {
            Face::p64 pix0p;

            Face::p64Load8a(pix0p, sPixels);
            Face::p64ARGB64FromPRGB64(pix0p, pix0p);
            Face::p64Xor(pix0p, pix0p, mask);
            Face::p64PRGB64FromARGB64(pix0p, pix0p);
            Face::p64Store8a(dPixels, pix0p);
          }
        }
      }
      break;
    }

    case IMAGE_FORMAT_RGB48:
    {
#if FOG_BYTE_ORDER == FOG_LITTLE_ENDIAN
      uint16_t m0 = (channels & COLOR_CHANNEL_BLUE ) ? 0xFFFF : 0x0000;
      uint16_t m1 = (channels & COLOR_CHANNEL_GREEN) ? 0xFFFF : 0x0000;
      uint16_t m2 = (channels & COLOR_CHANNEL_RED  ) ? 0xFFFF : 0x0000;
#else
      uint16_t m0 = (channels & COLOR_CHANNEL_RED  ) ? 0xFFFF : 0x0000;
      uint16_t m1 = (channels & COLOR_CHANNEL_GREEN) ? 0xFFFF : 0x0000;
      uint16_t m2 = (channels & COLOR_CHANNEL_BLUE ) ? 0xFFFF : 0x0000;
#endif // FOG_BYTE_ORDER

      for (y = 0; y < h; y++, dPixels += dStride, sPixels += sStride)
      {
        for (x = 0; x < w; x++, dPixels += 6, sPixels += 6)
        {
          ((uint16_t*)dPixels)[0] = ((const uint16_t*)sPixels)[0] ^ m0;
          ((uint16_t*)dPixels)[1] = ((const uint16_t*)sPixels)[1] ^ m1;
          ((uint16_t*)dPixels)[2] = ((const uint16_t*)sPixels)[2] ^ m2;
        }
      }
      break;
    }

    case IMAGE_FORMAT_A16:
    {
      for (y = 0; y < h; y++, dPixels += dStride, sPixels += sStride)
      {
        for (x = 0; x < w; x++, dPixels += 2, sPixels += 2)
        {
          uint16_t pix0p = ((const uint16_t*)sPixels)[0];
          pix0p ^= 0xFFFF;
          ((uint16_t*)dPixels)[0] = pix0p;
        }
      }
      break;
    }
  }
  // ${IMAGE_FORMAT:END}

  dst->_modified();
  return ERR_OK;
}

// ============================================================================
// [Fog::Image - Mirror]
// ============================================================================

typedef void (FOG_FASTCALL *MirrorFunc)(uint8_t*, uint8_t*, int);

template<int SIZE>
static void FOG_FASTCALL _MirrorCopySrcIsNotDst(uint8_t* dst, uint8_t* src, int w)
{
  MemOps::copy(dst, src, w * SIZE);
}

template<int SIZE>
static void FOG_FASTCALL _MirrorFlipSrcIsNotDst(uint8_t* dst, uint8_t* src, int w)
{
  src += w * SIZE - SIZE;
  for (int x = 0; x < w; x++, dst += SIZE, src -= SIZE)
    MemOps::copy_s<SIZE>(dst, src);
}

template<int SIZE>
static void FOG_FASTCALL _MirrorCopySrcIsDst(uint8_t* dst, uint8_t* src, int w)
{
  MemOps::xchg(dst, src, w * SIZE);
}

template<int SIZE>
static void FOG_FASTCALL _MirrorFlipSrcIsDst(uint8_t* dst, uint8_t* src, int w)
{
  int x = w;
  if (src == dst) x >>= 1;

  src += w * SIZE - SIZE;
  for (; x; x--, dst += SIZE, src -= SIZE)
    MemOps::xchg_s<SIZE>(dst, src);
}

static const MirrorFunc _MirrorFuncsCopySrcIsNotDst[] =
{
  NULL,
  _MirrorCopySrcIsNotDst<1>,
  _MirrorCopySrcIsNotDst<2>,
  _MirrorCopySrcIsNotDst<3>,
  _MirrorCopySrcIsNotDst<4>,
  NULL,
  _MirrorCopySrcIsNotDst<6>,
  NULL,
  _MirrorCopySrcIsNotDst<8>
};

static const MirrorFunc _MirrorFuncsFlipSrcIsNotDst[] =
{
  NULL,
  _MirrorFlipSrcIsNotDst<1>,
  _MirrorFlipSrcIsNotDst<2>,
  _MirrorFlipSrcIsNotDst<3>,
  _MirrorFlipSrcIsNotDst<4>,
  NULL,
  _MirrorFlipSrcIsNotDst<6>,
  NULL,
  _MirrorFlipSrcIsNotDst<8>
};

static const MirrorFunc _MirrorFuncsCopySrcIsDst[] =
{
  NULL,
  _MirrorCopySrcIsDst<1>,
  _MirrorCopySrcIsDst<2>,
  _MirrorCopySrcIsDst<3>,
  _MirrorCopySrcIsDst<4>,
  NULL,
  _MirrorCopySrcIsDst<6>,
  NULL,
  _MirrorCopySrcIsDst<8>
};

static const MirrorFunc _MirrorFuncsFlipSrcIsDst[] =
{
  NULL,
  _MirrorFlipSrcIsDst<1>,
  _MirrorFlipSrcIsDst<2>,
  _MirrorFlipSrcIsDst<3>,
  _MirrorFlipSrcIsDst<4>,
  NULL,
  _MirrorFlipSrcIsDst<6>,
  NULL,
  _MirrorFlipSrcIsDst<8>
};

static err_t FOG_CDECL Image_mirror(Image* dst, const Image* src, const RectI* area, uint32_t mirrorMode)
{
  ImageData* dst_d = dst->_d;
  ImageData* src_d = src->_d;

  int x, y;
  int w, h;

  if (mirrorMode >= IMAGE_MIRROR_COUNT)
  {
    return ERR_RT_INVALID_ARGUMENT;
  }

  if (src_d->stride == 0)
  {
    dst->reset();
    return ERR_RT_INVALID_STATE;
  }

  if (area == NULL)
  {
    x = 0;
    y = 0;

    w = src_d->size.w;
    h = src_d->size.h;
  }
  else
  {
    x = area->x;
    y = area->y;

    int x1 = x + area->w;
    int y1 = y + area->h;

    if (x < 0) x = 0;
    if (y < 0) y = 0;

    if (x1 > src_d->size.w) x1 = src_d->size.w;
    if (y1 > src_d->size.h) y1 = src_d->size.h;

    if (x >= x1 || y >= y1)
    {
      dst->reset();
      return ERR_OK;
    }

    w = x1 - x;
    h = y1 - y;
  }

  uint32_t format = src_d->format;
  int bytesPerPixel = src_d->bytesPerPixel;

  if (w == src_d->size.w && h == src_d->size.h)
  {
    // Can't be anything else if we are here.
    FOG_ASSERT(x == 0);
    FOG_ASSERT(y == 0);

    if (mirrorMode == IMAGE_MIRROR_NONE)
      return dst->setImage(*src);

    FOG_RETURN_ON_ERROR(dst != src
      ? dst->create(src_d->size, format)
      : dst->detach());

    dst_d = dst->_d;
    src_d = src->_d;
  }
  else
  {
    if (mirrorMode == IMAGE_MIRROR_NONE)
      return dst->setImage(*src, RectI(x, y, w, h));

    FOG_RETURN_ON_ERROR(dst != src
      ? dst->create(SizeI(w, h), format, src_d->type)
      : dst->setImage(*src, RectI(x, y, w, h)));

    dst_d = dst->_d;
    src_d = src->_d;

    if (dst == src)
    {
      x = 0;
      y = 0;
    }
  }

  if (format == IMAGE_FORMAT_I8)
  {
    dst_d->colorKey = src_d->colorKey;
    dst_d->palette->setData(src_d->palette);
  }

  ssize_t dStride = dst_d->stride;
  ssize_t sStride = src_d->stride;

  uint8_t* dPixels = dst_d->first;
  uint8_t* sPixels = src_d->first + y * sStride + x * bytesPerPixel;

  MirrorFunc mirrorFunc = NULL;

  switch (mirrorMode)
  {
    case IMAGE_MIRROR_VERTICAL:
      sPixels += sStride * ((ssize_t)h - 1);
      sStride = -sStride;

      if (dst_d != src_d)
      {
        mirrorFunc = _MirrorFuncsCopySrcIsNotDst[bytesPerPixel];
      }
      else
      {
        mirrorFunc = _MirrorFuncsCopySrcIsDst[bytesPerPixel];
        h >>= 1;
      }
      break;

    case IMAGE_MIRROR_HORIZONTAL:
      if (dst_d != src_d)
        mirrorFunc = _MirrorFuncsFlipSrcIsNotDst[bytesPerPixel];
      else
        mirrorFunc = _MirrorFuncsFlipSrcIsDst[bytesPerPixel];
      break;

    case IMAGE_MIRROR_BOTH:
      sPixels += sStride * ((ssize_t)h - 1);
      sStride = -sStride;

      if (dst_d != src_d)
      {
        mirrorFunc = _MirrorFuncsFlipSrcIsNotDst[bytesPerPixel];
      }
      else
      {
        mirrorFunc = _MirrorFuncsFlipSrcIsDst[bytesPerPixel];
        h >>= 1;
      }
      break;
  }

  for (int y = h; y; y--, dPixels += dStride, sPixels += sStride)
  {
    mirrorFunc(dPixels, sPixels, w);
  }

  dst->_modified();
  return ERR_OK;
}

// ============================================================================
// [Fog::Image - Rotate]
// ============================================================================

static err_t FOG_CDECL Image_rotate(Image* dst, const Image* src, const RectI* area, uint32_t rotateMode)
{
  ImageData* dst_d = dst->_d;
  ImageData* src_d = src->_d;

  int x, y;
  int w, h;

  if (rotateMode >= IMAGE_ROTATE_COUNT)
    return ERR_RT_INVALID_ARGUMENT;

  // Rotation by 180 degrees has the same effect as MIRROR_BOTH.
  if (rotateMode == IMAGE_ROTATE_180)
    return _api.image_mirror(dst, src, area, IMAGE_MIRROR_BOTH);

  if (src_d->stride == 0)
  {
    dst->reset();
    return ERR_RT_INVALID_STATE;
  }

  if (area == NULL)
  {
    x = 0;
    y = 0;

    w = src_d->size.w;
    h = src_d->size.h;
  }
  else
  {
    x = area->x;
    y = area->y;

    int x1 = x + area->w;
    int y1 = y + area->h;

    if (x < 0) x = 0;
    if (y < 0) y = 0;

    if (x1 > src_d->size.w) x1 = src_d->size.w;
    if (y1 > src_d->size.h) y1 = src_d->size.h;

    if (x >= x1 || y >= y1)
    {
      dst->reset();
      return ERR_OK;
    }

    w = x1 - x;
    h = y1 - y;
  }

  if (rotateMode == IMAGE_ROTATE_0)
  {
    if (w == src_d->size.w && h == src_d->size.h)
      return dst->setImage(*src);
    else
      return dst->setImage(*src, RectI(x, y, w, h));
  }

  // Now we have only two possibilities:
  // - rotate by 90.
  // - rotate by 270.

  uint32_t format = src_d->format;
  int bytesPerPixel = src_d->bytesPerPixel;

  if (dst == src)
    src_d->reference.inc();

  err_t err = dst->create(SizeI(w, h), format, src_d->type);
  if (FOG_IS_ERROR(err))
  {
    if (dst == src)
      src_d->release();
    return err;
  }

  dst_d = dst->_d;

  if (format == IMAGE_FORMAT_I8)
  {
    dst_d->colorKey = src_d->colorKey;
    dst_d->palette->setData(src_d->palette);
  }

  ssize_t dStride = dst_d->stride;
  ssize_t sStride = src_d->stride;

  uint8_t* dPixels = dst_d->first;
  uint8_t* sPixels = src_d->first + y * sStride + x * bytesPerPixel;

  uint8_t* dPtr;
  uint8_t* sPtr;

  ssize_t srcInc1;
  ssize_t srcInc2;

  if (rotateMode == IMAGE_ROTATE_90)
  {
    sPixels += (ssize_t)(src_d->size.h - 1) * sStride;
    srcInc1 = bytesPerPixel;
    srcInc2 = -sStride;
  }
  else
  {
    sPixels += (ssize_t)(src_d->size.w - 1) * bytesPerPixel;
    srcInc1 = -bytesPerPixel;
    srcInc2 = sStride;
  }

  switch (bytesPerPixel)
  {
#define _FOG_ROTATE_LOOP(_Size_) \
    FOG_MACRO_BEGIN \
      for (int i = w; i; i--, dPixels += dStride, sPixels += srcInc1) \
      { \
        dPtr = dPixels; \
        sPtr = sPixels; \
        \
        for (int j = h; j; j--, dPtr += _Size_, sPtr += srcInc2) \
        { \
          MemOps::copy_s<_Size_>(dPtr, sPtr); \
        } \
      } \
    FOG_MACRO_END

    case 1: _FOG_ROTATE_LOOP(1); break;
    case 2: _FOG_ROTATE_LOOP(2); break;
    case 3: _FOG_ROTATE_LOOP(3); break;
    case 4: _FOG_ROTATE_LOOP(4); break;
    case 6: _FOG_ROTATE_LOOP(6); break;
    case 8: _FOG_ROTATE_LOOP(8); break;

    default:
      FOG_ASSERT_NOT_REACHED();
  }

  if (dst == src)
    src_d->release();

  dst->_modified();
  return ERR_OK;
}

// ============================================================================
// [Fog::Image - ImageData]
// ============================================================================

static ImageData* FOG_CDECL Image_dAddRef(ImageData* d)
{
  d->reference.inc();
  return d;
}

static void FOG_CDECL Image_dRelease(ImageData* d)
{
  if (d->reference.deref())
  {
    bool wasStatic = (d->vType & VAR_FLAG_STATIC) != 0;

    // FATAL: Image dereferenced during painting.
    if (d->locked)
    {
      Debug::failFunc("Fog::ImageData", "release", "ImageData released during painting.\n");
    }

    d->destroy();
  }
}

// ============================================================================
// [Init / Fini]
// ============================================================================

FOG_NO_EXPORT void Image_init(void)
{
  // --------------------------------------------------------------------------
  // [Funcs]
  // --------------------------------------------------------------------------

  _api.image_ctor = Image_ctor;
  _api.image_ctorCopy = Image_ctorCopy;
  _api.image_ctorCreate = Image_ctorCreate;
  _api.image_dtor = Image_dtor;

  _api.image_detach = Image_detach;

  _api.image_getAlphaDistribution = Image_getAlphaDistribution;
  _api.image_modified = Image_modified;

  _api.image_reset = Image_reset;
  _api.image_create = Image_create;
  _api.image_adopt = Image_adopt;

  _api.image_copy = Image_copy;
  _api.image_copyDeep = Image_copyDeep;
  _api.image_setImage = Image_setImage;

  _api.image_convert = Image_convert;
  _api.image_forceFormat = Image_forceFormat;

  _api.image_convertTo8BitDepth = Image_convertTo8BitDepth;
  _api.image_convertTo8BitDepthPalette = Image_convertTo8BitDepthPalette;

  _api.image_setPalette = Image_setPalette;
  _api.image_setPaletteData = Image_setPaletteData;

  _api.image_clearArgb32 = Image_clearArgb32;
  _api.image_clearColor = Image_clearColor;

  _api.image_fillRectArgb32 = Image_fillRectArgb32;
  _api.image_fillRectColor = Image_fillRectColor;

  _api.image_blitImageAt = Image_blitImageAt;

  _api.image_scroll = Image_scroll;

  _api.image_readFromFile = Image_readFromFile;
  _api.image_readFromStream = Image_readFromStream;
  _api.image_readFromBufferStringA = Image_readFromBufferStringA;
  _api.image_readFromBufferRaw = Image_readFromBufferRaw;

  _api.image_writeToFile = Image_writeToFile;
  _api.image_writeToStream = Image_writeToStream;
  _api.image_writeToBuffer = Image_writeToBuffer;

#if defined(FOG_OS_WINDOWS)
  _api.image_toWinBitmap = Image_toWinBitmap;
  _api.image_fromWinBitmap = Image_fromWinBitmap;
  _api.image_getDC = Image_getDC;
  _api.image_releaseDC = Image_releaseDC;
#endif // FOG_OS_WINDOWS
  
  _api.image_eq = Image_eq;

  _api.image_getStrideFromWidth = Image_getStrideFromWidth;

  _api.image_glyphFromPathF = Image_glyphFromPath<float>;
  _api.image_glyphFromPathD = Image_glyphFromPath<double>;

  _api.image_invert = Image_invert;
  _api.image_mirror = Image_mirror;
  _api.image_rotate = Image_rotate;

  _api.image_dAddRef = Image_dAddRef;
  _api.image_dRelease = Image_dRelease;

  // Virtual tables.
  _api.image_vTable[IMAGE_TYPE_BUFFER] = &Image_Buffer_vTable;

#if defined(FOG_OS_WINDOWS)
  _api.image_vTable[IMAGE_TYPE_WIN_DIB] = &Image_WinDib_vTable;
#endif // FOG_OS_WINDOWS

  // --------------------------------------------------------------------------
  // [Data]
  // --------------------------------------------------------------------------

  ImageData* d = &Image_dEmpty;
  d->reference.init(1);
  d->vType = VAR_TYPE_IMAGE | VAR_FLAG_NONE;

  d->locked = 0;
  d->vtable = &Image_Buffer_vTable;
  d->format = IMAGE_FORMAT_NULL;
  d->type = IMAGE_TYPE_BUFFER;
  d->adopted = 0;
  d->colorKey = IMAGE_COLOR_KEY_NONE;
  d->bytesPerPixel = 0;
  d->palette.initCustom1(_api.imagepalette_oEmpty->_d);

  _api.image_oEmpty = Image_oEmpty.initCustom1(d);
}

} // Fog namespace







































// TODO: These were removed from the older fog and it's time to reimplement them
// back.

#if 0
// ============================================================================
// [Fog::Image - Color Filter]
// ============================================================================

static err_t applyColorFilter(Image& im, const BoxI& box, ColorFilterFunc fn, const void* context)
{
  // Clip.
  int imgw = im.getWidth();
  int imgh = im.getHeight();

  int x0 = Math::max<int>(box.getX0(), 0);
  int y0 = Math::max<int>(box.getY0(), 0);
  int x2 = Math::min<int>(box.getX1(), imgw);
  int y2 = Math::min<int>(box.getY1(), imgh);

  if (x0 >= x2 || y0 >= y2) return ERR_OK;

  int w = x2 - x0;
  int h = y2 - y0;

  FOG_RETURN_ON_ERROR(im.detach());

  uint8_t* imData = im.getDataX();
  ssize_t imStride = im.getStride();
  ssize_t imBpp = im.getBytesPerPixel();

  size_t y;

  uint8_t* cur = imData + (ssize_t)y0 * imStride + (ssize_t)x0 * imBpp;
  for (y = (uint)h; y; y--, cur += imStride) fn(context, cur, cur, w);

  return ERR_OK;
}

err_t Image::filter(const ColorFilter& f, const RectI* area)
{
  BoxI abox(0, 0, getWidth(), getHeight());
  if (area) abox.set(area->getX0(), area->getY0(), area->getX1(), area->getY1());

  ColorFilterFunc fn = f.getEngine()->getColorFilterFunc(getFormat());
  if (!fn) return ERR_IMAGE_UNSUPPORTED_FORMAT;

  const void* context = f.getEngine()->getContext();
  err_t err = applyColorFilter(*this, abox, fn, context);

  f.getEngine()->releaseContext(context);
  return err;
}

err_t Image::filter(const FeColorLutFx& lut, const RectI* area)
{
  BoxI abox(0, 0, getWidth(), getHeight());
  if (area) abox.set(area->getX0(), area->getY0(), area->getX1(), area->getY1());

  ColorFilterFunc fn = (ColorFilterFunc)rasterFuncs.filter.color_lut[getFormat()];
  if (!fn) return ERR_IMAGE_UNSUPPORTED_FORMAT;

  return applyColorFilter(*this, abox, fn, lut.getData());
}

err_t Image::filter(const FeColorMatrix& cm, const RectI* area)
{
  BoxI abox(0, 0, getWidth(), getHeight());
  if (area) abox.set(area->getX0(), area->getY0(), area->getX1(), area->getY1());

  ColorFilterFunc fn = (ColorFilterFunc)rasterFuncs.filter.color_matrix[getFormat()];
  if (!fn) return ERR_IMAGE_UNSUPPORTED_FORMAT;

  return applyColorFilter(*this, abox, fn, &cm);
}

// ============================================================================
// [Fog::Image - Image Filter]
// ============================================================================

static err_t applyImageFilter(Image& im, const BoxI& box, const ImageFxFilter& filter)
{
  // Never call applyImageFilter() with color filter, see applyColorFilter().
  FOG_ASSERT((filter.getCharacteristics() & IMAGE_EFFECT_CHAR_COLOR_TRANSFORM) == 0);

  // Clip.
  int imgw = im.getWidth();
  int imgh = im.getHeight();
  int imgf = im.getFormat();

  int x0 = Math::max<int>(box.getX0(), 0);
  int y0 = Math::max<int>(box.getY0(), 0);
  int x2 = Math::min<int>(box.getX1(), imgw);
  int y2 = Math::min<int>(box.getY1(), imgh);

  if (x0 >= x2 || y0 >= y2) return ERR_OK;

  int w = x2 - x0;
  int h = y2 - y0;

  uint32_t filterFormat = imgf;
  uint32_t filterCharacteristics = filter.getCharacteristics();

  if ((filterCharacteristics & (IMAGE_EFFECT_CHAR_HV_PROCESSING | IMAGE_EFFECT_CHAR_ENTIRE_PROCESSING)) == 0)
  {
    // NOP.
    return ERR_OK;
  }

  FOG_RETURN_ON_ERROR(im.detach());

  const void* context = filter.getEngine()->getContext();
  ImageFilterFn fn;

  uint8_t* imData = im.getDataX();
  ssize_t imStride = im.getStride();
  ssize_t imBpp = im.getBytesPerPixel();

  uint8_t* imBegin = imData + (ssize_t)y0 * imStride + (ssize_t)x0 * imBpp;
  uint8_t* imCur = imBegin;

  // Demultiply if needed.
  if (imgf == IMAGE_FORMAT_PRGB32 && (filterCharacteristics & IMAGE_EFFECT_CHAR_SUPPORTS_PRGB32) == 0)
  {
    RasterVBlitLineFunc vblit_line =
      rasterFuncs.dib.convert[IMAGE_FORMAT_ARGB32][IMAGE_FORMAT_PRGB32];
    for (int y = h; y; y--, imCur += imStride) vblit_line(imCur, imCur, w, NULL);

    imCur = imBegin;
    filterFormat = IMAGE_FORMAT_ARGB32;
  }

  err_t err = ERR_OK;

  // Vertical & Horizontal processing.
  if ((filterCharacteristics & IMAGE_EFFECT_CHAR_HV_PROCESSING) == IMAGE_EFFECT_CHAR_HV_PROCESSING)
  {
    fn = filter.getEngine()->getImageFilterFunc(filterFormat, IMAGE_EFFECT_CHAR_VERT_PROCESSING);
    if (!fn) { err = ERR_IMAGE_UNSUPPORTED_FORMAT; goto _End; }

    fn(context, imCur, imStride, imCur, imStride, w, h, -1);

    fn = filter.getEngine()->getImageFilterFunc(filterFormat, IMAGE_EFFECT_CHAR_HORZ_PROCESSING);
    if (!fn) { err = ERR_IMAGE_UNSUPPORTED_FORMAT; goto _End; }

    fn(context, imCur, imStride, imCur, imStride, w, h, -1);
  }
  // Vertical processing only (one pass).
  else if ((filterCharacteristics & IMAGE_EFFECT_CHAR_VERT_PROCESSING) != 0)
  {
    fn = filter.getEngine()->getImageFilterFunc(filterFormat, IMAGE_EFFECT_CHAR_VERT_PROCESSING);
    if (!fn) { err = ERR_IMAGE_UNSUPPORTED_FORMAT; goto _End; }

    fn(context, imCur, imStride, imCur, imStride, w, h, -1);
  }
  // Horizontal processing only (one pass).
  else if ((filterCharacteristics & IMAGE_EFFECT_CHAR_HORZ_PROCESSING) != 0)
  {
    fn = filter.getEngine()->getImageFilterFunc(filterFormat, IMAGE_EFFECT_CHAR_HORZ_PROCESSING);
    if (!fn) { err = ERR_IMAGE_UNSUPPORTED_FORMAT; goto _End; }

    fn(context, imCur, imStride, imCur, imStride, w, h, -1);
  }
  // Entire processing (one pass).
  else if ((filterCharacteristics & IMAGE_EFFECT_CHAR_ENTIRE_PROCESSING) != 0)
  {
    fn = filter.getEngine()->getImageFilterFunc(filterFormat, IMAGE_EFFECT_CHAR_ENTIRE_PROCESSING);
    if (!fn) { err = ERR_IMAGE_UNSUPPORTED_FORMAT; goto _End; }

    fn(context, imCur, imStride, imCur, imStride, w, h, -1);
  }

  // Premultiply if demultiplied.
  if (imgf == IMAGE_FORMAT_PRGB32 && (filterCharacteristics & IMAGE_EFFECT_CHAR_SUPPORTS_PRGB32) == 0)
  {
    RasterVBlitLineFunc vblit_line =
      rasterFuncs.dib.convert[IMAGE_FORMAT_PRGB32][IMAGE_FORMAT_ARGB32];
    for (int y = h; y; y--, imCur += imStride) vblit_line(imCur, imCur, w, NULL);
  }

_End:
  filter.getEngine()->releaseContext(context);
  return err;
}

err_t Image::filter(const ImageFxFilter& f, const RectI* area)
{
  // Use optimized way for ColorFilter if image doest color transform.
  if (f.getCharacteristics() & IMAGE_EFFECT_CHAR_COLOR_TRANSFORM)
  {
    return filter(reinterpret_cast<const ColorFilter&>(f), area);
  }

  BoxI abox(0, 0, getWidth(), getHeight());
  if (area) abox.set(area->getX0(), area->getY0(), area->getX1(), area->getY1());

  return applyImageFilter(*this, abox, f);
}
#endif

// ============================================================================
// [Fog::Image - Filter]
// ============================================================================

#if 0
err_t Image::filter(
  const ImageFx& fx,
  Image& dst, PointI* dstOffset,
  const Image& src, const RectI* srcFragment)
{
  ImageFxFilter fxFilter;
  FOG_RETURN_ON_ERROR(fxFilter.setEffect(fx));

  return filter(fxFilter, dst, dstOffset, src, srcFragment);
}

err_t Image::filter(
  const ImageFxFilter& fxFilter,
  Image& dst, PointI* dstOffset,
  const Image& src, const RectI* srcFragment)
{
  // TODO:
  return ERR_RT_NOT_IMPLEMENTED;
}

Image Image::filtered(const ImageFx& fx, const RectI* srcFragment)
{
  Image dst;
  ImageFxFilter fxFilter;

  if (fxFilter.setEffect(fx) == ERR_OK)
  {
    filter(fxFilter, dst, NULL, *this, srcFragment);
  }

  return dst;
}

Image Image::filtered(const ImageFxFilter& fxFilter, const RectI* srcFragment)
{
  Image dst;
  filter(fxFilter, dst, NULL, *this, srcFragment);
  return dst;
}
#endif

// ============================================================================
// [Fog::Image - Scale]
// ============================================================================

#if 0
Image Image::scaled(const SizeI& to, uint32_t interpolationType) const
{
  Image dst;

  if (isEmpty() || (getWidth() == to.w && getHeight() == to.h))
  {
    dst = *this;
    return dst;
  }

  if (to.w == 0 || to.h == 0) return dst;
  if (dst.create(to, getFormat()) != ERR_OK) return dst;

  RasterPattern ctx;
  err_t err = rasterFuncs.pattern.texture_init_scale(&ctx, *this, to.w, to.h, interpolationType);
  if (FOG_IS_ERROR(err)) { dst.reset(); return dst; }

  uint8_t* dstData = (uint8_t*)dst.getData();
  ssize_t dstStride = dst.getStride();

  RasterSpanExt8 span;
  span.setPositionAndType(0, to.w, RASTER_SPAN_C);
  span.setCMask(0xFF);
  span.setNext(NULL);

  for (int y = 0; y < to.h; y++, dstData += dstStride)
  {
    ctx.fetch(&ctx, &span, dstData, y, RASTER_FETCH_COPY);
  }

  ctx.destroy(&ctx);
  return dst;
}
#endif
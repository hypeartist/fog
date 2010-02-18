// [Fog/Gui Library - Public API]
//
// [Licence]
// MIT, See COPYING file in package

// [Guard]
#ifndef _FOG_GUI_GUIENGINE_H
#define _FOG_GUI_GUIENGINE_H

// [Dependencies]
#include <Fog/Core/Hash.h>
#include <Fog/Core/Library.h>
#include <Fog/Core/Object.h>
#include <Fog/Graphics/Argb.h>
#include <Fog/Graphics/Geometry.h>
#include <Fog/Graphics/Image.h>
#include <Fog/Gui/Constants.h>

//! @addtogroup Fog_Gui
//! @{

namespace Fog {

// ============================================================================
// [Forward Declarations]
// ============================================================================

struct Application;
struct CancelableTask;
struct String;
struct Widget;

// Defined here.
struct GuiBackBuffer;
struct GuiEngine;
struct GuiWindow;

// ============================================================================
// [Fog::UIEngine]
// ============================================================================

//! @brief Base for @c Fog::Widget or @c Fog::Layout classes.
//!
//! Layout item can be imagined as interface. It contains pure virtual methods
//! that's overriden by widgets or layouts.
struct FOG_API GuiEngine : public Object
{
  FOG_DECLARE_OBJECT(GuiEngine, Object)

  // [Structures]

  //! @brief Display information.
  struct DisplayInfo
  {
    //! @brief Screen width.
    uint32_t width;
    //! @brief Screen height.
    uint32_t height;
    //! @brief Screen depth.
    uint32_t depth;

    //! @brief Screen red mask.
    uint32_t rMask;
    //! @brief Screen green mask.
    uint32_t gMask;
    //! @brief Screen blue mask.
    uint32_t bMask;
    //! @brief If true, 16 bit depth is byteswapped (X Server). In other depths
    //! are byteswapped instead rMask, gMask and bMask values.
    uint32_t is16BitSwapped;
  };

  //! @brief Palette information.
  struct PaletteInfo
  {
    //! @brief Count of entities in palConv[] 2 to 256.
    uint32_t palCount;
    //! @brief Palette conversion (image quantization to 8 bits)
    //! Also for X server that is running in low resolution (8 bit or less).
    uint8_t palConv[256];
  };

  //! @brief Caret status.
  struct CaretStatus
  {
    Widget* widget;
    Rect rect;
    Argb color;
    uint32_t type;
    uint32_t animation;
  };

  //! @brief Contains information about keyboard status.
  struct KeyboardStatus
  {
    uint8_t keymap[256];
    uint32_t modifiers;
  };

  //! @brief Contains information about mouse status.
  struct MouseStatus
  {
    //! @brief Widget where mouse is.
    Widget* widget;
    //! @brief Mouse position relative to @c widget.
    Point position;
    //! @brief Hover state.
    uint32_t hover;
    //! @brief Pressed buttons.
    uint32_t buttons;
    //! @brief Whether this mouse status is valid.
    uint32_t valid;
  };

  struct SystemMouseStatus
  {
    //! @brief System window where mouse is (or NULL).
    GuiWindow* uiWindow;
    //! @brief Mouse position relative to @c uiWindow (in client area).
    Point position;
    //! @brief Hover state.
    uint32_t hover;
    //! @brief Pressed buttons.
    uint32_t buttons;
  };

  struct UpdateStatus
  {
    uint32_t scheduled;
    uint32_t updating;
    CancelableTask* task;
  };

  // [Construction / Destruction]

  GuiEngine();
  virtual ~GuiEngine();

  // [Lock]

  FOG_INLINE Lock& lock() { return _lock; }

  // [ID <-> UIWindow]

  virtual bool mapHandle(void* handle, GuiWindow* w) = 0;
  virtual bool unmapHandle(void* handle) = 0;
  virtual GuiWindow* handleToNative(void* handle) const = 0;

  // [Display]

  virtual err_t getDisplayInfo(DisplayInfo* out) const = 0;
  virtual err_t getPaletteInfo(PaletteInfo* out) const = 0;

  virtual void updateDisplayInfo() = 0;

  // [Caret]

  virtual err_t getCaretStatus(CaretStatus* out) const = 0;

  // [Keyboard]

  virtual err_t getKeyboardStatus(KeyboardStatus* out) const = 0;
  virtual uint32_t getKeyboardModifiers() const = 0;
  virtual uint32_t keyToModifier(uint32_t key) const;

  // [Mouse]

  virtual err_t getMouseStatus(MouseStatus* out) const = 0;
  virtual err_t getSystemMouseStatus(SystemMouseStatus* out) const = 0;

  virtual void invalidateMouseStatus() = 0;
  virtual void updateMouseStatus() = 0;
  virtual void changeMouseStatus(Widget* w, const Point& pos) = 0;

  virtual void clearSystemMouseStatus() = 0;

  virtual bool startButtonRepeat(uint32_t button, 
    bool reset, TimeDelta delay, TimeDelta interval) = 0;
  virtual bool stopButtonRepeat(uint32_t button) = 0;
  virtual void clearButtonRepeat() = 0;

  // [Wheel]

  virtual int getWheelLines() const = 0;
  virtual void setWheelLines(int count) = 0;

  // [Timing]

  virtual TimeDelta getRepeatingDelay() const = 0;
  virtual TimeDelta getRepeatingInterval() const = 0;
  virtual TimeDelta getDoubleClickInterval() const = 0;

  // [Windowing System]

  virtual void dispatchEnabled(Widget* w, bool enabled) = 0;
  virtual void dispatchVisibility(Widget* w, bool visible) = 0;
  virtual void dispatchConfigure(Widget* w, const Rect& rect, bool changedOrientation) = 0;

  //! @brief Called by widget destructor to erase all links to the widget from UIEngine.
  virtual void widgetDestroyed(Widget* w) = 0;

  // [Update]

  //! Tells application that some widget needs updating. This is key feature
  //! in the library that updating is in one place, so widgets can update()
  //! very often.
  virtual void update() = 0;

  //! @brief Runs updating. Do not use directly, use @c update() or you get into troubles.
  virtual void doUpdate() = 0;

  //! @brief Runs updating to specific window. This is internally done by 
  //! @c doUpdate() for all needed windows.
  virtual void doUpdateWindow(GuiWindow* window) = 0;

  //! @brief Blits window content into screen. Called usually from @c doUpdateWindow().
  virtual void doBlitWindow(GuiWindow* window, const Box* rects, sysuint_t count) = 0;

  // [UIWindow Create / Destroy]

  virtual GuiWindow* createUIWindow(Widget* widget) = 0;
  virtual void destroyUIWindow(GuiWindow* native) = 0;

  // [Members]

protected:
  //! @brief Lock.
  Lock _lock;

  //! @brief Library, if this UIEngine was opened as plugin.
  Library _library;

  //! @brief Whether UIEngine is correctly initialized.
  bool _initialized;

  friend struct Application;
};

// ============================================================================
// [Fog::UIWindow]
// ============================================================================

struct FOG_API GuiWindow : public Object
{
  FOG_DECLARE_OBJECT(GuiWindow, Object)

  // [Create Flags]

  //! @brief Native create flags.
  enum CreateFlags
  {
    //! @brief Create UIWindow (this flag is used in Fog::Widget).
    CreateUIWindow = (1 << 0),

    //! @brief Create popup like window instead of normal one
    //!
    //! Created popup window hasn't native borders and decoration, use
    //! border style to set these borders.
    CreatePopup = (1 << 16),

    //! @brief Create X11 window that listens only for PropertyChange events.
    X11OnlyPropertyChangeMask = (1 << 20),

    //! @brief Override redirection from window managers under X11.
    X11OverrideRedirect = (1 << 21)
  };

  // [Construction / Destruction]

  GuiWindow(Widget* widget);
  virtual ~GuiWindow();

  // [Window Manipulation]

  virtual err_t create(uint32_t flags) = 0;
  virtual err_t destroy() = 0;

  virtual err_t enable() = 0;
  virtual err_t disable() = 0;

  virtual err_t show() = 0;
  virtual err_t hide() = 0;
  virtual err_t move(const Point& pt) = 0;
  virtual err_t resize(const Size& size) = 0;
  virtual err_t reconfigure(const Rect& rect) = 0;

  virtual err_t takeFocus() = 0;

  virtual err_t setTitle(const String& title) = 0;
  virtual err_t getTitle(String& title) = 0;

  virtual err_t setIcon(const Image& icon) = 0;
  virtual err_t getIcon(Image& icon) = 0;

  virtual err_t setSizeGranularity(const Point& pt) = 0;
  virtual err_t getSizeGranularity(Point& pt) = 0;

  virtual err_t worldToClient(Point* coords) = 0;
  virtual err_t clientToWorld(Point* coords) = 0;

  // [Windowing System]

  virtual void onEnabled(bool enabled) = 0;
  virtual void onVisibility(bool visible) = 0;

  virtual void onConfigure(const Rect& windowRect, const Rect& clientRect) = 0;

  virtual void onMouseHover(int x, int y) = 0;
  virtual void onMouseMove(int x, int y) = 0;
  virtual void onMouseLeave(int x, int y) = 0;

  virtual void onMousePress(uint32_t button, bool repeated) = 0;
  virtual void onMouseRelease(uint32_t button) = 0;
  virtual void onMouseWheel(uint32_t wheel) = 0;

  virtual void onFocus(bool focus) = 0;

  virtual bool onKeyPress(uint32_t key, uint32_t modifier, uint32_t systemCode, Char unicode) = 0;
  virtual bool onKeyRelease(uint32_t key, uint32_t modifier, uint32_t systemCode, Char unicode) = 0;

  virtual void clearFocus() = 0;
  virtual void setFocus(Widget* w, uint32_t reason) = 0;

  // [Dirty]

  virtual void setDirty() = 0;

  // [Getters]

  FOG_INLINE Widget* getWidget() const { return _widget; }
  FOG_INLINE void* getHandle() const { return _handle; }
  FOG_INLINE GuiBackBuffer* getBackingStore() const { return _backingStore; }

  FOG_INLINE bool isDirty() const { return _isDirty; }
  FOG_INLINE bool hasFocus() const { return _hasFocus; }

  //! @brief Widget.
  Widget* _widget;

  //! @brief Window handle.
  void* _handle;

  //! @brief Backing store.
  GuiBackBuffer* _backingStore;

  //! @brief Whether window is enabled.
  bool _enabled;

  //! @brief Whether window is visible.
  bool _visible;

  //! @brief Whether window has native windowing system focus.
  bool _hasFocus;

  //! @brief Whether window needs blit to screen.
  bool _needBlit;

  //! @brief Window is dirty and needs update.
  bool _isDirty;

  friend struct GuiEngine;
  friend struct Widget;
};

// ============================================================================
// [Fog::UIBackingStore]
// ============================================================================

//! @brief Provides cacheable backing store for system windows (@c UIWindow).
struct FOG_API GuiBackBuffer
{
  GuiBackBuffer();
  virtual ~GuiBackBuffer();

  virtual bool resize(int width, int height, bool cache) = 0;
  virtual void destroy() = 0;
  virtual void updateRects(const Box* rects, sysuint_t count) = 0;

  void _clear();

  bool expired(TimeTicks now) const;

  FOG_INLINE uint32_t getType() const { return _type; }

  FOG_INLINE uint8_t* getPixels() const { return _pixels; }
  FOG_INLINE int getWidth() const { return _width; }
  FOG_INLINE int getHeight() const { return _height; }
  FOG_INLINE int getFormat() const { return _format; }
  FOG_INLINE int getDepth() const { return _depth; }
  FOG_INLINE sysint_t getStride() const { return _stride; }

  FOG_INLINE int getWidthOrig() const { return _widthOrig; }
  FOG_INLINE int getHeightOrig() const { return _heightOrig; }

  FOG_INLINE uint8_t* getPixelsPrimary() const { return _pixelsPrimary; }
  FOG_INLINE sysint_t getStridePrimary() const { return _stridePrimary; }

  FOG_INLINE uint8_t* getPixelsSecondary() const { return _pixelsSecondary; }
  FOG_INLINE sysint_t getStrideSecondary() const { return _strideSecondary; }

  FOG_INLINE void* getConvertFunc() const { return _convertFunc; }
  FOG_INLINE TimeTicks getCreatedTime() const { return _createdTime; }
  FOG_INLINE TimeTicks getExpireTime() const { return _expireTime; }

  uint32_t _type;

  // primary or secondary buffer, these parameters are used for painting, 
  // they aren't cached. So if window has 320x200 size it will be equal to 
  // _width and _height.
  uint8_t* _pixels;
  int _width;
  int _height;
  int _format;
  int _depth;
  sysint_t _stride;

  // original cached width and height, can be greater or equal than _width 
  // or _height
  int _widthOrig;
  int _heightOrig;

  // 24 or 32 bit double buffer parameters, parameters are cached.
  uint8_t* _pixelsPrimary;
  sysint_t _stridePrimary;

  // 4/8/16/24/32 bit conversion buffer (only created when needed).
  // Parameters are cached.
  uint8_t* _pixelsSecondary;
  sysint_t _strideSecondary;

  void* _convertFunc;
  int _convertDepth;

  // Time when the backing store wes created (for caching)
  TimeTicks _createdTime;
  TimeTicks _expireTime;
};

} // Fog namespace

//! @addtogroup Fog_Gui
//! @}

// ============================================================================
// [Fog::TypeInfo<>]
// ============================================================================

FOG_DECLARE_TYPEINFO(Fog::GuiEngine::DisplayInfo, Fog::TYPEINFO_PRIMITIVE)
FOG_DECLARE_TYPEINFO(Fog::GuiEngine::PaletteInfo, Fog::TYPEINFO_PRIMITIVE)
FOG_DECLARE_TYPEINFO(Fog::GuiEngine::CaretStatus, Fog::TYPEINFO_PRIMITIVE)
FOG_DECLARE_TYPEINFO(Fog::GuiEngine::MouseStatus, Fog::TYPEINFO_PRIMITIVE)
FOG_DECLARE_TYPEINFO(Fog::GuiEngine::SystemMouseStatus, Fog::TYPEINFO_PRIMITIVE)
FOG_DECLARE_TYPEINFO(Fog::GuiEngine::KeyboardStatus, Fog::TYPEINFO_PRIMITIVE)

// [Guard]
#endif // _FOG_GUI_GUIENGINE_H

// [Fog-Gui Library - Public API]
//
// [License]
// MIT, See COPYING file in package

// [Precompiled Headers]
#if defined(FOG_PRECOMP)
#include FOG_PRECOMP
#endif // FOG_PRECOMP

// [Dependencies]
#include <Fog/Gui/Layout/Layout.h>
#include <Fog/Gui/Layout/LayoutItem.h>
#include <Fog/Gui/Widget.h>
#include <Fog/Gui/GuiEngine.h>

FOG_IMPLEMENT_OBJECT(Fog::Layout)

namespace Fog {

// ============================================================================
// [Fog::Layout]
// ============================================================================

Layout::Layout() : _parentItem(0), _toplevel(0), _spacing(0), _enabled(1), _activated(1), _flexcount(0), _nextactivate(0),_invalidated(0)
{
  _flags |= OBJ_IS_LAYOUT;  
}

Layout::Layout(Widget *parent, Layout* parentLayout) : _flexcount(0), _toplevel(0), _spacing(0), _enabled(1), _activated(1), _parentItem(0), _nextactivate(0),_invalidated(0) {
  _flags |= OBJ_IS_LAYOUT;

  if (parentLayout) {
    _parentItem = parent;
     parentLayout->addChild(this);
  } else if (parent) {
     parent->setLayout(this);
     if(_parentItem == parent) {
       parent->_withinLayout = this;
       _toplevel = 1;

       invalidateLayout();
     }
  } else {
    _parentItem = 0;
  }
}

Layout::~Layout() 
{
  FOG_ASSERT(_nextactivate == NULL);
}

void Layout::markAsDirty() {
  if(_toplevel == 1) {
    Widget* w = getParentWidget();
    if(w) {
      GuiWindow * window = w->getClosestGuiWindow();
      if(window) {
        _activated = 0;
        _invalidated = 0;
        _nextactivate = window->_activatelist;
        window->_activatelist = this;
      }
    }
  } else {
    _toplevel = _toplevel;
  }
}

Widget* Layout::getParentWidget() const
{
  if (!_toplevel) {
    if (_parentItem) {
      FOG_ASSERT(_parentItem->isLayout());
      return static_cast<Layout*>(_parentItem)->getParentWidget();
    } else {
      return 0;
    }
  }

  FOG_ASSERT(_parentItem && _parentItem->isWidget());
  return (Widget *)(_parentItem);
}

int Layout::calcMargin(int margin, MarginPosition pos) const {
  if (margin >= 0) {
    return margin;
  } else if (!_toplevel) {
    return 0;
  } else if (const Widget *pw = getParentWidget()) {
    if(pw->isGuiWindow()) {
      return DEFAULT_WINDOW_MARGIN;
    } else {
      return DEFAULT_WIDGET_MARGIN;
    }
  }

  return 0;
}

int Layout::getSpacing() const { 
  if (_spacing >=0) {
    return _spacing;
  } else {
    if (!_parentItem) {
      return -1;
    } else if (_parentItem->isWidget()) {
      return DEFAULT_LAYOUT_SPACING;
    } else {
      FOG_ASSERT(_parentItem->isLayout());
      return static_cast<Layout *>(_parentItem)->getSpacing();
    }

    return 0;
  }  
}

bool Layout::setLayoutAlignment(LayoutItem *item, uint32_t alignment)
{
  if(item->_withinLayout == this) {
    item->setLayoutAlignment(alignment);
    return true;
  }

  return false;
}

#define FOR_EACH(ITEM, CONTAINER) \
LayoutItem *ITEM;\
int i=0;\
while ((ITEM = CONTAINER->getAt(i++)))

bool Layout::isEmpty() const {
  FOR_EACH(iitem, this) {
    if (!iitem->isEmpty())
      return false;
  }

  return true;
}

int Layout::indexOf(LayoutItem* item) const
{
  if(item->_withinLayout != this)
    return -1;

  FOR_EACH(iitem, this) {
    if (iitem == item)
      return i;
  }

  return -1;
}

void Layout::remove(LayoutItem* item) {
  if(item->_withinLayout != this)
    return;

  FOR_EACH(iitem, this) {
    if (iitem == item) {      
      if(item->hasFlex())
        removeFlexItem();

      item->_withinLayout = 0;
      item->removeLayoutStruct();
      takeAt(i);
      invalidateLayout();
    }
  }
}

bool Layout::removeAllWidgets(LayoutItem *layout, Widget *w)
{  
  if(!layout->_withinLayout)
    return false;

  if (!layout->isLayout())
    return false;

  Layout *lay = static_cast<Layout*>(layout);

  FOR_EACH(iitem, lay) {
    if (iitem == w) {
      lay->takeAt(i);
      if(iitem->hasFlex())
        lay->removeFlexItem();
      w->_withinLayout = 0;
      lay->invalidateLayout();
      return true;
    } else if (removeAllWidgets(iitem, w)) {
      return true;
    } 
  }

  return false;
}

//reparent 
void Layout::reparentChildWidgets(Widget* widget)
{
  FOR_EACH(iitem, this) {
    if(iitem->isWidget()) {
      Widget* w = static_cast<Widget*>(iitem);
      if (w->getParent() != widget)
        w->setParent(widget);
    } else if (iitem->isLayout()) {
      static_cast<Layout*>(iitem)->reparentChildWidgets(widget);
    }
  }
}

void Layout::addChild(LayoutItem* item) 
{  
  if(item->isWidget()) {
    Widget* w = static_cast<Widget*>(item);
    Widget *layoutparent = getParentWidget();
    Widget *widgetparent = w->getParent();

    if(widgetparent && widgetparent->getLayout() && item->_withinLayout) {
      if(removeAllWidgets(widgetparent->getLayout(), w)) {
        //WARNING: removed from existing layout
      }
    }

    if (widgetparent && layoutparent && widgetparent != layoutparent) {
      w->setParent(layoutparent);
    } else if(!widgetparent && layoutparent) {
      w->setParent(layoutparent);
    }
  } else if(item->isLayout()) {
    //support for Flex-Layout?
    return addChildLayout(static_cast<Layout*>(item));
  }

  item->_withinLayout = this;
  if(item->hasFlex()) {
    addFlexItem();
  }

  prepareItem(item, getLength());
  _children.append(item);
}

void Layout::addChildLayout(Layout *l)
{
  if (l->_parentItem) {
    //WARNING already has a parent!
    return;
  }

  if (Widget *mw = getParentWidget()) {
    l->reparentChildWidgets(mw);
  }

  l->_parentItem = this;  
  l->_toplevel = 0;

  prepareItem(l, getLength());
  _children.append(l);
}

IntSize Layout::getTotalMinimumSize() const
{
  int side=0, top=0;
  calcContentMargins(side,top);

  IntSize s = getLayoutMinimumSize();
  return s + IntSize(side, top);
}

IntSize Layout::getTotalMaximumSize() const { 
  int side=0, top=0;
  calcContentMargins(side,top);

  IntSize s = getLayoutMaximumSize();
  if (_toplevel) {
    s = IntSize(Math::min<int>(s.getWidth() + side, WIDGET_MAX_SIZE), Math::min<int>(s.getHeight() + top, WIDGET_MAX_SIZE));
  }

  return s;
}

int Layout::getTotalHeightForWidth(int w) const
{
  int side=0, top=0;
  calcContentMargins(side,top);
  int h = getLayoutHeightForWidth(w - side) + top;
  return h;
}

void Layout::calcContentMargins(int&side, int&top) const {
  if (_toplevel) {
    Widget* parent = getParentWidget();
    FOG_ASSERT(parent);
    side = parent->getContentLeftMargin() + parent->getContentRightMargin();
    top = parent->getContentTopMargin() + parent->getContentBottomMargin();
  }
}

IntSize Layout::getTotalSizeHint() const
{
  int side=0, top=0;
  calcContentMargins(side,top);

  IntSize s = getLayoutSizeHint();
  if (hasLayoutHeightForWidth())
    s.setHeight(getLayoutHeightForWidth(s.getWidth() + side));

  return s + IntSize(side, top);
}

void Layout::callSetGeometry(const IntSize& size) {
  if(size.isValid()) {
    //support for parentWidget-Margin (so layouts do not need to do this within calculation)
    IntRect rect = getParentWidget()->getClientContentGeometry();
    //TODO: EntireRect
    setLayoutGeometry(rect);
  }
}

void Layout::update() {
  Layout *layout = this;
  while (layout && layout->_activated) {
    layout->_activated = false;
    if (layout->_toplevel) {
      FOG_ASSERT(layout->_parentItem->isWidget());
      layout->markAsDirty();
      break;
    }
    FOG_ASSERT(!layout->_parentItem || (layout->_parentItem && layout->_parentItem->isLayout()));
    layout = static_cast<Layout*>(layout->_parentItem);
  }
}

//Method invalidates all children
//Also it will mark all Layouts in the hierarchy to the value of activate
void Layout::invalidActivateAll(bool activate)
{
//   item->invalidateLayout();
//   if(item->isLayout()) {
//     FOR_EACH(child, this)
//     {
//       invalidActivateAll(child, activate);
//    }    
// 
//     ((Layout*)item)->_activated = activate;
//   }

  FOG_ASSERT(isLayout());

  invalidateLayout();

  FOR_EACH(child, this)
  {
    if(child->isLayout()) {
      Layout* layout = (Layout*)child;
      layout->invalidActivateAll(activate);
    } else {
      child->invalidateLayout();
    }
  } 

  _activated = activate;
}

bool Layout::activate() {
  if (!isEnabled() || !_parentItem)
    return false;  

  if (!_toplevel) {
    FOG_ASSERT(!_parentItem || (_parentItem && _parentItem->isLayout()));
    return static_cast<Layout*>(_parentItem)->activate();
  }
  if (_activated)
    return false;

  Widget *mw = static_cast<Widget*>(_parentItem);
  if (mw == 0) {
    return false;
  }
  FOG_ASSERT(!_parentItem || (_parentItem && _parentItem->isWidget()));

  //invalid all childs and mark them directly as activated
  //mark all childs as activated (recursive!)
  invalidActivateAll(true);

  Widget *md = mw;
  bool hasH = md->hasMinimumHeight();
  bool hasW = md->hasMinimumWidth();
  bool calc = (!hasH || !hasW);
  
  if (mw->isGuiWindow()) {
    IntSize ms = getTotalMinimumSize();
    if(calc) {
      if (hasW) {
        ms.setWidth(mw->getMinimumSize().getWidth());      
      }
      if (hasH) {
        ms.setHeight(mw->getMinimumSize().getHeight());
      }
    }

    if (calc && hasLayoutHeightForWidth()) {
      int h = getLayoutMinimumHeightForWidth(ms.getWidth());
      if (h > ms.getHeight()) {
        if (!hasH)
          ms.setHeight(0);
        if (!hasW)
          ms.setWidth(0);
      }
    }
    mw->setMinimumSize(ms);
  } else if (calc) {
    IntSize ms = mw->getMinimumSize();
    if (!hasH) {
      ms.setHeight(0);
    }
    if (!hasW) {
      ms.setWidth(0);
    }
    mw->setMinimumSize(ms);
  }

  callSetGeometry(mw->getSize());
  return false;
}


void Layout::onLayout(LayoutEvent* e) {
  if (!isEnabled())
    return;

  if(e->_code == EVENT_LAYOUT_REQUEST) {
    FOG_ASSERT(!_parentItem || (_parentItem && _parentItem->isWidget()));
    if (static_cast<Widget *>(_parentItem)->isVisible())
    {
      activate();
    }
  }
}



} // Fog namespace
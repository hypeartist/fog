// [Fog/Core Library - C++ API]
//
// [Licence] 
// MIT, See COPYING file in package

// [Guard]
#ifndef _FOG_CORE_STATIC_H
#define _FOG_CORE_STATIC_H

// [Dependencies]
#include <Fog/Build/Build.h>

#include <new>

//! @addtogroup Fog_Core
//! @{

namespace Fog {

//! @brief Simple template that can create class using placement new and delete.
//!
//! This template should be used in situations where compiler can't guarante
//! order of static instantiation and you want full control about it.
//!
//! Example:
//! @verbatim
//! Fog::Static<Fog::Mutex> mutex;
//!
//! // initialize class, call new Mutex
//! mutex.init();
//!
//! // do something, use -> operator or instance() method
//! mutex->lock();
//! mutex->unlock();
//!
//! mutex.instance().lock();
//! mutex.instance().unlock();
//!
//! // destroy class, call delete Mutex
//! mutex.destroy();
//! @endverbatim
//!
//! This template is very effective, because the memory is not allocated on the
//! heap, instead stack based allocation is used together with placement @c new
//! and @c delete operators.
template<typename Type>
struct Static
{
public:
  //! @brief Initializer (calls placement @c new operator).
  FOG_INLINE void init() { new ((void*)_storage) Type; }
  //! @brief Initializer with copy assignment (calls placement @c new operator).
  FOG_INLINE void init(const Static<Type>& t) { new ((void*)_storage) Type(t.instance()); }
  //! @brief Initializer with copy assignment (calls placement @c new operator).
  FOG_INLINE void init(const Type& t) { new ((void*)_storage) Type(t); }
  //! @brief Deinitializer (calls placement @c delete operator).
  FOG_INLINE void destroy() { ((Type*)(_storage))->~Type(); }

  //! @brief Returns instance of @c Type.
  FOG_INLINE Type& instance() { return *((Type*)(_storage)); }
  //! @brief Returns const instance of @c Type.
  FOG_INLINE const Type& instance() const { return *((Type*)(_storage)); }

  //! @brief Returns pointer to instance of @c Type.
  FOG_INLINE Type* instancep() { return ((Type*)(_storage)); }
  //! @brief Returns const pointer to instance of @c Type.
  FOG_INLINE const Type* instancep() const { return ((Type*)(_storage)); }

  //! @brief Overriden Type* operator.
  FOG_INLINE operator Type*() { return instancep(); }
  //! @brief Overriden const Type* operator.
  FOG_INLINE operator const Type*() const { return instancep(); }

  //! @brief Overriden -> operator.
  FOG_INLINE Type* operator->() { return ((Type*)(_storage)); }
  //! @brief Overriden const -> operator.
  FOG_INLINE Type const* operator->() const { return ((Type const*)(_storage)); }

private:
  //! @brief Stack based storage.
  uint8_t _storage[sizeof(Type)];
};

} // Fog namespace

//! @}

// [Guard]
#endif // _FOG_CORE_STATIC_H
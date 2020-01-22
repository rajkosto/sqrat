// Sqrat: altered version by Gaijin Entertainment Corp.
// SqratTypes: Type Translators
//

//
// Copyright (c) 2009 Brandon Jones
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
//  1. The origin of this software must not be misrepresented; you must not
//  claim that you wrote the original software. If you use this software
//  in a product, an acknowledgment in the product documentation would be
//  appreciated but is not required.
//
//  2. Altered source versions must be plainly marked as such, and must not be
//  misrepresented as being the original software.
//
//  3. This notice may not be removed or altered from any source
//  distribution.
//

#pragma once
#if !defined(_SQRAT_TYPES_H_)
#define _SQRAT_TYPES_H_

#include "../../squirrel.h"
#include <string>

#include "sqratClassType.h"
#include "sqratUtil.h"

namespace Sqrat {


template <typename T>
struct popAsInt
{
    static bool pop(HSQUIRRELVM vm, SQInteger idx, T &value)
    {
        static_assert(std::is_convertible<T, SQInteger>::value, "type is not convertible to int");

        SQObjectType value_type = sq_gettype(vm, idx);
        switch(value_type) {
          case OT_BOOL: {
              SQBool sqValueb = SQFalse;
              SQRESULT res = sq_getbool(vm, idx, &sqValueb);
              value = static_cast<T>(sqValueb);
              return SQ_SUCCEEDED(res);
          }
          case OT_INTEGER: {
              SQInteger sqValue = 0;
              SQRESULT res = sq_getinteger(vm, idx, &sqValue);
              value = static_cast<T>(sqValue);
              return SQ_SUCCEEDED(res);
          }
          case OT_FLOAT: {
              SQFloat sqValuef = 0;
              SQRESULT res = sq_getfloat(vm, idx, &sqValuef);
              value = static_cast<T>(static_cast<int>(sqValuef));
              return SQ_SUCCEEDED(res);
          }
          default:
              SQRAT_ASSERTF(0, FormatTypeError(vm, idx, _SC("integer")).c_str());
              value = static_cast<T>(0);
              break;
        }
        return false;
    }
};


template <typename T>
struct popAsFloat
{
    static bool pop(HSQUIRRELVM vm, SQInteger idx, T& value)
    {
        static_assert(std::is_convertible<T, float>::value, "type is not convertible to float");

        SQObjectType value_type = sq_gettype(vm, idx);
        switch(value_type) {
          case OT_BOOL: {
              SQBool sqValueb = SQFalse;
              SQRESULT res = sq_getbool(vm, idx, &sqValueb);
              value = static_cast<T>(sqValueb);
              return SQ_SUCCEEDED(res);
          }
          case OT_INTEGER: {
              SQInteger sqValue = 0;
              SQRESULT res = sq_getinteger(vm, idx, &sqValue);
              value = static_cast<T>(sqValue);
              return SQ_SUCCEEDED(res);
          }
          case OT_FLOAT: {
              SQFloat sqValuef = 0;
              SQRESULT res = sq_getfloat(vm, idx, &sqValuef);
              value = static_cast<T>(sqValuef);
              return SQ_SUCCEEDED(res);
          }
          default:
              SQRAT_ASSERTF(0, FormatTypeError(vm, idx, _SC("float")).c_str());
              value = 0;
              break;
        }
        return false;
    }
};


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Used to get and push class instances to and from the stack as copies
///
/// \remarks
/// This specialization requires T to have a default constructor.
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T, class>
struct Var {

    using ClassT = ClassType<remove_const_t<T>>;
    T value; ///< The actual value of get operations

    /// Attempts to get the value off the stack at idx as the given type
    Var(HSQUIRRELVM vm, SQInteger idx) {
        T* ptr = ClassT::GetInstance(vm, idx);
        if (ptr != NULL) {
            value = *ptr;
        }
    }

    /// Called by Sqrat::PushVar to put a class object on the stack
    static void push(HSQUIRRELVM vm, const T& value) {
        if (ClassT::hasClassData(vm))
            ClassT::PushInstanceCopy(vm, value);
        else
            SQRAT_ASSERTF(0, "Class/typename was not bound");
    }

    static const SQChar * getVarTypeName() { return _SC("native instance"); }

    static bool check_type(HSQUIRRELVM vm, SQInteger idx) {
        return ClassT::IsClassInstance(vm, idx);
    }
};

// Var<T> trait to find-out whether value can be used after Var has been
// destroyed. If Var has user-provided destructor it likely controls value
// lifetime
template<typename T>
struct VarControlsValueLifeTime
{
  enum {value = 0};
};

template<class Callable> SQFUNCTION SqGlobalThunk();

template<typename Func>
struct Var<Func, std::enable_if_t<std::is_function_v<Func> || std::is_member_function_pointer<Func>::value>>
{
  static void push(HSQUIRRELVM vm, const Func& value)
  {
    SQFUNCTION funcThunk = SqGlobalThunk<Func>();
    SQUserPointer funcPtr = sq_newuserdata(vm, sizeof(Func));
    new (funcPtr) Func(value);
    sq_setreleasehook(vm, -1, ImplaceFreeReleaseHook<Func>);
    sq_newclosure(vm, funcThunk, 1);
  }
};


/// Used to get and push class instances to and from the stack as references
template<class T>
struct Var<T&> {

    using ClassT = ClassType<remove_const_t<T>>;
    T& value; ///< The actual value of get operations

    /// Attempts to get the value off the stack at idx as the given type
    Var(HSQUIRRELVM vm, SQInteger idx) : value(*ClassT::GetInstance(vm, idx)) {
    }

    /// Called by Sqrat::PushVarR to put a class object on the stack
    static void push(HSQUIRRELVM vm, T& value) {
        if (ClassT::hasClassData(vm))
        {
          if (std::is_const<T>::value)
            ClassT::PushInstanceCopy(vm, value);
          else
            ClassT::PushInstance(vm, const_cast<remove_const_t<T>*>(&value));
        }
        else
            SQRAT_ASSERTF(0, "Class/typename was not bound");
    }

    static const SQChar * getVarTypeName() { return _SC("native instance ref"); }

    static bool check_type(HSQUIRRELVM vm, SQInteger idx) {
        return ClassT::IsClassInstance(vm, idx);
    }
};

/// Used to get and push class instances to and from the stack as pointers
template<class T>
struct Var<T*, std::enable_if_t<!is_callable_v<T*>>> {

    using ClassT = ClassType<remove_const_t<T>>;
    T* value; ///< The actual value of get operations

    /// Attempts to get the value off the stack at idx as the given type
    Var(HSQUIRRELVM vm, SQInteger idx) : value(ClassT::GetInstance(vm, idx, true)) {
    }

    /// Called by Sqrat::PushVar to put a class object on the stack
    static void push(HSQUIRRELVM vm, T* value) {
        if (ClassT::hasClassData(vm))
          ClassT::PushInstance(vm, const_cast<remove_const_t<T>*>(value));
        else
          sq_pushinteger(vm, reinterpret_cast<SQInteger>(value));
    }

    static const SQChar * getVarTypeName() { return _SC("native instance ptr"); }

    static bool check_type(HSQUIRRELVM vm, SQInteger idx) {
        return ClassT::IsClassInstance(vm, idx);
    }
};


/// Used to get (as copies) and push (as references) class instances to and from the stack as a std::shared_ptr
template<class T> void PushVarR(HSQUIRRELVM vm, T& value);
template<class T>
struct Var<std::shared_ptr<T> > {

    std::shared_ptr<T> value; ///< The actual value of get operations

    /// Attempts to get the value off the stack at idx as the given type
    Var(HSQUIRRELVM vm, SQInteger idx) {
        if (sq_gettype(vm, idx) != OT_NULL) {
            Var<T> instance(vm, idx);
            value.reset(new T(instance.value));
        }
    }

    /// Called by Sqrat::PushVar to put a class object on the stack
    static void push(HSQUIRRELVM vm, const std::shared_ptr<T>& value) {
        PushVarR(vm, *value);
    }
};

// Integer types
#define SQRAT_INTEGER( type ) \
 template<> \
 struct Var<type> { \
     type value; \
     Var(HSQUIRRELVM vm, SQInteger idx) { \
         popAsInt<type>::pop(vm, idx, value); \
     } \
     static void push(HSQUIRRELVM vm, type value) { \
         sq_pushinteger(vm, static_cast<SQInteger>(value)); \
     } \
    static const SQChar * getVarTypeName() { return _SC("integer"); } \
    static bool check_type(HSQUIRRELVM vm, SQInteger idx) { return sq_gettype(vm, idx) & SQOBJECT_NUMERIC; } \
 };\
 \
 template<> \
 struct Var<type&> { \
     type value; \
     Var(HSQUIRRELVM vm, SQInteger idx) { \
         popAsInt<type>::pop(vm, idx, value); \
     } \
     static void push(HSQUIRRELVM vm, type& value) { \
         sq_pushinteger(vm, static_cast<SQInteger>(value)); \
     } \
    static const SQChar * getVarTypeName() { return _SC("integer ref"); } \
    static bool check_type(HSQUIRRELVM vm, SQInteger idx) { return sq_gettype(vm, idx) & SQOBJECT_NUMERIC; } \
 }; \
 \
 template<> \
 struct Var<const type&> { \
     type value; \
     Var(HSQUIRRELVM vm, SQInteger idx) { \
         popAsInt<type>::pop(vm, idx, value); \
     } \
     static void push(HSQUIRRELVM vm, type value) { \
         sq_pushinteger(vm, static_cast<SQInteger>(value)); \
     } \
    static const SQChar * getVarTypeName() { return _SC("integer const ref"); } \
    static bool check_type(HSQUIRRELVM vm, SQInteger idx) { return sq_gettype(vm, idx) & SQOBJECT_NUMERIC; } \
 };

SQRAT_INTEGER(unsigned int)
SQRAT_INTEGER(signed int)
SQRAT_INTEGER(unsigned long)
SQRAT_INTEGER(signed long)
SQRAT_INTEGER(unsigned short)
SQRAT_INTEGER(signed short)
SQRAT_INTEGER(unsigned char)
SQRAT_INTEGER(signed char)
SQRAT_INTEGER(unsigned long long)
SQRAT_INTEGER(signed long long)

#ifdef _MSC_VER
#if defined(__int64)
SQRAT_INTEGER(unsigned __int64)
SQRAT_INTEGER(signed __int64)
#endif
#endif

// Float types
#define SQRAT_FLOAT( type ) \
 template<> \
 struct Var<type> { \
     type value; \
     Var(HSQUIRRELVM vm, SQInteger idx) { \
         popAsFloat<type>::pop(vm, idx, value); \
     } \
     static void push(HSQUIRRELVM vm, const type& value) { \
         sq_pushfloat(vm, static_cast<SQFloat>(value)); \
     } \
    static const SQChar * getVarTypeName() { return _SC("float"); } \
    static bool check_type(HSQUIRRELVM vm, SQInteger idx) { return sq_gettype(vm, idx) & SQOBJECT_NUMERIC; } \
 }; \
 \
 template<> \
 struct Var<const type&> { \
     type value; \
     Var(HSQUIRRELVM vm, SQInteger idx) { \
         popAsFloat<type>::pop(vm, idx, value); \
     } \
     static void push(HSQUIRRELVM vm, const type& value) { \
         sq_pushfloat(vm, static_cast<SQFloat>(value)); \
     } \
    static const SQChar * getVarTypeName() { return _SC("float const ref"); } \
    static bool check_type(HSQUIRRELVM vm, SQInteger idx) { return sq_gettype(vm, idx) & SQOBJECT_NUMERIC; } \
 };

SQRAT_FLOAT(float)
SQRAT_FLOAT(double)


///////////////////////////////////////
/// Enums
///////////////////////////////////////

template<typename T>
struct Var<T, typename std::enable_if<std::is_enum<T>::value, void>::type> {
    T value;
    Var(HSQUIRRELVM vm, SQInteger idx) {
        SQInteger intVal = 0;
        if (SQ_SUCCEEDED(sq_getinteger(vm, idx, &intVal)))
            value = static_cast<T>(intVal);
     }

     static void push(HSQUIRRELVM vm, T value) {
         sq_pushinteger(vm, static_cast<SQInteger>(value));
     }
    static const SQChar * getVarTypeName() { return _SC("enum"); }
    static bool check_type(HSQUIRRELVM vm, SQInteger idx) {
        return sq_gettype(vm, idx) == OT_INTEGER;
    }
};


/// Used to get and push bools to and from the stack
template<>
struct Var<bool> {
    bool value; ///< The actual value of get operations

    /// Attempts to get the value off the stack at idx as a bool
    Var(HSQUIRRELVM vm, SQInteger idx) {
        SQBool sqValue;
        sq_tobool(vm, idx, &sqValue);
        value = (sqValue != 0);
    }

    /// Called by Sqrat::PushVar to put a bool on the stack
    static void push(HSQUIRRELVM vm, const bool& value) {
        sq_pushbool(vm, static_cast<SQBool>(value));
    }

    static const SQChar * getVarTypeName() { return _SC("bool"); }
    static bool check_type(HSQUIRRELVM /*vm*/, SQInteger /*idx*/) { return true; }
};

template<>
struct Var<const bool&> {
    bool value; ///< The actual value of get operations

    /// Attempts to get the value off the stack at idx as a bool
    Var(HSQUIRRELVM vm, SQInteger idx) {
        SQBool sqValue;
        sq_tobool(vm, idx, &sqValue);
        value = (sqValue != 0);
    }

    /// Called by Sqrat::PushVar to put a bool on the stack
    static void push(HSQUIRRELVM vm, const bool& value) {
        sq_pushbool(vm, static_cast<SQBool>(value));
    }

    static const SQChar * getVarTypeName() { return _SC("bool const ref"); }
    static bool check_type(HSQUIRRELVM /*vm*/, SQInteger /*idx*/) { return true; }
};

/// Used to get and push strings as SQChar arrays to and from the stack
template<>
struct Var<SQChar*> {
private:

    HSQOBJECT obj; /* hold a reference to the object holding value during the Var struct lifetime*/
    HSQUIRRELVM v;

public:

    SQChar* value; ///< The actual value of get operations
    SQInteger valueLen;

    /// Attempts to get the value off the stack at idx as a character array
    Var(HSQUIRRELVM vm, SQInteger idx) {
        sq_tostring(vm, idx);
        SQRAT_VERIFY(SQ_SUCCEEDED(sq_getstackobj(vm, -1, &obj)));
        sq_getstringandsize(vm, -1, (const SQChar**)&value, &valueLen);
        sq_addref(vm, &obj);
        sq_pop(vm,1);
        v = vm;
    }

    Var(Var<SQChar *> const &rhs)
      : obj(rhs.obj)
      , v(rhs.v)
      , value(rhs.value)
      , valueLen(rhs.valueLen)
    {
      sq_addref(v, &obj);
    }

    Var<SQChar *> &operator=(Var<SQChar *> const &rhs)
    {
      obj = rhs.obj;
      v = rhs.v;
      value = rhs.value;
      valueLen = rhs.valueLen;
      sq_addref(v, &obj);
      return *this;
    }

    ~Var()
    {
        if(v && !sq_isnull(obj)) {
            sq_release(v, &obj);
        }
    }

    /// Called by Sqrat::PushVar to put a character array on the stack
    static void push(HSQUIRRELVM vm, const SQChar* value, SQInteger len = -1) {
        sq_pushstring(vm, value, len);
    }

    static const SQChar * getVarTypeName() { return _SC("string"); }
    static bool check_type(HSQUIRRELVM vm, SQInteger idx) { return sq_gettype(vm, idx) == OT_STRING; }
};

template<>
struct VarControlsValueLifeTime<SQChar*>
{
  enum {value = 1};
};

/// Used to get and push strings as SQChar arrays to and from the stack
template<>
struct Var<const SQChar*> {
private:
    HSQOBJECT obj; /* hold a reference to the object holding value during the Var struct lifetime*/
    HSQUIRRELVM v;

public:
    const SQChar* value; ///< The actual value of get operations
    SQInteger valueLen;

    /// Attempts to get the value off the stack at idx as a character array
    Var(HSQUIRRELVM vm, SQInteger idx) {
        sq_tostring(vm, idx);
        SQRAT_VERIFY(SQ_SUCCEEDED(sq_getstackobj(vm, -1, &obj)));
        sq_getstringandsize(vm, -1, &value, &valueLen);
        sq_addref(vm, &obj);
        sq_pop(vm,1);
        v = vm;
    }

    Var(Var<const SQChar *> const &rhs)
      : obj(rhs.obj)
      , v(rhs.v)
      , value(rhs.value)
      , valueLen(rhs.valueLen)
    {
      sq_addref(v, &obj);
    }

    Var<const SQChar *> &operator=(Var<const SQChar *> const &rhs)
    {
      obj = rhs.obj;
      v = rhs.v;
      value = rhs.value;
      valueLen = rhs.valueLen;
      sq_addref(v, &obj);
      return *this;
    }

    ~Var()
    {
        if(v && !sq_isnull(obj)) {
            sq_release(v, &obj);
        }
    }

    /// Called by Sqrat::PushVar to put a character array on the stack
    static void push(HSQUIRRELVM vm, const SQChar* value, SQInteger len = -1) {
        sq_pushstring(vm, value, len);
    }

    static const SQChar * getVarTypeName() { return _SC("string"); }
    static bool check_type(HSQUIRRELVM vm, SQInteger idx) { return sq_gettype(vm, idx) == OT_STRING; }
};

template<>
struct VarControlsValueLifeTime<const SQChar*>
{
  enum {value = 1};
};

/// Used to get and push strings to and from the stack
template<>
struct Var<string> {

    string value; ///< The actual value of get operations

    /// Attempts to get the value off the stack at idx as a string
    Var(HSQUIRRELVM vm, SQInteger idx) {
        const SQChar* ret = _SC("n/a");
        sq_tostring(vm, idx);
        sq_getstring(vm, -1, &ret);
        value = string(ret, sq_getsize(vm, -1));
        sq_pop(vm,1);
    }

    /// Called by Sqrat::PushVar to put a string on the stack
    static void push(HSQUIRRELVM vm, const string& value) {
        sq_pushstring(vm, value.c_str(), value.size());
    }

    static const SQChar * getVarTypeName() { return _SC("string"); }
    static bool check_type(HSQUIRRELVM vm, SQInteger idx) { return sq_gettype(vm, idx) == OT_STRING; }
};

/// Used to get and push const string references to and from the stack as copies (strings are always copied)
template<>
struct Var<const string&> {

    string value; ///< The actual value of get operations

    /// Attempts to get the value off the stack at idx as a string
    Var(HSQUIRRELVM vm, SQInteger idx) {
        const SQChar* ret = nullptr;
        sq_tostring(vm, idx);
        sq_getstring(vm, -1, &ret);
        value = string(ret, sq_getsize(vm, -1));
        sq_pop(vm,1);
    }

    /// Called by Sqrat::PushVar to put a string on the stack
    static void push(HSQUIRRELVM vm, const string& value) {
        sq_pushstring(vm, value.c_str(), value.size());
    }

    static const SQChar * getVarTypeName() { return _SC("string"); }
    static bool check_type(HSQUIRRELVM vm, SQInteger idx) { return sq_gettype(vm, idx) == OT_STRING; }
};



// Non-referencable type definitions
template <class T, class = void> struct is_referencable : public std::true_type {};
template <class T>
struct is_referencable<T, std::enable_if_t<std::is_scalar<T>::value>> : public std::false_type {};

#define SQRAT_MAKE_NONREFERENCABLE( type ) \
 template<> struct is_referencable<type> : public std::false_type {};

SQRAT_MAKE_NONREFERENCABLE(string)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Pushes a value on to a given VM's stack
///
/// \remarks
/// What this function does is defined by Sqrat::Var template specializations,
/// and thus you can create custom functionality for it by making new template specializations.
/// When making a custom type that is not referencable, you must use SQRAT_MAKE_NONREFERENCABLE( type )
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
inline void PushVar(HSQUIRRELVM vm, T* value) {
    Var<T*>::push(vm, value);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Pushes a value on to a given VM's stack
///
/// \remarks
/// What this function does is defined by Sqrat::Var template specializations,
/// and thus you can create custom functionality for it by making new template specializations.
/// When making a custom type that is not referencable, you must use SQRAT_MAKE_NONREFERENCABLE( type )
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
inline void PushVar(HSQUIRRELVM vm, const T& value) {
    Var<T>::push(vm, value);
}
template<class T>
inline void PushVar(HSQUIRRELVM vm, T& value) {
  Var<T*>::push(vm, &value);
}


template<class T, bool b>
struct PushVarR_helper {
    inline static void push(HSQUIRRELVM vm, T value) {
        PushVar<T>(vm, value);
    }
};
template<class T>
struct PushVarR_helper<T, false> {
    inline static void push(HSQUIRRELVM vm, const T& value) {
        PushVar<const T&>(vm, value);
    }
};


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Pushes a reference on to a given VM's stack (some types cannot be referenced and will be copied instead)
///
/// \remarks
/// What this function does is defined by Sqrat::Var template specializations,
/// and thus you can create custom functionality for it by making new template specializations.
/// When making a custom type that is not referencable, you must use SQRAT_MAKE_NONREFERENCABLE( type )
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class T>
inline void PushVarR(HSQUIRRELVM vm, T& value) {
    if (!std::is_pointer<T>::value && is_referencable<typename std::remove_cv<T>::type>::value) {
        Var<T&>::push(vm, value);
    } else {
        PushVarR_helper<T, std::is_scalar<T>::value>::push(vm, value);
    }
}

namespace vargs
{
  template <typename T>
  std::tuple<Var<T>> make_vars(HSQUIRRELVM vm, int idx)
  {
    return std::make_tuple(Var<T>(vm, idx));
  }

  template <typename Head, typename... Tail>
  std::tuple<Var<Head>, Var<Tail>...>
    make_vars(HSQUIRRELVM vm, int idx, std::enable_if_t<(sizeof...(Tail) > 0), bool> = false)
  {
    return std::tuple_cat(std::make_tuple(Var<Head>(vm, idx)),
                            make_vars<Tail...>(vm, idx + 1));
  }

  template <typename... T>
  std::tuple<T...> make_vars(HSQUIRRELVM /*vm*/,
                               int /*idx*/,
                               std::enable_if_t<(sizeof...(T) == 0), bool> = false)
  {
    return std::make_tuple<T...>();
  }

  template <typename T>
  bool check_var_types(HSQUIRRELVM vm, int idx)
  {
    if (!Var<T>::check_type(vm, idx)) {
      const SQChar *argTypeName = _SC("unknown");
      SQInteger prevTop = sq_gettop(vm);
      if (SQ_SUCCEEDED(sq_typeof(vm, idx))) {
        sq_tostring(vm, -1);
        sq_getstring(vm, -1, &argTypeName);
      }

      std::string errMsg = _SC("Wrong argument type, expected '");
      errMsg += Var<T>::getVarTypeName();
      errMsg += "', got '"; 
      errMsg += argTypeName;
      errMsg += "'";
      sq_settop(vm, prevTop);
      sq_throwerror(vm, errMsg.c_str());
      return false;
    }
    return true;
  }

  template <typename Head, typename... Tail>
  bool check_var_types(HSQUIRRELVM vm, int idx, std::enable_if_t<(sizeof...(Tail) > 0), bool> = false)
  {
    if (!check_var_types<Head>(vm, idx))
      return false;
    return check_var_types<Tail...>(vm, idx+1);
  }

  template <typename... T>
  bool check_var_types(HSQUIRRELVM /*vm*/,
                       int /*idx*/,
                       std::enable_if_t<(sizeof...(T) == 0), bool> = false)
  {
    return true;
  }
}
}

#endif

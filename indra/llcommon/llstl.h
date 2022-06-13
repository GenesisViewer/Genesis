/** 
 * @file llstl.h
 * @brief helper object & functions for use with the stl.
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#ifndef LL_LLSTL_H
#define LL_LLSTL_H

#include <functional>
#include <algorithm>
#include <utility>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <typeinfo>
#include "stdtypes.h"

// Use to compare the first element only of a pair
// e.g. typedef std::set<std::pair<int, Data*>, compare_pair<int, Data*> > some_pair_set_t; 
template <typename T1, typename T2>
struct compare_pair_first
{
	bool operator()(const std::pair<T1, T2>& a, const std::pair<T1, T2>& b) const
	{
		return a.first < b.first;
	}
};

template <typename T1, typename T2>
struct compare_pair_greater
{
	bool operator()(const std::pair<T1, T2>& a, const std::pair<T1, T2>& b) const
	{
		if (!(a.first < b.first))
			return true;
		else if (!(b.first < a.first))
			return false;
		else
			return !(a.second < b.second);
	}
};

// Use to compare the contents of two pointers (e.g. std::string*)
template <typename T>
struct compare_pointer_contents
{
	typedef const T* Tptr;
	bool operator()(const Tptr& a, const Tptr& b) const
	{
		return *a < *b;
	}
};

// DeletePointer is a simple helper for deleting all pointers in a container.
// The general form is:
//
//  std::for_each(cont.begin(), cont.end(), DeletePointer());
//  somemap.clear();
//
// Don't forget to clear()!

struct DeletePointer
{
	template<typename T> void operator()(T* ptr) const
	{
		delete ptr;
	}
};
struct DeletePointerArray
{
	template<typename T> void operator()(T* ptr) const
	{
		delete[] ptr;
	}
};

// DeletePairedPointer is a simple helper for deleting all pointers in a map.
// The general form is:
//
//  std::for_each(somemap.begin(), somemap.end(), DeletePairedPointer());

struct DeletePairedPointer
{
	template<typename T> void operator()(T &ptr) const
	{
		delete ptr.second;
		ptr.second = NULL;
	}
};
struct DeletePairedPointerArray
{
	template<typename T> void operator()(T &ptr) const
	{
		delete[] ptr.second;
		ptr.second = NULL;
	}
};


// Alternate version of the above so that has a more cumbersome
// syntax, but it can be used with compositional functors.
// NOTE: The functor retuns a bool because msdev bombs during the
// composition if you return void. Once we upgrade to a newer
// compiler, the second unary_function template parameter can be set
// to void.
//
// Here's a snippit showing how you use this object:
//
// typedef std::map<int, widget*> map_type;
// map_type widget_map;
// ... // add elements
// // delete them all
// for_each(widget_map.begin(),
//          widget_map.end(),
//          llcompose1(DeletePointerFunctor<widget>(),
//                     llselect2nd<map_type::value_type>()));

template<typename T>
struct DeletePointerFunctor : public std::unary_function<T*, bool>
{
	bool operator()(T* ptr) const
	{
		delete ptr;
		return true;
	}
};

// See notes about DeleteArray for why you should consider avoiding this.
template<typename T>
struct DeleteArrayFunctor : public std::unary_function<T*, bool>
{
	bool operator()(T* ptr) const
	{
		delete[] ptr;
		return true;
	}
};

// CopyNewPointer is a simple helper which accepts a pointer, and
// returns a new pointer built with the copy constructor. Example:
//
//  transform(in.begin(), in.end(), out.end(), CopyNewPointer());

struct CopyNewPointer
{
	template<typename T> T* operator()(const T* ptr) const
	{
		return new T(*ptr);
	}
};

template<typename T, typename ALLOC>
void delete_and_clear(std::list<T*, ALLOC>& list)
{
	std::for_each(list.begin(), list.end(), DeletePointer());
	list.clear();
}

template<typename T, typename ALLOC>
void delete_and_clear(std::vector<T*, ALLOC>& vector)
{
	std::for_each(vector.begin(), vector.end(), DeletePointer());
	vector.clear();
}

template<typename T, typename COMPARE, typename ALLOC>
void delete_and_clear(std::set<T*, COMPARE, ALLOC>& set)
{
	std::for_each(set.begin(), set.end(), DeletePointer());
	set.clear();
}

template<typename K, typename V, typename COMPARE, typename ALLOC>
void delete_and_clear(std::map<K, V*, COMPARE, ALLOC>& map)
{
	std::for_each(map.begin(), map.end(), DeletePairedPointer());
	map.clear();
}

template<typename T>
void delete_and_clear(T*& ptr)
{
	delete ptr;
	ptr = NULL;
}


template<typename T>
void delete_and_clear_array(T*& ptr)
{
	delete[] ptr;
	ptr = NULL;
}

// helper function which returns true if key is in inmap.
template <typename T>
//Singu note: This has been generalized to support a broader range of map-esque containers
inline bool is_in_map(const T& inmap, typename T::key_type const& key)
{
	return inmap.find(key) != inmap.end();
}

// Similar to get_ptr_in_map, but for any type with a valid T(0) constructor.
// To replace LLSkipMap getIfThere, use:
//   get_if_there(map, key, 0)
// WARNING: Make sure default_value (generally 0) is not a valid map entry!
//
//Singu note: This has been generalized to support a broader range of map-esque containers.
template <typename T>
inline typename T::mapped_type get_if_there(const T& inmap, typename T::key_type const& key, typename T::mapped_type default_value)
{
	// Typedef here avoids warnings because of new c++ naming rules.
	typedef typename T::const_iterator map_iter;
	map_iter iter = inmap.find(key);
	if(iter == inmap.end())
	{
		return default_value;
	}
	else
	{
		return iter->second;
	}
};

// Simple function to help with finding pointers in maps.
// For example:
// 	typedef  map_t;
//  std::map<int, const char*> foo;
//	foo[18] = "there";
//	foo[2] = "hello";
// 	const char* bar = get_ptr_in_map(foo, 2); // bar -> "hello"
//  const char* baz = get_ptr_in_map(foo, 3); // baz == NULL
//Singu note: This has been generalized to support a broader range of map-esque containers
template <typename T>
inline typename T::mapped_type get_ptr_in_map(const T& inmap, typename T::key_type const& key)
{
	return get_if_there(inmap,key,NULL);
};

template <typename T, typename P>
inline typename T::iterator get_in_vec(T& invec, P pred)
{
	return std::find_if(invec.begin(), invec.end(), pred);
}

template <typename T, typename K>
inline typename T::value_type::second_type& get_val_in_pair_vec(T& invec, K key)
{
	auto it = get_in_vec(invec, [&key](typename T::value_type& e) { return e.first == key; });
	if (it == invec.end())
	{
		invec.emplace_back(key, typename T::value_type::second_type());
		return invec.back().second;
	}
	return it->second;
}

// Useful for replacing the removeObj() functionality of LLDynamicArray
// Example:
//  for (std::vector<T>::iterator iter = mList.begin(); iter != mList.end(); )
//  {
//    if ((*iter)->isMarkedForRemoval())
//      iter = vector_replace_with_last(mList, iter);
//    else
//      ++iter;
//  }
//
//Singu note: This has been generalized to support a broader range of sequence containers
template <typename T>
inline typename T::iterator vector_replace_with_last(T& invec, typename T::iterator iter)
{
	typename T::iterator last = invec.end();
	if (iter == invec.end())
	{
		return iter;
	}
	else if (iter == --last)
	{
		invec.pop_back();
		return invec.end();
	}
	else
	{
		*iter = *last;
		invec.pop_back();
		return iter;
	}
};

// Useful for replacing the removeObj() functionality of LLDynamicArray
// Example:
//   vector_replace_with_last(mList, x);
//
//Singu note: This has been generalized to support a broader range of sequence containers
template <typename T>
inline bool vector_replace_with_last(T& invec, typename T::value_type const& val)
{
	typename T::iterator iter = std::find(invec.begin(), invec.end(), val);
	if (iter != invec.end())
	{
		typename T::iterator last = invec.end(); --last;
		*iter = *last;
		invec.pop_back();
		return true;
	}
	return false;
}

// Append N elements to the vector and return a pointer to the first new element.
template <typename T>
inline T* vector_append(std::vector<T>& invec, size_t N)
{
	size_t sz = invec.size();
	invec.resize(sz+N);
	return &(invec[sz]);
}

// call function f to n members starting at first. similar to std::for_each
template <class InputIter, class Size, class Function>
Function ll_for_n(InputIter first, Size n, Function f)
{
	for ( ; n > 0; --n, ++first)
		f(*first);
	return f;
}

// copy first to result n times, incrementing each as we go
template <class InputIter, class Size, class OutputIter>
OutputIter ll_copy_n(InputIter first, Size n, OutputIter result)
{
	for ( ; n > 0; --n, ++result, ++first)
		*result = *first;
	return result;
}

// set  *result = op(*f) for n elements of f
template <class InputIter, class OutputIter, class Size, class UnaryOp>
OutputIter ll_transform_n(
	InputIter first,
	Size n,
	OutputIter result,
	UnaryOp op)
{
	for ( ; n > 0; --n, ++result, ++first)
		*result = op(*first);
	return result;
}



/*
 *
 * Copyright (c) 1994
 * Hewlett-Packard Company
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Hewlett-Packard Company makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 *
 * Copyright (c) 1996-1998
 * Silicon Graphics Computer Systems, Inc.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Silicon Graphics makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 */


// helper to deal with the fact that MSDev does not package
// select... with the stl. Look up usage on the sgi website.

template <class _Pair>
struct _LLSelect1st : public std::unary_function<_Pair, typename _Pair::first_type> {
  const typename _Pair::first_type& operator()(const _Pair& __x) const {
	return __x.first;
  }
};

template <class _Pair>
struct _LLSelect2nd : public std::unary_function<_Pair, typename _Pair::second_type>
{
  const typename _Pair::second_type& operator()(const _Pair& __x) const {
	return __x.second;
  }
};

template <class _Pair> struct llselect1st : public _LLSelect1st<_Pair> {};
template <class _Pair> struct llselect2nd : public _LLSelect2nd<_Pair> {};

// helper to deal with the fact that MSDev does not package
// compose... with the stl. Look up usage on the sgi website.

template <class _Operation1, class _Operation2>
class ll_unary_compose :
	public std::unary_function<typename _Operation2::argument_type,
							   typename _Operation1::result_type>
{
protected:
  _Operation1 __op1;
  _Operation2 __op2;
public:
  ll_unary_compose(const _Operation1& __x, const _Operation2& __y)
	: __op1(__x), __op2(__y) {}
  typename _Operation1::result_type
  operator()(const typename _Operation2::argument_type& __x) const {
	return __op1(__op2(__x));
  }
};

template <class _Operation1, class _Operation2>
inline ll_unary_compose<_Operation1,_Operation2>
llcompose1(const _Operation1& __op1, const _Operation2& __op2)
{
  return ll_unary_compose<_Operation1,_Operation2>(__op1, __op2);
}

template <class _Operation1, class _Operation2, class _Operation3>
class ll_binary_compose
  : public std::unary_function<typename _Operation2::argument_type,
							   typename _Operation1::result_type> {
protected:
  _Operation1 _M_op1;
  _Operation2 _M_op2;
  _Operation3 _M_op3;
public:
  ll_binary_compose(const _Operation1& __x, const _Operation2& __y,
					const _Operation3& __z)
	: _M_op1(__x), _M_op2(__y), _M_op3(__z) { }
  typename _Operation1::result_type
  operator()(const typename _Operation2::argument_type& __x) const {
	return _M_op1(_M_op2(__x), _M_op3(__x));
  }
};

template <class _Operation1, class _Operation2, class _Operation3>
inline ll_binary_compose<_Operation1, _Operation2, _Operation3>
llcompose2(const _Operation1& __op1, const _Operation2& __op2,
		 const _Operation3& __op3)
{
  return ll_binary_compose<_Operation1,_Operation2,_Operation3>
	(__op1, __op2, __op3);
}

// helpers to deal with the fact that MSDev does not package
// bind... with the stl. Again, this is from sgi.
template <class _Operation>
class llbinder1st :
	public std::unary_function<typename _Operation::second_argument_type,
							   typename _Operation::result_type> {
protected:
  _Operation op;
  typename _Operation::first_argument_type value;
public:
  llbinder1st(const _Operation& __x,
			  const typename _Operation::first_argument_type& __y)
	  : op(__x), value(__y) {}
	typename _Operation::result_type
	operator()(const typename _Operation::second_argument_type& __x) const {
		return op(value, __x);
	}
};

template <class _Operation, class _Tp>
inline llbinder1st<_Operation>
llbind1st(const _Operation& __oper, const _Tp& __x)
{
  typedef typename _Operation::first_argument_type _Arg1_type;
  return llbinder1st<_Operation>(__oper, _Arg1_type(__x));
}

template <class _Operation>
class llbinder2nd
	: public std::unary_function<typename _Operation::first_argument_type,
								 typename _Operation::result_type> {
protected:
	_Operation op;
	typename _Operation::second_argument_type value;
public:
	llbinder2nd(const _Operation& __x,
				const typename _Operation::second_argument_type& __y)
		: op(__x), value(__y) {}
	typename _Operation::result_type
	operator()(const typename _Operation::first_argument_type& __x) const {
		return op(__x, value);
	}
};

template <class _Operation, class _Tp>
inline llbinder2nd<_Operation>
llbind2nd(const _Operation& __oper, const _Tp& __x)
{
  typedef typename _Operation::second_argument_type _Arg2_type;
  return llbinder2nd<_Operation>(__oper, _Arg2_type(__x));
}

/**
 * Compare std::type_info* pointers a la std::less. We break this out as a
 * separate function for use in two different std::less specializations.
 */
inline
bool before(const std::type_info* lhs, const std::type_info* rhs)
{
    // Just use before(), as we normally would
    return lhs->before(*rhs) ? true : false;
}

/**
 * Specialize std::less<std::type_info*> to use std::type_info::before().
 * See MAINT-1175. It is NEVER a good idea to directly compare std::type_info*
 * because, on Linux, you might get different std::type_info* pointers for the
 * same type (from different load modules)!
 */
namespace std
{
	template <>
	struct less<const std::type_info*>:
		public std::binary_function<const std::type_info*, const std::type_info*, bool>
	{
		bool operator()(const std::type_info* lhs, const std::type_info* rhs) const
		{
			return before(lhs, rhs);
		}
	};

	template <>
	struct less<std::type_info*>:
		public std::binary_function<std::type_info*, std::type_info*, bool>
	{
		bool operator()(std::type_info* lhs, std::type_info* rhs) const
		{
			return before(lhs, rhs);
		}
	};
} // std


/**
 * Implementation for ll_template_cast() (q.v.).
 *
 * Default implementation: trying to cast two completely unrelated types
 * returns 0. Typically you'd specify T and U as pointer types, but in fact T
 * can be any type that can be initialized with 0.
 */
template <typename T, typename U>
struct ll_template_cast_impl
{
	T operator()(U)
	{
		return 0;
	}
};

/**
 * ll_template_cast<T>(some_value) is for use in a template function when
 * some_value might be of arbitrary type, but you want to recognize type T
 * specially.
 *
 * It's designed for use with pointer types. Example:
 * @code
 * struct SpecialClass
 * {
 *     void someMethod(const std::string&) const;
 * };
 *
 * template <class REALCLASS>
 * void somefunc(const REALCLASS& instance)
 * {
 *     const SpecialClass* ptr = ll_template_cast<const SpecialClass*>(&instance);
 *     if (ptr)
 *     {
 *         ptr->someMethod("Call method only available on SpecialClass");
 *     }
 * }
 * @endcode
 *
 * Why is this better than dynamic_cast<>? Because unless OtherClass is
 * polymorphic, the following won't even compile (gcc 4.0.1):
 * @code
 * OtherClass other;
 * SpecialClass* ptr = dynamic_cast<SpecialClass*>(&other);
 * @endcode
 * to say nothing of this:
 * @code
 * void function(int);
 * SpecialClass* ptr = dynamic_cast<SpecialClass*>(&function);
 * @endcode
 * ll_template_cast handles these kinds of cases by returning 0.
 */
template <typename T, typename U>
T ll_template_cast(U value)
{
	return ll_template_cast_impl<T, U>()(value);
}

/**
 * Implementation for ll_template_cast() (q.v.).
 *
 * Implementation for identical types: return same value.
 */
template <typename T>
struct ll_template_cast_impl<T, T>
{
	T operator()(T value)
	{
		return value;
	}
};

/**
 * LL_TEMPLATE_CONVERTIBLE(dest, source) asserts that, for a value @c s of
 * type @c source, <tt>ll_template_cast<dest>(s)</tt> will return @c s --
 * presuming that @c source can be converted to @c dest by the normal rules of
 * C++.
 *
 * By default, <tt>ll_template_cast<dest>(s)</tt> will return 0 unless @c s's
 * type is literally identical to @c dest. (This is because of the
 * straightforward application of template specialization rules.) That can
 * lead to surprising results, e.g.:
 *
 * @code
 * Foo myFoo;
 * const Foo* fooptr = ll_template_cast<const Foo*>(&myFoo);
 * @endcode
 *
 * Here @c fooptr will be 0 because <tt>&myFoo</tt> is of type <tt>Foo*</tt>
 * -- @em not <tt>const Foo*</tt>. (Declaring <tt>const Foo myFoo;</tt> would
 * force the compiler to do the right thing.)
 *
 * More disappointingly:
 * @code
 * struct Base {};
 * struct Subclass: public Base {};
 * Subclass object;
 * Base* ptr = ll_template_cast<Base*>(&object);
 * @endcode
 *
 * Here @c ptr will be 0 because <tt>&object</tt> is of type
 * <tt>Subclass*</tt> rather than <tt>Base*</tt>. We @em want this cast to
 * succeed, but without our help ll_template_cast can't recognize it.
 *
 * The following would suffice:
 * @code
 * LL_TEMPLATE_CONVERTIBLE(Base*, Subclass*);
 * ...
 * Base* ptr = ll_template_cast<Base*>(&object);
 * @endcode
 *
 * However, as noted earlier, this is easily fooled:
 * @code
 * const Base* ptr = ll_template_cast<const Base*>(&object);
 * @endcode
 * would still produce 0 because we haven't yet seen:
 * @code
 * LL_TEMPLATE_CONVERTIBLE(const Base*, Subclass*);
 * @endcode
 *
 * @TODO
 * This macro should use Boost type_traits facilities for stripping and
 * re-adding @c const and @c volatile qualifiers so that invoking
 * LL_TEMPLATE_CONVERTIBLE(dest, source) will automatically generate all
 * permitted permutations. It's really not fair to the coder to require
 * separate:
 * @code
 * LL_TEMPLATE_CONVERTIBLE(Base*, Subclass*);
 * LL_TEMPLATE_CONVERTIBLE(const Base*, Subclass*);
 * LL_TEMPLATE_CONVERTIBLE(const Base*, const Subclass*);
 * @endcode
 *
 * (Naturally we omit <tt>LL_TEMPLATE_CONVERTIBLE(Base*, const Subclass*)</tt>
 * because that's not permitted by normal C++ assignment anyway.)
 */
#define LL_TEMPLATE_CONVERTIBLE(DEST, SOURCE)   \
template <>                                     \
struct ll_template_cast_impl<DEST, SOURCE>      \
{                                               \
	DEST operator()(SOURCE wrapper)             \
	{                                           \
		return wrapper;                         \
	}                                           \
}


#endif // LL_LLSTL_H

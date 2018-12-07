
// Copyright 2017-2018 by Tom Clunie
// https://github.com/clunietp/value_ptr
// Distributed under the Boost Software License, Version 1.0.
//    (See http://www.boost.org/LICENSE_1_0.txt)

#ifndef SMART_PTR_VALUE_PTR
#define SMART_PTR_VALUE_PTR

#include <memory>		// unique_ptr
#include <functional>	// less_equal
#include <cassert>		// assert

#if defined( _MSC_VER)	

#if (_MSC_VER >= 1915)	// constexpr tested/working _MSC_VER 1915 (vs17 15.8)
#define VALUE_PTR_CONSTEXPR constexpr
#else	// msvc 15 bug prevents constexpr in some cases
#define VALUE_PTR_CONSTEXPR
#endif

//	https://blogs.msdn.microsoft.com/vcblog/2016/03/30/optimizing-the-layout-of-empty-base-classes-in-vs2015-update-2-3/
#define USE_EMPTY_BASE_OPTIMIZATION __declspec(empty_bases)	// requires vs2015 update 2 or later.  still needed vs2017

#else
#define VALUE_PTR_CONSTEXPR constexpr 
#define USE_EMPTY_BASE_OPTIMIZATION
#endif

namespace smart_ptr {

	namespace detail {

		// is_incomplete<T>, based on https://stackoverflow.com/a/39816909
		template <class, class = void> struct is_incomplete : std::true_type {};
		template <class T> struct is_incomplete<
			T
			, typename std::enable_if<std::is_object<T>::value && !std::is_pointer<T>::value && (sizeof(T) > 0)>::type
			>
			: std::false_type
		{};

		// has clone() method detection
		template<class T, class = void> struct has_clone : std::false_type {};
		template<class T> struct has_clone<T, decltype(void(std::declval<T>().clone()))> : std::true_type {};

		// Returns flag if test passes (false==slicing is probable)
		// T==base pointer, U==derived/supplied pointer
		template <typename T, typename U, bool IsDefaultCopier>
		struct slice_test : std::integral_constant<bool, 
			std::is_same<T, U>::value	// if U==T, no need to check for slicing
			|| std::is_same<std::nullptr_t, U>::value	// nullptr is fine
			|| !IsDefaultCopier	// user provided cloner, assume they're handling it
			|| has_clone<typename std::remove_pointer<U>::type>::value	// using default cloner, clone method must exist in U
		>::type 
		{};

		// ptr_data:  holds pointer, deleter, copier
		//	pointer and deleter held in unique_ptr member, this struct is derived from copier to minimize overall footprint
		//	uses EBCO to solve sizeof(value_ptr<T>) == sizeof(T*) problem
		template <typename T, typename Deleter, typename Copier>
			struct 
			USE_EMPTY_BASE_OPTIMIZATION
			ptr_data
			: public Copier
		{
			
			using unique_ptr_type = std::unique_ptr<T, Deleter>;
			using pointer = typename unique_ptr_type::pointer;
			using deleter_type = typename unique_ptr_type::deleter_type;
			using copier_type = Copier;

			unique_ptr_type uptr;

			ptr_data() = default;

			template <typename Dx, typename Cx>
			constexpr ptr_data( T* px, Dx&& dx, Cx&& cx )
				: copier_type( std::forward<Cx>(cx) )
				, uptr(px, std::forward<Dx>(dx))
			{}

			ptr_data( ptr_data&& ) = default;
			ptr_data& operator=( ptr_data&& ) = default;
			
			constexpr ptr_data( const ptr_data& that )
				: ptr_data( that.clone() )
			{}

			ptr_data& operator=( const ptr_data& that ) {
				if ( this != &that )
					*this = that.clone();
				return *this;
			}

			// get_copier, analogous to std::unique_ptr<T>::get_deleter()
			copier_type& get_copier() { return *this; }

			// get_copier, analogous to std::unique_ptr<T>::get_deleter()
			const copier_type& get_copier() const { return *this; }

			ptr_data clone() const {
				// get a copier, use it to clone ptr, construct/return a ptr_data
				return{ 
					(T*)this->get_copier()(this->uptr.get())
					, this->uptr.get_deleter()
					, this->get_copier() 
				};
			}

		};	// ptr_data

		// ptr_base:	value_ptr base class
		//	holds ptr_data
		template <typename T, typename Deleter, typename Copier>
		struct ptr_base {

			using deleter_type = Deleter;
			using copier_type = Copier;
			using _data_type = ptr_data<T, deleter_type, copier_type>;

			_data_type _data;

			using pointer = typename _data_type::pointer;
			using unique_ptr_type = std::unique_ptr<T, Deleter>;

			template <typename Px, typename Dx, typename Cx>
			constexpr ptr_base( Px&& px, Dx&& dx, Cx&& cx )
				: _data(
					std::forward<Px>( px )
					, std::forward<Dx>( dx )
					, std::forward<Cx>(cx)
				)
			{}

			// return unique_ptr
			const unique_ptr_type& uptr() const & {
				return this->_data.uptr;
			}

			// return unique_ptr
			unique_ptr_type& uptr() & {
				return this->_data.uptr;
			}
			
			// conversion to unique_ptr, ref qualified
			operator unique_ptr_type const&() const & {
				return this->uptr();
			}

			// conversion to unique_ptr, ref qualified
			operator unique_ptr_type& () & {
				return this->uptr();
			}

			deleter_type& get_deleter() { return this->uptr().get_deleter(); }
			const deleter_type& get_deleter() const { return this->uptr().get_deleter(); }

			copier_type& get_copier() { return this->_data.get_copier(); }
			const copier_type& get_copier() const { return this->_data.get_copier(); }

		};	// ptr_base

		// wraps a functor (Op), intercepts calls to Op::operator(), and forwards the call to the specified delegate
		//	this is a smallest-footprint dynamic dispatch approach to handling (potentially) incomplete types
		//	delegate should have the signature: result( [const] functor_wrapper<...>& (or [const] Op&), params... (from Op::operator()) )
		//	inheriting from Op to minimize sizeof(functor_wrapper)
		template <typename Op, typename Delegate>
		struct
			functor_wrapper
			: public Op	
		{			
			// delegate function to call
			Delegate delegate_;

			// construct with Op, Delegate
			template <typename Op_, typename Delegate_>
			constexpr functor_wrapper( Op_&& op, Delegate_&& del)
				: Op(std::forward<Op>(op))
				, delegate_(std::forward<Delegate>(del))
			{}
			
			// invoked for event, const
			template <typename... Args>
			auto operator()(Args&&... args) const -> typename std::result_of<Op(Args...)>::type {
				if (this->delegate_ == nullptr)
					throw std::bad_function_call();
				return this->delegate_( *this, std::forward<Args>(args)...);	//	call delegate, with reference to this as first parameter
			}

			// invoked for event
			template <typename... Args>
			auto operator()(Args&&... args) -> typename std::result_of<Op(Args...)>::type {
				if (this->delegate_ == nullptr)
					throw std::bad_function_call();
				return this->delegate_( *this, std::forward<Args>(args)...);	//	call delegate, with const reference to this as first parameter
			}

		};	// functor_wrapper
		
		// ptr_base_incomplete:	intermediate base class for incomplete types
		//	wraps copy and delete ops in functor wrappers to handle incomplete types
		template <typename T, typename DeleteOp, typename CopyOp
			, typename Deleter = functor_wrapper<DeleteOp, void(*)(const DeleteOp&, T*)>
			, typename Copier = functor_wrapper<CopyOp, T*(*)(const CopyOp&, const T*)>
		>
			struct ptr_base_incomplete
			: ptr_base<T, Deleter, Copier> {

			using base_type = ptr_base<T, Deleter, Copier>;
			using pointer = typename base_type::pointer;

			// default construct for incomplete type
			template <typename Dx, typename Cx>
			constexpr ptr_base_incomplete(std::nullptr_t, Dx&& dx, Cx&& cx)
				: base_type(
					nullptr
					, Deleter(std::forward<Dx>(dx), [](const DeleteOp&, T* ptr) { assert(ptr == nullptr); })
					, Copier(std::forward<Cx>(cx), [](const CopyOp&, const T* ptr) -> T* { assert(ptr == nullptr); return nullptr; })
				)
			{}

			template <typename Dx, typename Cx>
			constexpr ptr_base_incomplete(pointer px, Dx&& dx, Cx&& cx)
				: base_type(
					px
					, Deleter(std::forward<Dx>(dx), [](const DeleteOp& op, T* ptr) { op(ptr); } )
					, Copier(std::forward<Cx>(cx), [](const CopyOp& op, const T* ptr) -> T* { return op(ptr); } )
				)
			{}

		};	// ptr_base_incomplete

	}	// detail

	template <typename T>
	struct 
		USE_EMPTY_BASE_OPTIMIZATION
		default_copy {

	private:
		struct _clone_tag {};
		struct _copy_tag {};

		T* operator()(const T* what, _clone_tag) const {
			return what->clone();
		}

		T* operator()(const T* what, _copy_tag) const {
			return new T(*what);
		}	// _copy

	public:
		// copy operator
		T* operator()( const T* what ) const {

			if ( !what )
				return nullptr;

			// tag dispatch on has_clone
			return this->operator()( what
				, typename std::conditional<detail::has_clone<T>::value, _clone_tag, _copy_tag>::type() 
				);
		}	// operator()

	};	// default_copy

	template <typename T
		, typename Deleter = std::default_delete<T>
		, typename Copier = default_copy<T>
		, typename Base = 
			typename std::conditional<detail::is_incomplete<T>::value, 
				detail::ptr_base_incomplete<T, Deleter, Copier>
				, detail::ptr_base<T, Deleter, Copier>
			>::type
	>
		struct value_ptr 
			: Base
		{
			using base_type = Base;
			using element_type = T;

			using pointer = typename base_type::pointer;
			using const_pointer = const pointer;
			
			using reference = typename std::add_lvalue_reference<element_type>::type;
			using const_reference = const reference;

			using deleter_type = Deleter;
			using copier_type = Copier;
			
			// construct with pointer, deleter, copier
			template <typename Px>
			constexpr value_ptr( Px px, deleter_type dx, copier_type cx )
				: base_type( px
					, std::move( dx )
					, std::move( cx )
				)
			{
				static_assert(
					detail::slice_test<pointer, Px, std::is_same<default_copy<T>, copier_type>::value>::value
					, "value_ptr; clone() method not detected and not using custom copier; slicing may occur"
					);
			}

			// construct with pointer, deleter
			template <typename Px>
			VALUE_PTR_CONSTEXPR
			value_ptr( Px px, deleter_type dx )
				: value_ptr( px, std::move(dx), copier_type() )
			{}

			// construct with pointer
			template <typename Px>
			VALUE_PTR_CONSTEXPR
			value_ptr( Px px )
				: value_ptr( px, deleter_type(), copier_type() )
			{}

			// construct from unique_ptr, copier
			VALUE_PTR_CONSTEXPR
			value_ptr(std::unique_ptr<T, Deleter> uptr, copier_type copier = copier_type() )
				: value_ptr(uptr.release(), uptr.get_deleter(), std::move(copier) )
			{}

			// std::nullptr_t, default ctor 
			explicit 
			VALUE_PTR_CONSTEXPR
			value_ptr( std::nullptr_t = nullptr )
				: value_ptr( nullptr, deleter_type(), copier_type() )
			{}
			
			// get pointer
			pointer get() { return this->uptr().get(); }

			// get const pointer
			const_pointer get() const { return this->uptr().get(); }

			// reset pointer
			template <typename Px = std::nullptr_t>
			void reset( Px px = nullptr ) {

				static_assert(
					detail::slice_test<pointer, Px, std::is_same<default_copy<T>, copier_type>::value>::value
					, "value_ptr; clone() method not detected and not using custom copier; slicing may occur"
					);

				*this = value_ptr( px, this->get_deleter(), this->get_copier() );
			}

			// release pointer
			pointer release() {
				return this->uptr().release();
			}	// release

			// return flag if has pointer
			explicit operator bool() const {
				return this->get() != nullptr;
			}

			const_reference operator*() const { return *this->get(); }
			reference operator*() { return *this->get(); }

			const_pointer operator-> () const { return this->get(); }
			pointer operator-> () { return this->get(); }
			
			void swap( value_ptr& that ) { std::swap( this->_data, that._data ); }

	};// value_ptr

	  // non-member swap
	template <class T1, class D1, class C1, class T2, class D2, class C2> void swap( value_ptr<T1, D1, C1>& x, value_ptr<T2, D2, C2>& y ) { x.swap( y ); }

	// non-member operators
	template <class T1, class D1, class C1, class T2, class D2, class C2> bool operator == ( const value_ptr<T1, D1, C1>& x, const value_ptr<T2, D2, C2>& y ) { return x.get() == y.get(); }
	template<class T1, class D1, class C1, class T2, class D2, class C2> bool operator != ( const value_ptr<T1, D1, C1>& x, const value_ptr<T2, D2, C2>& y ) { return x.get() != y.get(); }
	template<class T1, class D1, class C1, class T2, class D2, class C2> bool operator < ( const value_ptr<T1, D1, C1>& x, const value_ptr<T2, D2, C2>& y ) {
		using common_type = typename std::common_type<typename value_ptr<T1, D1, C1>::pointer, typename value_ptr<T2, D2, C2>::pointer>::type;
		return std::less<common_type>()( x.get(), y.get() );
	}
	template<class T1, class D1, class C1, class T2, class D2, class C2> bool operator <= ( const value_ptr<T1, D1, C1>& x, const value_ptr<T2, D2, C2>& y ) { return !( y < x ); }
	template<class T1, class D1, class C1, class T2, class D2, class C2> bool operator >( const value_ptr<T1, D1, C1>& x, const value_ptr<T2, D2, C2>& y ) { return y < x; }
	template<class T1, class D1, class C1, class T2, class D2, class C2> bool operator >= ( const value_ptr<T1, D1, C1>& x, const value_ptr<T2, D2, C2>& y ) { return !( x < y ); }
	template <class T, class D, class C> bool operator == ( const value_ptr<T, D, C>& x, std::nullptr_t ) noexcept { return !x; }
	template <class T, class D, class C> bool operator == ( std::nullptr_t, const value_ptr<T, D, C>& x ) noexcept { return !x; }
	template <class T, class D, class C> bool operator != ( const value_ptr<T, D, C>& x, std::nullptr_t ) noexcept { return (bool)x; }
	template <class T, class D, class C> bool operator != ( std::nullptr_t, const value_ptr<T, D, C>& x ) noexcept { return (bool)x; }
	template <class T, class D, class C> bool operator < ( const value_ptr<T, D, C>& x, std::nullptr_t ) { return std::less<typename value_ptr<T, D, C>::pointer>()( x.get(), nullptr ); }
	template <class T, class D, class C> bool operator<( std::nullptr_t, const value_ptr<T, D, C>& y ) { return std::less<typename value_ptr<T, D, C>::pointer>()( nullptr, y.get() ); }
	template <class T, class D, class C> bool operator <= ( const value_ptr<T, D, C>& x, std::nullptr_t ) { return std::less_equal<typename value_ptr<T, D, C>::pointer>()( x.get(), nullptr ); }
	template <class T, class D, class C> bool operator <= ( std::nullptr_t, const value_ptr<T, D, C>& y ) { return std::less_equal<typename value_ptr<T, D, C>::pointer>()( nullptr, y.get() ); }
	template <class T, class D, class C> bool operator >( const value_ptr<T, D, C>& x, std::nullptr_t ) { return !( nullptr < x ); }
	template <class T, class D, class C> bool operator > ( std::nullptr_t, const value_ptr<T, D, C>& y ) { return !( y < nullptr ); }
	template <class T, class D, class C> bool operator >= ( const value_ptr<T, D, C>& x, std::nullptr_t ) { return !( x < nullptr ); }
	template <class T, class D, class C> bool operator >= ( std::nullptr_t, const value_ptr<T, D, C>& y ) { return !( nullptr < y ); }

	// make value_ptr with default deleter and copier, analogous to std::make_unique
	template<typename T, typename... Args>
	value_ptr<T> make_value(Args&&... args) {
		return value_ptr<T>(new T(std::forward<Args>(args)...));
	}

	// make a value_ptr from pointer with custom deleter and copier
	template <typename T, typename Deleter = std::default_delete<T>, typename Copier = default_copy<T>>
	static inline auto make_value_ptr(T* ptr, Deleter&& dx = {}, Copier&& cx = {}) -> value_ptr<T, Deleter, Copier> {
		return value_ptr<T, Deleter, Copier>( ptr, std::forward<Deleter>( dx ), std::forward<Copier>(cx) );
	}	// make_value_ptr

}	// smart_ptr ns

#undef VALUE_PTR_CONSTEXPR
#undef USE_EMPTY_BASE_OPTIMIZATION

#endif // !SMART_PTR_VALUE_PTR


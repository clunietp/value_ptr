
// Distributed under the Boost Software License, Version 1.0.
//    (See http://www.boost.org/LICENSE_1_0.txt)

#ifndef SMART_PTR_VALUE_PTR
#define SMART_PTR_VALUE_PTR

#include <memory>		// unique_ptr
#include <functional>	// less_equal
#include <cassert>		// assert

#if defined( _MSC_VER)	// todo:  check constexpr/delegating ctor issue in vs17.  issue persists in vs15 update 3 despite ms closed bug as fixed, or i'm doing something wrong
#define VALUE_PTR_CONSTEXPR 
#else
#define VALUE_PTR_CONSTEXPR constexpr 
#endif

namespace smart_ptr {

	namespace detail {

		// void_t for c++11
		//	from http://en.cppreference.com/w/cpp/types/void_t
		template<typename... Ts> struct make_void { typedef void type; };
		template<typename... Ts> using void_t = typename make_void<Ts...>::type;

		// is_defined<T>, from https://stackoverflow.com/a/39816909
		template <class, class = void> struct is_defined : std::false_type { };
		template <class T> struct is_defined<
			T
			, typename std::enable_if<std::is_object<T>::value && !std::is_pointer<T>::value && ( sizeof( T ) > 0 )>::type
			>
			: std::true_type{}
		;

		// Class function/type detection
		//	https://stackoverflow.com/a/30848101

		// Primary template handles all types not supporting the operation.
		template <typename, template <typename> class, typename = void_t<>>
		struct detect : std::false_type {};

		// Specialization recognizes/validates only types supporting the archetype.
		template <typename T, template <typename> class Op>

		struct detect<T, Op, void_t<Op<T>>> : std::true_type {};

		// clone function
		template <typename T> using fn_clone_t = decltype( std::declval<T>().clone() );

		// has_clone
		template <typename T> using has_clone = detect<T, fn_clone_t>;

		// Returns flag if test passes (false==slicing is probable)
		// T==base pointer, U==derived/supplied pointer
		template <typename T, typename U, bool IsDefaultCopier>
		struct slice_test : std::conditional<
			std::is_same<T, U>::value	// if U==T, no need to check for slicing
			|| std::is_same<std::nullptr_t, U>::value	// nullptr is fine
			|| !IsDefaultCopier	// user provided cloner, assume they're handling it
			|| has_clone<typename std::remove_pointer<U>::type>::value	// using default cloner, clone method must exist in U
			, std::true_type
			, std::false_type
		>::type {};

		// op_wrapper wraps Op::operator() and dispatches to observer fn
		//	observer fn then calls op_wrapper.op() to invoke Op::operator()
		//	this redirection is needed to call the actual operation in a context when T is actually defined
		template <typename T, typename Op, typename R, typename ObserverFnSig>
		struct op_wrapper : public Op {
			using this_type = op_wrapper<T, Op, R, ObserverFnSig>;
			using return_type = R;

			// observer function to call
			ObserverFnSig observer_fn;

			template <typename Op_, typename Fn>
			constexpr op_wrapper( Op_&& op, Fn&& obs )
				: Op( std::forward<Op_>( op ) )
				, observer_fn( std::forward<Fn>( obs ) )
			{}

			// invoked for event
			template <typename... Args>
			return_type operator()( Args&&... args ) const {
				assert( this->observer_fn != nullptr );
				//	here we want to notify observer of event, with reference to this as first parameter
				return this->observer_fn( (const void*)this, std::forward<Args>( args )... );
			}
			
			// call to actual operation (Op::operator()), invoked by observer
			template <typename... Args>
			return_type op( Args&&... args ) const {
				return Op::operator()( std::forward<Args>(args)... );
			}

		};	// op_wrapper
		
		// ptr_data
		template <typename T, typename Deleter, typename Copier>
			struct
#ifdef _MSC_VER
			//	https://blogs.msdn.microsoft.com/vcblog/2016/03/30/optimizing-the-layout-of-empty-base-classes-in-vs2015-update-2-3/
			__declspec( empty_bases )	// requires vs2015 update 2
#endif
			ptr_data
			: public std::unique_ptr<T, Deleter>
			, public Copier
		{
			using copier_type = Copier;
			using base_type_uptr = std::unique_ptr<T, Deleter>;
			using deleter_type = Deleter;

			ptr_data() = default;

			template <typename Dx, typename Cx>
			constexpr ptr_data( T* px, Dx&& dx, Cx&& cx ) noexcept
				: base_type_uptr( px, std::forward<Dx>(dx) )
				, copier_type( std::forward<Cx>(cx) )
			{}

			copier_type& get_copier() { return static_cast<copier_type&>( *this ); }
			const copier_type& get_copier() const { return static_cast<const copier_type&>( *this ); }

			ptr_data clone() const {
				return{ this->get_copier()( this->get() ), this->get_deleter(), this->get_copier() };
			}

			ptr_data( ptr_data&& ) = default;
			ptr_data& operator=( ptr_data&& ) = default;
			ptr_data( const ptr_data& that )
				: ptr_data( that.clone() )
			{}

			ptr_data& operator=( const ptr_data& that ) {
				if ( this == &that )
					return *this;
				*this = that.clone();
				return *this;
			}

		};	// ptr_data

		// ptr_base:	base class for defined types
		template <typename T, typename Deleter, typename Copier>
		struct ptr_base {

			using _data_type = ptr_data<T, Deleter, Copier>;
			using _pointer = typename _data_type::pointer;
			_data_type _data;

			using pointer = _pointer;

			template <typename Px, typename Dx, typename Cx>
			constexpr ptr_base( Px&& px, Dx&& dx, Cx&& cx )
				: _data(
					std::forward<Px>( px )
					, std::forward<Dx>( dx )
					, std::forward<Cx>(cx)
				)
			{}

			// conversion to unique_ptr
			const typename _data_type::base_type_uptr& ptr() const {
				return this->_data;
			}

			// conversion to unique_ptr
			typename _data_type::base_type_uptr& ptr() {
				return this->_data;
			}
			
			// conversion to unique_ptr
			operator typename _data_type::base_type_uptr const&() const {
				return this->_data;
			}

			// conversion to unique_ptr
			operator typename _data_type::base_type_uptr& () {
				return this->_data;
			}
			

		};	// ptr_base

		// ptr_base_undefined:	intermediate base class for undefined types
		template <typename T, typename DeleteOp, typename CopyOp
			, typename Deleter = op_wrapper<T, DeleteOp, void, void( *)( const void*, T* )>
			, typename Copier = op_wrapper<T, CopyOp, T*, T*(*)(const void*, const T*)>
		>
		struct ptr_base_undefined 
			: ptr_base<T, Deleter, Copier> {

			using base_type = ptr_base<T,Deleter,Copier>;
			using pointer = typename base_type::pointer;

			// default construct for undefined type
			template <typename Dx, typename Cx>
			constexpr ptr_base_undefined( std::nullptr_t, Dx&& dx, Cx&& cx )
				: base_type(
					nullptr
					, Deleter( std::forward<Dx>( dx ), []( const void*, T* ptr ) { assert( ptr == nullptr ); } )
					, Copier( std::forward<Cx>( cx ), []( const void* op, const T* ptr ) -> T* { assert( ptr == nullptr ); return nullptr; } )
				)
			{}

			template <typename Dx, typename Cx>
			constexpr ptr_base_undefined( pointer px, Dx&& dx, Cx&& cx )
				: base_type(
					px
					, Deleter( std::forward<Dx>( dx ), []( const void* op, T* ptr ) {
							if ( ptr )
								static_cast<const Deleter*>( op )->op( ptr );
						}
					)
					, Copier( std::forward<Cx>( cx ), []( const void* op, const T* ptr ) -> T* {
							if ( !ptr )
								return nullptr;
							return static_cast<const Copier*>( op )->op( ptr );
						}
					)
				)
			{}
		};	// ptr_base_undefined

	}	// detail

	template <typename T>
	struct default_copy {

		// copy operator
		T *operator()( const T* what ) const {

			if ( !what )
				return nullptr;

			// tag dispatch on has_clone
			return this->operator()( what
				, typename std::conditional<detail::has_clone<T>::value, _clone, _copy>::type() 
				);
		}	// operator()

	private:
		struct _clone {};
		struct _copy {};

		T* operator()( const T* what, _clone ) const {
			return what->clone();
		}

		T* operator()( const T* what, _copy ) const {
			return new T( *what );
		}	// _copy

	};	// default_copy

	template <typename T
		, typename Deleter = std::default_delete<T>
		, typename Copier = default_copy<T>
		, typename Base = 
			typename std::conditional<detail::is_defined<T>::value, 
				detail::ptr_base<T, Deleter, Copier>
				, detail::ptr_base_undefined<T, Deleter, Copier>
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
			VALUE_PTR_CONSTEXPR value_ptr( Px px, deleter_type dx )	// constexpr here yields c2476 on msvc15
				: value_ptr( px, std::move(dx), copier_type() )
			{}

			// construct with pointer
			template <typename Px>
			VALUE_PTR_CONSTEXPR value_ptr( Px px ) // constexpr here yields c2476 on msvc15
				: value_ptr( px, deleter_type(), copier_type() )
			{}

			// std::nullptr_t, default ctor 
			explicit VALUE_PTR_CONSTEXPR value_ptr( std::nullptr_t = nullptr )	// constexpr here yields c2476 on msvc15
				: value_ptr( nullptr, deleter_type(), copier_type() )
			{}
			
			// get pointer
			pointer get() { return this->_data.get(); }

			// get const pointer
			const_pointer get() const { return this->_data.get(); }

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
			pointer release() noexcept {
				return this->_data.release();
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

			deleter_type& get_deleter() { return this->_data.get_deleter(); }
			const deleter_type& get_deleter() const { return this->_data.get_deleter(); }

			copier_type& get_copier() { return this->_data.get_copier(); }
			const copier_type& get_copier() const { return this->_data.get_copier(); }

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

	template <typename T, typename Deleter>
	static inline auto make_value_ptr( T* ptr, Deleter&& dx ) -> value_ptr<T, Deleter> {
		return value_ptr<T, Deleter>( ptr, std::forward<Deleter>( dx ) );
	}	// make_value_ptr

	template <typename T, typename Deleter, typename Copier>
	static inline auto make_value_ptr( T* ptr, Deleter&& dx, Copier&& cx ) -> value_ptr<T, Deleter, Copier> {
		return value_ptr<T, Deleter, Copier>( ptr, std::forward<Deleter>( dx ), std::forward<Copier>( cx ) );
	}	// make_value_ptr

}	// smart_ptr ns

#undef VALUE_PTR_CONSTEXPR

#endif // !SMART_PTR_VALUE_PTR


#ifndef _SMART_PTR_VALUE_PTR_
#define _SMART_PTR_VALUE_PTR_

#include <memory>
#include <functional>
#include <cassert>
#include <stdexcept>	// runtime_error

namespace smart_ptr {

	namespace detail {

		// void_t for c++11
		//	from http://en.cppreference.com/w/cpp/types/void_t
		template<typename... Ts> struct make_void { typedef void type; };
		template<typename... Ts> using void_t = typename make_void<Ts...>::type;

		// is_derived_from<Base, Derived>
		//	check Base != Derived && Derived is derived from Base; polymorphic derived not required 
		template <class Base, class Derived>
		struct is_derived_from :
			std::conditional<
			!std::is_same<Base, Derived>::value && std::is_base_of<Base, Derived>::value
			, std::true_type
			, std::false_type
			>::type
		{};

		// is_defined<T>, from https://stackoverflow.com/a/39816909/882436
		template <class, class = void> struct is_defined : std::false_type { };
		template <class T> struct is_defined<
			T
			, typename std::enable_if<std::is_object<T>::value && !std::is_pointer<T>::value && ( sizeof( T ) > 0 )>::type
			>
			: std::true_type{}
		;

		// Class function/type detection
		//	https://stackoverflow.com/a/30848101/882436

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

		// return_type, from https://stackoverflow.com/a/22863957/882436
		template <typename T> struct return_type : return_type<decltype( &T::operator() )> {};

		// For generic types, directly use the result of the signature of its 'operator()'
		template <typename T, typename R, typename... Args> struct return_type<R( T::* )( Args... ) const> { using type = R; };

		// Returns flag if test passes (false==slicing is probable)
		// T==base pointer, U==derived/supplied pointer
		template <typename T, typename U, bool IsDefaultCopier>
		struct slice_test : std::conditional<
			std::is_same<T, U>::value	// if U==T, no need to check for slicing
			|| std::is_same<nullptr_t, U>::value	// nullptr is fine
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

			// op_wrapper() = default;

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


		template <typename T, typename Op>
		struct delete_wrapper : public Op {

			using this_type = delete_wrapper<T, Op>;

			// observer callback fn; using fn pointer vs std::function to gain size efficiency
			using observer_fn_type = void( *)( T*, const this_type& );

			observer_fn_type observer_fn;

			delete_wrapper() = default;

			template <typename Op_, typename Fn>
			constexpr delete_wrapper( Op_&& op, Fn&& obs )
				: Op( std::forward<Op_>( op ) )
				, observer_fn( std::forward<Fn>(obs) )
			{}

			/*
			delete_wrapper( delete_wrapper&& ) = default;
			delete_wrapper( const delete_wrapper& ) = default;
			delete_wrapper& operator=( delete_wrapper&& ) = default;

			delete_wrapper& operator=( const delete_wrapper& that ) {
				if ( this == &that )
					return *this;
				*this = delete_wrapper( static_cast<const Op&>( *this ), this->observer_fn );	// call ctor, move assign
				return *this;
			}	// op=
			*/

			// invoked for delete event
			void operator()( T* ptr ) const {
				if ( !ptr )
					return;	//nothing to do

				assert( this->observer_fn != nullptr );

				//	here we want to notify observer of event, with reference to this
				//		pointer cannot be deleted in this context due to T still being undefined
				this->observer_fn( ptr, *this );
			}

			// call to actual deleter, invoked by observer
			void operator()( T* ptr, bool ) const {
				if ( ptr )
					Op::operator()( ptr );
			}

		};	// delete_wrapper

		template <typename T, typename Op>
		struct copy_wrapper : public Op {

			using this_type = copy_wrapper<T, Op>;

			// observer callback fn; using fn pointer vs std::function to gain size efficiency
			using observer_fn_type = T*( *)( const T*, const this_type& );

			observer_fn_type observer_fn;

			copy_wrapper() = default;

			template <typename Op_, typename Fn>
			constexpr copy_wrapper( Op_&& op, Fn&& obs )
				: Op( std::forward<Op_>( op ) )
				, observer_fn( std::forward<Fn>( obs ) )
			{}

			// invoked for copy event
			T* operator()( const T* ptr ) const {
				if ( !ptr )
					return nullptr;	// nothing to to

				assert( this->observer_fn != nullptr );

				//	here we want to notify observer of event, with reference to this
				return this->observer_fn( ptr, *this );
			}

			// call to actual copy operator, invoked by observer
			T* operator()( const T* ptr, bool ) const {
				if ( ptr )
					return Op::operator()( ptr );
				return nullptr;
			}

		};	// copy_wrapper

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


		template <typename T, typename DeleteOp, typename CopyOp
			// , typename Deleter = delete_wrapper<T, DeleteOp>
			, typename Deleter = op_wrapper<T, DeleteOp, void, void( *)( const void*, T* )>
			// , typename Copier = copy_wrapper<T, CopyOp>
			, typename Copier = op_wrapper<T, CopyOp, T*, T*(*)(const void*, const T*)>
		>
		struct ptr_base_undefined {
		private:

		public:
			using _data_type = ptr_data<T, Deleter, Copier>;
			using _pointer = typename _data_type::pointer;
			_data_type _data;

			using pointer = _pointer;

			// default construct for undefined type
			template <typename Dx, typename Cx>
			constexpr ptr_base_undefined( nullptr_t, Dx&& dx, Cx&& cx )
				: _data( nullptr
					, Deleter( std::forward<Dx>( dx ), []( const void*, T* ptr ) { assert( ptr == nullptr ); } )
					, Copier( std::forward<Cx>( cx ), []( const void* op, const T* ptr ) -> T* { assert( ptr == nullptr ); return nullptr; } )
				)
			{}

			template <typename Dx, typename Cx>
			constexpr ptr_base_undefined( pointer px, Dx&& dx, Cx&& cx )
				: _data( px
					/*
					, Deleter( std::forward<Dx>( dx ), []( T* ptr, const Deleter& op ) {
							op( ptr, true );	// call DeleteOp through op
						}
					)
					*/
					, Deleter( std::forward<Dx>( dx ), []( const void* op, T* ptr ) {
							// op( ptr, true );	// call DeleteOp through op
							if ( ptr )
								static_cast<const Deleter*>( op )->op( ptr );
						}
					)
					/*
					, Copier( std::forward<Cx>( cx ), []( const T* ptr, const Copier& op ) {
							return op( ptr, true );	// call CopyOp through op
						} 
					)
					*/
					, Copier( std::forward<Cx>( cx ), []( const void* op, const T* ptr ) -> T* {
							if ( !ptr )
								return nullptr;
							return static_cast<const Copier*>( op )->op( ptr );
						}
					)
				)
			{}

			// const _data_type& data() const { return this->_data; }
			// _data_type& data() { return this->_data; }

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
		, typename Deleter = std::default_delete<T> // typename std::conditional<detail::is_defined<T>::value, default_delete<T>, default_delete_undefined<T>>::type
		, typename Copier = default_copy<T>
		, typename Base = detail::ptr_base_undefined<T, Deleter, Copier>
	>
		struct value_ptr 
			: Base
		{
		private:
			
			using _base_type = Base;
			using _pointer = typename _base_type::pointer;
			using _element_type = T;
			// static constexpr bool _is_defined() { return std::conditional<detail::is_defined<T>::value, std::true_type, std::false_type>::value; }
			// using _is_defined_type = typename std::conditional<detail::is_defined<T>::value, std::true_type, std::false_type>::type;

			void _delete( T* what ) {
				if ( what )
					this->get_deleter()( what );
			}	// _delete
			// _data_type _data;

		public:
			using element_type = _element_type;

			using pointer = _pointer;
			using const_pointer = const pointer;
			
			using reference = typename std::add_lvalue_reference<element_type>::type;
			using const_reference = const reference;

			using deleter_type = Deleter;
			using copier_type = Copier;
			
			// value_ptr() = default;
			
			// construct with pointer, deleter, copier
			template <typename Px>
			constexpr value_ptr( Px px, deleter_type dx, copier_type cx )
				: _base_type( px
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
			value_ptr( Px px, deleter_type dx )	// constexpr here yields c2476 on msvc15
				: value_ptr( px, std::move(dx), copier_type() )
			{}

			// construct with pointer
			template <typename Px>
			value_ptr( Px px ) // constexpr here yields c2476 on msvc15
				: value_ptr( px, deleter_type(), copier_type() )
			{}

			// nullptr_t
			explicit value_ptr( std::nullptr_t =nullptr )	// constexpr here yields c2476 on msvc15
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

				// this->_data.reset( px );
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
	template <class T, class D, class C> bool operator == ( const value_ptr<T, D, C>& x, nullptr_t ) noexcept { return !x; }
	template <class T, class D, class C> bool operator == ( nullptr_t, const value_ptr<T, D, C>& x ) noexcept { return !x; }
	template <class T, class D, class C> bool operator != ( const value_ptr<T, D, C>& x, nullptr_t ) noexcept { return (bool)x; }
	template <class T, class D, class C> bool operator != ( nullptr_t, const value_ptr<T, D, C>& x ) noexcept { return (bool)x; }
	template <class T, class D, class C> bool operator < ( const value_ptr<T, D, C>& x, nullptr_t ) { return std::less<typename value_ptr<T, D, C>::pointer>()( x.get(), nullptr ); }
	template <class T, class D, class C> bool operator<( nullptr_t, const value_ptr<T, D, C>& y ) { return std::less<typename value_ptr<T, D, C>::pointer>()( nullptr, y.get() ); }
	template <class T, class D, class C> bool operator <= ( const value_ptr<T, D, C>& x, nullptr_t ) { return std::less_equal<typename value_ptr<T, D, C>::pointer>()( x.get(), nullptr ); }
	template <class T, class D, class C> bool operator <= ( nullptr_t, const value_ptr<T, D, C>& y ) { return std::less_equal<typename value_ptr<T, D, C>::pointer>()( nullptr, y.get() ); }
	template <class T, class D, class C> bool operator >( const value_ptr<T, D, C>& x, nullptr_t ) { return !( nullptr < x ); }
	template <class T, class D, class C> bool operator > ( nullptr_t, const value_ptr<T, D, C>& y ) { return !( y < nullptr ); }
	template <class T, class D, class C> bool operator >= ( const value_ptr<T, D, C>& x, nullptr_t ) { return !( x < nullptr ); }
	template <class T, class D, class C> bool operator >= ( nullptr_t, const value_ptr<T, D, C>& y ) { return !( nullptr < y ); }

	template <typename T, typename Deleter>
	static inline auto make_value_ptr( T* ptr, Deleter&& dx ) -> value_ptr<T, Deleter> {
		return value_ptr<T, Deleter>( ptr, std::forward<Deleter>( dx ) );
	}	// make_value_ptr

	template <typename T, typename Deleter, typename Copier>
	static inline auto make_value_ptr( T* ptr, Deleter&& dx, Copier&& cx ) -> value_ptr<T, Deleter, Copier> {
		return value_ptr<T, Deleter, Copier>( ptr, std::forward<Deleter>( dx ), std::forward<Copier>( cx ) );
	}	// make_value_ptr

}	// smart_ptr ns
#endif // !_SMART_PTR_VALUE_PTR_


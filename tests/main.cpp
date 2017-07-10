#include <cassert>
#include <iostream>

#ifdef _DEBUG
#ifdef _WIN32
// http://msdn.microsoft.com/en-us/library/e5ewb1h3%28v=VS.90%29.aspx
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif
#endif

#include "../value_ptr.hpp"
#include "test-pimpl.hpp"

// todo:  
//	base ptr type for complete types, so that sizeof(value_ptr<Complete>==sizeof(Complete*))
//	cleanup

namespace {
	using namespace smart_ptr;
	struct A { int foo; };
}

void basic_tests() {
	// User-provided delete functor, stateless
	struct MyDeleter {
		void operator()( A* px ) const {
			delete px;
		};
	};	// MyDeleter

		// User-provided copy functor, stateless
	struct MyCopier {
		A* operator()( const A* px ) const {
			return new A( *px );
		};
	};	// MyCopier

		// stateful structs for size testing
	struct MyDeleterStateful : MyDeleter { int* ptr; };
	struct MyCopierStateful : MyCopier { int* ptr; };

	// static asserts
	/*
	static_assert( sizeof( value_ptr<A> ) == sizeof( A* ), "Size check fail" );
	static_assert( sizeof( value_ptr<A> ) == sizeof( std::unique_ptr<A> ), "Size check fail" );
	static_assert( sizeof( value_ptr<A, MyDeleter> ) == sizeof( std::unique_ptr<A,MyDeleter> ), "Size check fail" );
	static_assert( sizeof( value_ptr<A, MyDeleter, MyCopier> ) == 8, "Size check fail" );
	static_assert( sizeof( value_ptr<A, MyDeleterStateful> ) == sizeof( std::unique_ptr<A, MyDeleterStateful> ), "Size check fail" );
	*/

	// construct, assign value_ptr, basic ops
	{
		value_ptr<A> a{};	// default ctor
		assert( !a );	// bool
		a = value_ptr<A>( new A{ 5 } );	// move assign
		assert( a );	// bool
		assert( a->foo == 5 );	// op->
		value_ptr<A> b( std::move( a ) );	// move construct
		assert( !a );
		assert( b->foo == 5 );
		value_ptr<A> c{ b };	// copy construct
		assert( c->foo == 5 );
		assert( c->foo == b->foo );
		auto d = c;	// copy assign
		c.reset();	// reset c, default param
		assert( !c );
		assert( d->foo == 5 );
		d.reset( new A{ 10 } );	// reset
		assert( d->foo == 10 );
		auto d_ptr = d.release();	// release
		assert( !d );
		assert( d_ptr->foo == 10 );
		delete d_ptr;
	}

	// nullptr_t construct/assign
	{
		value_ptr<A> a{ new A{ 21 } };
		assert( a );
		a = nullptr;	// nullptr assign
		assert( !a );
		value_ptr<A> b{ nullptr };	// nullptr construct
		assert( !b );
		assert( b.get() == nullptr );
	}
}

void operator_tests() {
	value_ptr<A> x{}, y{};
	assert( x == y );
	assert( x == nullptr );
	assert( nullptr == x );
	assert( y == nullptr );
	assert( nullptr == y );
	assert( x >= nullptr );
	assert( nullptr >= x );
	assert( x <= nullptr );
	assert( nullptr <= x );

	x = new A{ 1 };
	assert( x != nullptr );
	assert( nullptr != x );
	assert( x > y );
	assert( x >= y );
	assert( y < x );
	assert( y <= x );

	y.reset( x.get() );
	assert( x == y );	// pointer compare
	y.release();
	y = new A{ 1 };
	assert( x != y );
	const bool expected_less = std::less<A*>()( x.get(), y.get() );	// strict weak order test value for x < y
	assert( ( x < y ) == expected_less );
	assert( ( x <= y ) == expected_less );
	assert( ( x > y ) == !expected_less );
	assert( ( x >= y ) == !expected_less );
}

void copier_tests() {

	// default copy of non-polymorphic object
	{
		value_ptr<A> a{ new A{7} };
		auto b = a;	// do copy
		assert( a->foo == 7 );
		assert( b->foo == 7 );
	}

	{
		static bool copier_called = false;
		struct MyCopierTest {
			int baz;
			A* operator()( const A* ptr ) const {
				copier_called = true;
				return new A( *ptr );
			}
		};	// MyCopierTest

		{	// default deleter, user-provided copier
			value_ptr<A, value_ptr<A>::deleter_type, MyCopierTest> p{ new A{ 5 },{},{ 2 } };
			assert( p.get_copier().baz == 2 );	// check state
			auto p2 = p;	// do copy
			assert( copier_called );
			assert( p2->foo == 5 );
			assert( p2.get_copier().baz == 2 );

			// swap with p
			value_ptr<A, value_ptr<A>::deleter_type, MyCopierTest> other{ new A{ 7 },{},{ 10 } };
			other.swap( p );
			assert( other->foo == 5 );
			assert( other.get_copier().baz == 2 );
			assert( p->foo == 7 );
			assert( p.get_copier().baz == 10 );
		}

		// default construct with default deleter, MyCopierTest
		value_ptr<A, value_ptr<A>::deleter_type, MyCopierTest> default_{};
		assert( !default_ );
	}
}

void lambda_copier_tests() {

	// stateless lambda copier
	{
		static bool copier_called = false;

		auto lambda_copier = []( const A* ptr ) -> A* { copier_called = true; return new A( *ptr ); };
		value_ptr<A, std::default_delete<A>, decltype( lambda_copier )> p(
			new A{ 33 }
			, {}	// deleter
			, lambda_copier
		);
		assert( p->foo == 33 );
		auto p2 = p;	// execute copy
		assert( copier_called );
		assert( p2->foo == 33 );
	}

	// stateful lambda copier
	{
		int counter = 0;
		const auto lambda_copier = [&counter]( const A* ptr ) -> A* { counter++; return new A( *ptr ); };
		value_ptr<A, std::default_delete<A>, decltype( lambda_copier )> p( new A{ 33 }, {}, lambda_copier );
		assert( p->foo == 33 );
		auto p2 = p;	// execute copy
		assert( counter == 1 );	// check state
		assert( p2->foo == 33 );
	}

	{	// stateless inline lambda copier with make_value_ptr
		auto p = make_value_ptr( new A{ 5 }
			, std::default_delete<A>()	// deleter
			, []( const A* ptr ) -> A* { auto result = new A( *ptr ); result->foo++; return result; }	// copy, increment by one
		);
		assert( p->foo == 5 );
		auto p2 = p;		// execute ocpy
		assert( p2->foo == p->foo + 1 );	// check for increment to verify
	}

	// stateful lambda copier with make_value_ptr
	{
		int counter = 0;
		auto p = make_value_ptr( new A{ 33 }, std::default_delete<A>(), [&counter]( const A* ptr ) { ++counter; return new A( *ptr ); } );
		assert( p->foo == 33 );
		auto p2 = p;	// execute copy
		assert( counter == 1 );
		assert( p2->foo == 33 );
	}
}

void undefined_tests() {

	// basic undefined class
	{
		struct U;	// undefined
		value_ptr<U> u{};
		assert( !u );
		auto u2 = u;	// copy with undefined
		assert( !u2 );
	}

	// pimpl example using test-pimpl
	{
		widget w{};
		assert( w.pImpl );	// validate construction of undefined
		assert( w.pImpl_derived );
		assert( w.get_meaning_of_life() == 42 );
		assert( w.get_meaning_of_life_derived() == 420 );
		assert( w.pImpl_custom.get_copier().counter == 0 );	// no copies done

		auto w2 = w;	// copy undefined
		assert( w.pImpl_custom.get_copier().counter == 1 );	// check custom copier count of w.  todo:  custom deleter count

		assert( w2.get_meaning_of_life() == 42 );
		assert( w2.get_meaning_of_life_derived() == 420 );
		assert( w2.is_clone_derived() );	// derived should have been cloned via auto-detect

		auto w3 = std::move( w2 );
		assert( !w2.pImpl );	// should have been moved
		assert( !w2.pImpl_derived );
		assert( w3.pImpl );	// should be here
		assert( w3.pImpl_derived );

		assert( w3.get_meaning_of_life() == 42 );
		assert( w3.get_meaning_of_life_derived() == 420 );
		assert( w3.is_clone_derived() );	// state should have been carried over from w2
	}

}	// undefined_tests

void deleter_tests() {

	{	// default deleter, copier
		value_ptr<A> p{ new A{ 5 } };
		assert( p->foo == 5 );
	}

	{	// user provided deleter, default copier
		static bool deleter_called = false;
		struct MyDeleterTest {
			int bar;
			void operator()( A* ptr ) const { deleter_called = true; delete ptr; }
		};
		{
			value_ptr<A, MyDeleterTest> p{ new A{ 5 },{ 2 } };	// construct with value for dtor
			assert( p.get_deleter().bar == 2 );	// validate dtor value

														// member swap
			value_ptr<A, MyDeleterTest> other{ new A{ 7 },{ 10 } };
			other.swap( p );
			assert( other->foo == 5 );
			assert( other.get_deleter().bar == 2 );
			assert( p->foo == 7 );
			assert( p.get_deleter().bar == 10 );

			// non-member swap
			swap( other, p );
			assert( p->foo == 5 );
			assert( p.get_deleter().bar == 2 );
			assert( other->foo == 7 );
			assert( other.get_deleter().bar == 10 );
		}
		assert( deleter_called );

		// default construct with MyDeleterTest
		value_ptr<A, MyDeleterTest> default_{};
		assert( !default_ );


	}
}

void lambda_deleter_tests() {

	// stateless lambda deleter
	{
		static bool deleter_called = false;
		{
			auto lambda_deleter = []( A* ptr ) { delete ptr; deleter_called = true; };
			value_ptr<A, decltype( lambda_deleter )> p( new A{ 33 }, lambda_deleter );
			assert( p->foo == 33 );
			auto p2 = p;	// test copy of deleter lambda
			assert( p2->foo == 33 );
		}
		assert( deleter_called );
	}

	// stateful lambda deleter
	{
		int counter = 0;
		{
		auto lambda_deleter = [&counter]( A* ptr ) { delete ptr; ++counter; };
		value_ptr<A, decltype( lambda_deleter )> p( new A{ 33 }, lambda_deleter );
		assert( p->foo == 33 );
		auto p2 = p;	// test copy of deleter lambda
		assert( p2->foo == 33 );
		}
		assert( counter==2 );	// two deletes, p & p2
		
	}

	// stateful lambda deleter with make_value_ptr
	{
		
		int counter = 0;
		{
		auto p = make_value_ptr( new A{ 33 }, [&counter]( A* ptr ) { delete ptr; ++counter; } );
		assert( p->foo == 33 );
		auto p2 = p;	// test copy of deleter lambda
		assert( p2->foo == 33 );
		}
		assert( counter == 2 );	// two deletes, p & p2	
	}

	{	// stateless lambda deleter with make_value_ptr
		auto p = make_value_ptr( new A{ 5 }, []( A* ptr ) { delete ptr; } );
		assert( p->foo == 5 );
		auto p2 = p;		// test copy of deleter lambda
		assert( p2->foo == 5 );
	}
}

void clone_tests() {

	// base struct with clone member
	struct Base {
		int foo;
		Base( int foo_ ) : foo( foo_ ) {}
		virtual Base* clone() const { return new Base( *this ); }
		virtual ~Base() = default;
	};	// Base

		// Derived from base, clone member
	struct Derived : Base {
		int bar;
		Derived( int foo_, int bar_ ) : Base( foo_ ), bar( bar_ ) {}
		Base* clone() const final { return new Derived( *this ); }
	};	// Derived


	// clone auto detect
	{
		value_ptr<Base> a = new Derived( 1, 2 );
		assert( a->foo == 1 );
		assert( static_cast<Derived*>( a.get() )->bar == 2 );
	}

	// clone functor
	{
		static bool MyClone_called = false;
		struct MyClone {
			Base* operator()( const Base* what ) const {
				MyClone_called = true;
				return what->clone();
			}
		};
		value_ptr<Base, std::default_delete<Base>, MyClone> a = new Derived( 1, 2 );
		auto a2 = a;	// trigger copy
		assert( a2->foo == 1 );
		assert( static_cast<Derived*>( a2.get() )->bar == 2 );
		assert( MyClone_called );
	}
}

void slice_protection() {

	// base struct with no clone member function
	struct Base {
		int foo;
		Base( int foo_ ) : foo( foo_ ) {}
		virtual ~Base() = default;
	};	// Base

	// Derived from base
	struct Derived : Base {
		int bar;
		Derived( int foo_, int bar_ ) : Base( foo_ ), bar( bar_ ) {}
	};	// Derived

	value_ptr<Base> b{new Base(3)};	// fine
	b = nullptr;	// fine
	
	// b = new Derived( 1, 2 );	// expected: compilation failure
	// b.reset( new Derived( 1, 2 ) );	// expected: compilation failure
	// value_ptr<Base> a{ new Derived(1,2) };	// expected: compilation failure
}

int main() {

#ifdef _WIN32
	// set up leak checking for msvc
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );	// dump memory leaks at termination
	_CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_DEBUG );
	_CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_DEBUG );
#endif

	basic_tests();
	operator_tests();
	copier_tests();
	lambda_copier_tests();
	deleter_tests();
	lambda_deleter_tests();

	clone_tests();
	slice_protection();
	undefined_tests();
	
	

	std::cout << "All tests passed";
	return 0;
}
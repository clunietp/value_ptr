#include "test-pimpl.hpp"

// define the foo
struct incomplete_foo {
	int val;
};

// should still be considered 'incomplete' here so that we don't get linker errors for value_ptr<incomplete_foo>&
static_assert(smart_ptr::detail::is_incomplete<incomplete_foo>::value, "Incomplete type fail");

// use/check the incomplete foo
bool use_incomplete_foo(smart_ptr::value_ptr<incomplete_foo>& foo, int expected ) {
	
	// if !foo, create with 'expected', else check value vs expected
	if (!foo) {
		foo.reset(new incomplete_foo());
		foo->val = expected;
	}
	else
		return foo->val == expected;
	
	return true;
}

// define pimpl impl
struct widget::impl {
	int* val;

	impl( int val_ )
		: val( new int(val_) )
	{}

	// define copy & move ctors so we don't get double delete on dtor
	impl( const impl& that )
		:val( new int( *that.val ) )
	{}

	impl( impl&& that )
		:val( new int( *that.val ) )
	{}

	virtual impl* clone() const {
		return new impl( *this->val );
	}
	
	virtual int meaning_of_life() const { return *val; }

	virtual ~impl() {
		delete val;
	}
};

// define pimpl impl derived
struct impl_derived
	: widget::impl
{
	int factor;
	bool is_clone;

	impl_derived( int val_, int factor_, bool is_clone_ = false )
		: impl( val_ )
		, factor(factor_)
		, is_clone(is_clone_)
	{}

	int meaning_of_life() const final { return impl::meaning_of_life() * this->factor; }

	impl* clone() const final {
		return new impl_derived( *this->val, this->factor, true );
	}
};

void widget::impl_deleter::operator()( impl* ptr ) const {
	++this->counter;
	delete ptr;
}

widget::impl* widget::impl_copier::operator()( const impl* ptr ) const {
	++this->counter;
	return new impl( *ptr );
}

// pimpl widget ctor
widget::widget()
	: pImpl{ new widget::impl{ 42 } }
	, pImpl_derived{ new impl_derived{ 42, 10 } }
	, pImpl_custom{ nullptr }
{
	this->pImpl_custom.reset( new widget::impl{ 33 } );	// test reset method
}

// pimpl widget method
int widget::get_meaning_of_life() const {
	return this->pImpl->meaning_of_life();
}

// pimpl widget method
int widget::get_meaning_of_life_derived() const {
	return this->pImpl_derived->meaning_of_life();
}

bool widget::is_clone_derived() const { return static_cast<const impl_derived&>( *this->pImpl_derived ).is_clone; }

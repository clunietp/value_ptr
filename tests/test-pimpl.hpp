#ifndef TEST_PIMPL
#define TEST_PIMPL

#include "../value_ptr_incomplete.hpp"

class widget {
public:
	widget();

	struct impl;
	smart_ptr::value_ptr_incomplete<impl> pImpl;
	smart_ptr::value_ptr_incomplete<impl> pImpl_derived;

	int get_meaning_of_life() const;

	int get_meaning_of_life_derived() const;
	bool is_clone_derived() const;

	// custom deleter and copier
	struct impl_deleter {
		mutable int counter = 0;
		void operator()( impl* ) const;
	};

	struct impl_copier {
		mutable int counter = 0;
		impl* operator()( const impl* ) const;
	};

	smart_ptr::value_ptr<impl, impl_deleter, impl_copier> pImpl_custom;
};	// widget

#endif


#ifndef _TEST_PIMPL_
#define _TEST_PIMPL_

#include "../value_ptr.hpp"

class widget {
public:
	widget();
	int get_meaning_of_life() const;

	int get_meaning_of_life_derived() const;
	bool is_clone_derived() const;

	struct impl;
	smart_ptr::value_ptr<impl> pImpl;
	smart_ptr::value_ptr<impl> pImpl_derived;

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
};

#endif


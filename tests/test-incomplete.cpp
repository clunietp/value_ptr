#include "test-incomplete.hpp"

// define the foo
struct incomplete_foo {
	int val;
};

// should still be considered 'incomplete' here so that we don't get linker errors for value_ptr<incomplete_foo>&
// static_assert( smart_ptr::detail::is_incomplete<incomplete_foo>::value, "Incomplete type fail");

// use/check the incomplete foo
bool use_incomplete_foo(smart_ptr::value_ptr_incomplete<incomplete_foo>& foo, int expected) {

	// if !foo, create with 'expected', else check value vs expected
	if (!foo) {
		foo.reset(new incomplete_foo());
		foo->val = expected;
	}
	else
		return foo->val == expected;

	return true;
}


struct Widget {
	int i;
	int j;
	int k;
};

// using ty = decltype(wrapped<Widget>::m);
// static_assert( ty::_incomplete_test , "complete");

int get_wrapped_count(const wrapped<Widget>& w) { return w.count; }

void set_widget_i(wrapped<Widget>& w, int val) { 
	if ( !w.m )
		w.m.reset( new Widget() );
	w.m->i = val; 
}

int get_widget_i(const wrapped<Widget>& w) { return w.m->i; }

void reset_widget(wrapped<Widget>& w) { w.m.reset(); }

void swap_widgets(wrapped<Widget>& a, wrapped<Widget>& b) { a.m.swap(b.m); }
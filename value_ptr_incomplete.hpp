
// Copyright 2017-2018 by Tom Clunie
// https://github.com/clunietp/value_ptr
// Distributed under the Boost Software License, Version 1.0.
//    (See http://www.boost.org/LICENSE_1_0.txt)

#ifndef SMART_PTR_VALUE_PTR_INCOMPLETE
#define SMART_PTR_VALUE_PTR_INCOMPLETE

#include "value_ptr.hpp"

namespace smart_ptr {

	namespace detail {
		
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

			// construct with Op, Delegate; delegate must not be null
			template <typename Op_, typename Delegate_>
			constexpr functor_wrapper(Op_&& op, Delegate_&& del)
				: Op(std::forward<Op>(op))
				, delegate_(std::forward<Delegate>(del))
			{}

			// invoked for event, const
			template <typename... Args>
			auto operator()(Args&&... args) const -> typename std::result_of<Op(Args...)>::type {
				return this->delegate_(*this, std::forward<Args>(args)...);	//	call delegate, with reference to this as first parameter
			}

			// invoked for event
			template <typename... Args>
			auto operator()(Args&&... args) -> typename std::result_of<Op(Args...)>::type {
				return this->delegate_(*this, std::forward<Args>(args)...);	//	call delegate, with const reference to this as first parameter
			}

		};	// functor_wrapper

	}	// detail

	template <typename T
		, typename Deleter = std::default_delete<T>
		, typename Copier = detail::default_copy<T>
		, typename DeleterWrapper = detail::functor_wrapper<Deleter, void(*)(const Deleter&, T*)>
		, typename CopierWrapper = detail::functor_wrapper<Copier, T*(*)(const Copier&, const T*)>
	>
		struct value_ptr_incomplete
		: value_ptr<T
		, DeleterWrapper
		, CopierWrapper
		>
	{
		using base_type = value_ptr<T, DeleterWrapper, CopierWrapper>;
		using copier_type = typename base_type::copier_type;
		using deleter_type = typename base_type::deleter_type;
		using pointer = typename base_type::pointer;

		// default construct for incomplete type
		template <typename Dx = Deleter, typename Cx = Copier>
		constexpr value_ptr_incomplete(std::nullptr_t = nullptr, Dx&& dx = {}, Cx&& cx = {})
			: base_type(
				nullptr
				, deleter_type(std::forward<Dx>(dx), [](const Deleter&, T* ptr) { assert(ptr == nullptr); })
				, copier_type(std::forward<Cx>(cx), [](const Copier&, const T* ptr) -> T* { assert(ptr == nullptr); return nullptr; })
			)
		{}

		// construct when incomplete type is known; lambdas in this context will evaluate properly for previously-incomplete types
		template <typename Px, typename Dx = Deleter, typename Cx = Copier, typename = typename std::enable_if<std::is_convertible<Px, pointer>::value>::type>
		constexpr value_ptr_incomplete(Px&& px, Dx&& dx = {}, Cx&& cx = {})
			: base_type(
				std::forward<Px>(px)
				, deleter_type(std::forward<Dx>(dx), [](const Deleter& op, T* ptr) { op(ptr); })
				, copier_type(std::forward<Cx>(cx), [](const Copier& op, const T* ptr) -> T* { return op(ptr); })
			)
		{}

		// reset pointer to compatible type
		template <typename Px, typename = typename std::enable_if<std::is_convertible<Px, pointer>::value>::type>
		void reset(Px px) {
			// hides value_ptr::reset, needed to properly init lambdas via ctor
			static_assert(
				detail::slice_test<pointer, Px, std::is_same<detail::default_copy<T>, Copier>::value>::value
				, "value_ptr; clone() method not detected and not using custom copier; slicing may occur"
				);

			*this = value_ptr_incomplete(std::forward<Px>(px), std::move(this->get_deleter()), std::move(this->get_copier()));
		}

		// reset pointer
		void reset() { this->reset(nullptr); }

	};	// value_ptr_incomplete

}	// smart_ptr ns

#endif // !SMART_PTR_VALUE_PTR_INCOMPLETE


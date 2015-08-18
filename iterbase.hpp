#ifndef ITERBASE_HPP_
#define ITERBASE_HPP_


// This file consists of utilities used for the generic nature of the
// iterable wrapper classes.  As such, the contents of this file should be
// considered UNDOCUMENTED and is subject to change without warning.  This
// also applies to the name of the file.  No user code should include
// this file directly.

#include <utility>
#include <tuple>
#include <iterator>
#include <functional>
#include <memory>
#include <type_traits>
#include <cstddef>

namespace iter {
    template <typename T>
    struct type_is {
        using type = T;
    };

    // gcc CWG 1558
    template <typename...>
    struct void_t_help {
        using type = void;
    };
    template <typename... Ts>
    using void_t = typename void_t_help<Ts...>::type;


    // iterator_type<C> is the type of C's iterator
    template <typename Container>
    using iterator_type =
        decltype(std::begin(std::declval<Container&>()));

    // iterator_deref<C> is the type obtained by dereferencing an iterator
    // to an object of type C
    template <typename Container>
    using iterator_deref =
        decltype(*std::declval<iterator_type<Container>&>());

    // const_iteator_deref is the type obtained through dereferencing
    // a const iterator& (note: not a const_iterator).  ie: the result
    // of Container::iterator::operator*() const
    template <typename Container>
    using const_iterator_deref =
        decltype(*std::declval<const iterator_type<Container>&>());

    template <typename Container>
    using iterator_traits_deref =
        std::remove_reference_t<iterator_deref<Container>>;

    namespace detail {
    template <typename T, typename =void>
    struct ArrowHelper {
        using type = void;
    };

    template <typename T>
    struct ArrowHelper<T*, void> {
        using type = T*;
        constexpr type operator()(T* t) const noexcept {
            return t;
        }
    };


    template <typename T>
    struct ArrowHelper<T, void_t<decltype(std::declval<T&>().operator->())>> {
        using type = decltype(std::declval<T&>().operator->());
        type operator()(T& t) const {
            return t.operator->();
        }
    };

    template <typename T>
    using arrow = typename detail::ArrowHelper<T>::type;

    }

    // type of C::iterator::operator->, also works with pointers
    // void if the iterator has no operator->
    template <typename C>
    using iterator_arrow = detail::arrow<iterator_type<C>>;

    // applys the -> operator to an object, if the object is a pointer,
    // it returns the pointer
    template <typename T>
    detail::arrow<T> apply_arrow(T& t) {
        return detail::ArrowHelper<T>{}(t);
    }

    // For iterators that have an operator* which returns a value
    // they can return this type from their operator-> instead, which will
    // wrap an object and allow it to be used with arrow
    template <typename T>
    class ArrowProxy {
        private:
            using TPlain = typename std::remove_reference<T>::type;
            T obj;
        public:
            constexpr ArrowProxy(T&& in_obj)
                : obj(std::forward<T>(in_obj))
            { }

            TPlain *operator->() {
                return &obj;
            }
    };


    template <typename, typename =void>
    struct is_random_access_iter : std::false_type { };

    template <typename T>
    struct is_random_access_iter<T,
        std::enable_if_t<
             std::is_same<
                 typename std::iterator_traits<T>::iterator_category,
              std::random_access_iterator_tag>::value
          >> : std::true_type { };

    template <typename T>
    using has_random_access_iter = is_random_access_iter<iterator_type<T>>;
    // because std::advance assumes a lot and is actually smart, I need a dumb

    // version that will work with most things
    template <typename InputIt, typename Distance =std::size_t>
    void dumb_advance(InputIt& iter, Distance distance=1) {
        for (Distance i(0); i < distance; ++i) {
            ++iter;
        }
    }

    template <typename Iter, typename Distance>
    void dumb_advance_impl(Iter& iter, const Iter& end,
            Distance distance, std::false_type) {
        for (Distance i(0); i < distance && iter != end; ++i) {
            ++iter;
        }
    }

    template <typename Iter, typename Distance>
    void dumb_advance_impl(Iter& iter, const Iter& end,
            Distance distance, std::true_type) {
        if (static_cast<Distance>(end - iter) < distance) {
            iter = end;
        } else {
            iter += distance;
        }
    }

    // iter will not be incremented past end
    template <typename Iter, typename Distance =std::size_t>
    void dumb_advance(Iter& iter, const Iter& end, Distance distance=1) {
        dumb_advance_impl(iter, end, distance, is_random_access_iter<Iter>{});
    }

    template <typename ForwardIt, typename Distance =std::size_t>
    ForwardIt dumb_next(ForwardIt it, Distance distance=1) {
        dumb_advance(it, distance);
        return it;
    }

    template <typename ForwardIt, typename Distance =std::size_t>
    ForwardIt dumb_next(
            ForwardIt it, const ForwardIt& end, Distance distance=1) {
        dumb_advance(it, end, distance);
        return it;
    }

    template <typename Container, typename Distance =std::size_t>
    Distance dumb_size(Container&& container) {
        Distance d{0};
        for (auto it = std::begin(container), end = std::end(container);
                it != end;
                ++it) {
            ++d;
        }
        return d;
    }


    template <typename... Ts>
    struct are_same : std::true_type { };

    template <typename T, typename U, typename... Ts>
    struct are_same<T, U, Ts...>
        : std::integral_constant<bool,
            std::is_same<T, U>::value && are_same<T, Ts...>::value> { };

    namespace detail {
        template <typename... Ts>
        std::tuple<iterator_type<Ts>...> iterator_tuple_type_helper(
                const std::tuple<Ts...>&);
    }
    // Given a tuple template argument, evaluates to a tuple of iterators
    // for the template argument's contained types.
    template <typename TupleType>
    using iterator_tuple_type =
        decltype(detail::iterator_tuple_type_helper(
                    std::declval<TupleType>()));

    namespace detail {
        template <typename... Ts>
        std::tuple<iterator_deref<Ts>...> iterator_tuple_deref_helper(
                const std::tuple<Ts...>&);
    }

    // Given a tuple template argument, evaluates to a tuple of
    // what the iterators for the template argument's contained types
    // dereference to
    template <typename TupleType>
    using iterator_deref_tuple =
        decltype(detail::iterator_tuple_deref_helper(
                    std::declval<TupleType>()));

    // ---- Tuple utilities ---- //
    
    // function absorbing all arguments passed to it. used when
    // applying a function to a parameter pack but not passing the evaluated
    // results anywhere
    template <typename... Ts>
    void absorb(Ts&&...) { }

    namespace detail {
        template <typename Func, typename TupleType, std::size_t... Is>
        decltype(auto) call_with_tuple_impl(Func&& mf, TupleType&& tup,
                std::index_sequence<Is...>) {
            return mf(std::forward<
                        std::tuple_element_t<
                            Is, std::remove_reference_t<TupleType>>
                        >(std::get<Is>(tup))...);
        }
    }

    // expand a TupleType into individual arguments when calling a Func
    template <typename Func, typename TupleType>
    decltype(auto) call_with_tuple(Func&& mf, TupleType&& tup) {
        constexpr auto TUP_SIZE = std::tuple_size<
            std::decay_t<TupleType>>::value;
        return detail::call_with_tuple_impl(
                std::forward<Func>(mf),
                std::forward<TupleType>(tup),
                std::make_index_sequence<TUP_SIZE>{});
    }

    // DerefHolder holds the value gotten from an iterator dereference
    // if the iterate dereferences to an lvalue references, a pointer to the
    //     element is stored
    // if it does not, a value is stored instead
    // get() returns a reference to the held item
    // get_ptr() returns a pointer to the held item
    // reset() replaces the currently held item

    template <typename T, typename =void>
    class DerefHolder {
        private:
            static_assert(!std::is_lvalue_reference<T>::value,
                    "Non-lvalue-ref specialization used for lvalue ref type");
            // it could still be an rvalue reference
            using TPlain = std::remove_reference_t<T>;

            std::unique_ptr<TPlain> item_p;

        public:
            using reference = TPlain&;
            using pointer = TPlain*;

            DerefHolder() = default;

            DerefHolder(const DerefHolder& other)
                : item_p{other.item_p ?
                  std::make_unique<TPlain>(*other.item_p) : nullptr}
            { }

            DerefHolder& operator=(const DerefHolder& other) {
                this->item_p = other.item_p ?
                        std::make_unique<TPlain>(*other.item_p) : nullptr;
                return *this;
            }

            DerefHolder(DerefHolder&&) = default;
            DerefHolder& operator=(DerefHolder&&) = default;
            ~DerefHolder() = default;

            reference get() {
                return *this->item_p;
            }

            pointer get_ptr() {
                return this->item_p.get();
            }

            void reset(T&& item) {
                item_p = std::make_unique<TPlain>(std::move(item));
            }

            explicit operator bool() const {
                return this->item_p;
            }
    };


    // Specialization for when T is an lvalue ref.  Keep this in mind
    // wherever a T appears.
    template <typename T>
    class DerefHolder<T, std::enable_if_t<std::is_lvalue_reference<T>::value>>
    {
        public:
            using reference = T;
            using pointer = std::remove_reference_t<T>*;

        private:
            pointer item_p{};
            
        public:
            DerefHolder() = default;

            reference get() {
                return *this->item_p;
            }

            pointer get_ptr() {
                return this->item_p;
            }

            void reset(T item) {
                this->item_p = &item;
            }

            explicit operator bool() const {
                return this->item_p != nullptr;
            }
    };

}

#endif

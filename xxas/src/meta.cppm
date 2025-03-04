export module xxas: meta;

import std;
import :format;

namespace xxas
{   // Template meta-programming details.
    export namespace meta
    {   // Variadic std::same_as wrapper.
        template<class T, class... Ups> concept same_as = (std::same_as<T, Ups> || ...);

        // std::is_arithmetic_v wrapper concept.
        template<class T> concept arithmetic = std::is_arithmetic_v<T>;

        // Overloads the operator() with specializes provided by `Ts...`.
        template<class... Ts> struct Overloads: Ts...
        {
            using Ts::operator()...;
        };

        template<auto V, class T = decltype(V)> struct Value
        {
            using ValueType = decltype(V);
            constexpr auto value() const
                -> ValueType
            {
                return V;
            };
        };

        // Underlying type identity of `T`.
        template<class T> struct TypeIdentity
        {
            using Type = T;
        };

        // Simple template placeholder.
        struct Placeholder{};

        // Container of types.
        template<bool Is, class... Ts> struct Container
        {
            using ContainerTuple = std::tuple<Ts...>;

            constexpr auto size() const noexcept
                -> std::size_t
            {
                return sizeof...(Ts);
            };

            // Template is variadic.
            constexpr auto is_container() const noexcept
                -> bool
            {
                return Is;
            };

            // Returns the element type at `Index`.
            template<std::integral auto Index> constexpr auto type_at(Value<Index>) const noexcept
            {
                static_assert(sizeof...(Ts) > Index, "Index out of bounds");
                return TypeIdentity<Ts...[Index]>{};
            };
        };

        template<class... Ts> struct Template
            : TypeIdentity<Placeholder>, Container<false, Ts...>
        {
            using Container<false, Ts...>::size;
            using Container<false, Ts...>::is_container;
        };

        template<class T> struct Template<T>
          : TypeIdentity<T>, Container<false>
        {
            using Container<false>::is_container;
        };

        template<template<class...> class C, class... Ts> struct Template<C<Ts...>>
            : TypeIdentity<C<Ts...>>, Container<true, Ts...>
        {
            template<class... Es> using Extend = Template<C<Ts..., Es...>>;

            using Container<true, Ts...>::size;
            using Container<true, Ts...>::is_container;

            // Provided an empty container, returns the current template.
            constexpr auto dedup_extend(Template<C<>>) const noexcept
            {
                return *this;
            };

            constexpr auto dedup_extend(Template<>) const noexcept
            {
                return *this;
            };

            // If `E` is not a duplicate type, returns an extended template for `Container<Ts...>`.
            template<class E> constexpr auto dedup_extend(Template<E>) const noexcept
            {
                return std::conditional_t<meta::same_as<E, Ts...>, Template<C<Ts...>>, Extend<E>>{};
            };

            // Recursively extends the template types for `Container<Ts...>` with types from `EContainer<E, Es...>` that are not duplicates.
            template<template<class...> class OC, class E, class... Es> constexpr auto dedup_extend(Template<OC<E, Es...>>) const noexcept
                requires(meta::same_as<OC<E, Es...>, C<E, Es...>, Template<E, Es...>>)
            {
                return this->dedup_extend(Template<E>{}).dedup_extend(Template<Es...>{});
            };

            template<class... Es> constexpr auto operator==(Template<Es...>) const noexcept
                -> bool
            {
                return std::same_as<Template<C<Ts...>>, Template<Es...>>;
            };
        };

        // 
        template<class T> concept container = Template<T>{}.is_container();

        // Extends the template types of container `T` by the template types of container `U`.
        template<container T, class U> using DedupExtend   = decltype(Template<T>{}.dedup_extend(Template<U>{}));
        template<container T, class U> using DedupExtend_t = typename DedupExtend<T, U>::Type;

        template<container T, auto I> using Element   = decltype(Template<T>{}.type_at(Value<I>{}));
        template<container T, auto I> using Element_t = typename Element<T, I>::Type;
    };
};

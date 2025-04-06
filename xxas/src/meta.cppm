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

        // std::is_enum_v wrapper concept.
        template<class T> concept enumerable = std::is_enum_v<T>;

        // Overloads the operator() with specializes provided by `Ts...`.
        template<class... Ts> struct Overloads: Ts...
        {
            using Ts::operator()...;
        };

        template<auto V, class T = decltype(V)> struct Value
        {
            using ValueType = T;
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
        template<auto Index = 0uz> struct Placeholder{};

        // Container traits.
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

            // Returns the index of a type if found within the container.
            template<class E, std::integral auto Index = 0uz> constexpr auto index_of(TypeIdentity<E>) const noexcept
                -> std::size_t
            {
                static_assert(sizeof...(Ts) > Index, "Index out of bounds");
                return std::same_as<E, Ts...[Index]> ? Index : this->index_of<E, Index + 1>(TypeIdentity<E>{});
            };
        };

        template<class... Ts> struct Forwarding
        {
            template<class... Es> constexpr auto forward_alike(std::tuple<Es...>&& tuple) const noexcept
            {
                return [&tuple]<auto... In>(std::index_sequence<In...>)
                {
                    return std::tuple_cat([&]
                    {
                        if constexpr(meta::same_as<Es...[In], Ts...>)
                        {
                            return std::tuple<Es...[In]>(std::get<In>(tuple));
                        }
                        else
                        {
                            return std::tuple<>{};
                        };
                    }()...);
                }(std::make_index_sequence<sizeof...(Es)>{});
            };
        };

        // Multiple template types meta-programming.
        template<class... Ts> struct Template
            : TypeIdentity<Placeholder<>>, Container<false, Ts...>, Forwarding<Ts...>
        {
            using Container<false, Ts...>::size;
            using Container<false, Ts...>::is_container;
            using Forwarding<Ts...>::forward_alike;

            template<class... Es> constexpr auto operator==(Template<Es...>) const noexcept
                -> bool
            {
                return std::same_as<Template<Ts...>, Template<Es...>>;
            };
        };

        // Single template type meta-programming.
        template<class T> struct Template<T>
            : TypeIdentity<T>, Container<false>, Forwarding<T>
        {
            using Container<false>::is_container;
            using Forwarding<T>::forward_alike;

            template<class... Es> constexpr auto operator==(Template<Es...>) const noexcept
                -> bool
            {
                return std::same_as<Template<T>, Template<Es...>>;
            };
        };

        // Container template meta-programming.
        template<template<class...> class C, class... Ts> struct Template<C<Ts...>>
            : TypeIdentity<C<Ts...>>, Container<true, Ts...>, Forwarding<Ts...>
        {
            template<class... Es> using Extend = Template<C<Ts..., Es...>>;

            using Container<true, Ts...>::size;
            using Container<true, Ts...>::is_container;
            using Forwarding<Ts...>::forward_alike;

            // Provided an empty type, returns the current template.
            template<class E> constexpr auto dedup_extend(E) const noexcept
                requires meta::same_as<E, Template<>, Template<C<>>>
            {
                return *this;
            };

            // If `E` is not a duplicate type, returns an extended template for `C<Ts..., E>`, otherwise returns `C<Ts...>`.
            template<class E> constexpr auto dedup_extend(Template<E>) const noexcept
            {
                return std::conditional_t<meta::same_as<E, Ts...>, Template<C<Ts...>>, Extend<E>>{};
            };

            // Recursively extends the template types for `C<Ts...>` with types from `OC<E, Es...>` that are not duplicates.
            template<template<class...> class OC, class E, class... Es> constexpr auto dedup_extend(Template<OC<E, Es...>>) const noexcept
                requires meta::same_as<OC<E, Es...>, C<E, Es...>, Template<E, Es...>>
            {
                return this->dedup_extend(Template<E>{}).dedup_extend(Template<Es...>{});
            };

            template<class... Es> constexpr auto operator==(Template<Es...>) const noexcept
                -> bool
            {
                return std::same_as<Template<C<Ts...>>, Template<Es...>>;
            };
        };

        // Only forwards elements of `tuple` that are alike `Ts...`.
        template<class... Ts> constexpr auto forward_alike(auto&& tuple)
        {
            return Template<Ts...>{}.forward_alike(std::forward<decltype(tuple)>(tuple));
        };

        // Applies `funct` to the elements of `tuple` that are alike `Ts...`.
        template<class... Ts> constexpr auto apply_alike(auto&& funct, auto&& tuple)
        {
            return std::apply(std::forward<decltype(funct)>(funct), meta::forward_alike<Ts...>(std::forward<decltype(tuple)>(tuple)));
        };

        // Defines a concept that describes a type that include a variadic template of hetereogenous types.
        template<class T> concept container = Template<T>{}.is_container();

        // Extends the template types of container `T` by the template types of container `U`.
        template<container T, class... U> using DedupExtend   = decltype(Template<T>{}.dedup_extend(Template<U...>{}));
        template<container T, class... U> using DedupExtend_t = typename DedupExtend<T, U...>::Type;

        // Index access to a specific element in a template/container.
        template<container T, auto I> using Element   = decltype(Template<T>{}.type_at(Value<I>{}));
        template<container T, auto I> using Element_t = typename Element<T, I>::Type;

        // Returns the index of a type within a template container.
        template<container T, class U, std::integral auto In = 0uz> constexpr inline std::size_t index_of = Template<T>{}.index_of<U, In>();
    };
};

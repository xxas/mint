export module xxas: meta;

import std;

namespace xxas
{
    export namespace meta
    {
        template<class T, class F> struct DrainOnce;
        template<class... Ts, class F, class... Fs> struct DrainOnce<std::variant<Ts...>, std::variant<F, Fs...>>
        {
            constexpr static bool Contains_type = (std::same_as<F, Ts> || ...);
            using After = std::variant<Fs...>;
            using Type  = typename std::conditional_t<Contains_type, DrainOnce<std::variant<Ts...>, After>,
                                    DrainOnce<std::variant<F, Ts...>, After>>::Type;
        };

        template<class... Ts, class F> struct DrainOnce<std::variant<Ts...>, std::variant<F>>
        {
            constexpr static bool Contains_type = (std::same_as<F, Ts> || ...);
            using Type = std::conditional_t<Contains_type, std::variant<Ts...>, std::variant<F, Ts...>>;
        };

        template<class T, class F> using DrainOnce_t = typename DrainOnce<T, F>::Type;
 
        template<class T, class... Fs> struct DrainAll;
        template<class T, class F, class... Fs> struct DrainAll<T, F, Fs...>
        {
            using Once = DrainOnce_t<T, F>;
            using Type = typename DrainAll<Once, Fs...>::Type;
        };

        template<class T> struct DrainAll<T>
        {
            using Type = T;
        };

        template<class T, class... Fs> using DrainAll_t = typename DrainAll<T, Fs...>::Type;
    
        template<class... Ts> struct Overloads: Ts...
        {
            using Ts::operator()...;
        };
    };
};

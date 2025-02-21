export module xxas: error;

import std;

namespace xxas
{   // Meta programming details.
    namespace meta
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
    };
 
    // Simple error, and error propagating container.
    export template<class En = std::uint8_t, class... From> struct Error
    {   // Drain previous possible error types into a single variant.
        using Enum = meta::DrainAll_t<std::variant<En>, typename From::Enum...>;

        Enum        type;
        std::string message;

        // Construct from a previous error.
        template<class E> constexpr Error(E& err) requires(std::same_as<E, From> || ...)
        {
            err.type.visit([&](auto& type)
            {
                this->type = Enum(type);
            });
 
            this->message = err.message;
        };
 
        // Construct from current error type.
        constexpr Error(En t, std::string_view str)
            : type{ t }, message{ str } {};
 
        // Construct from an std::expected error.
        template<class R, class E> constexpr static auto from(const std::expected<R, E>& e)
            -> std::unexpected<Error>
            requires(std::same_as<E, Error> || (std::same_as<E, From> || ...))
        {
            auto err = e.error();
            return std::unexpected(Error(err));
        };
 
        // Returns an std::unexpected error.
        constexpr static auto err(En type, std::string_view view)
        {
            return std::unexpected(Error(type, view));
        };
 
        constexpr static auto err(std::string_view view)
        {
            return std::unexpected(Error(0, view));
        };
    };
};

// mylir::Error formatting.
template<class En, class... From>
struct std::formatter<xxas::Error<En, From...>>
{
    constexpr formatter() = default;
    constexpr formatter(const formatter&) = default;

    constexpr auto parse(std::format_parse_context& ctx) -> decltype(ctx.begin())
    {
        auto it = ctx.begin();

        if (it != ctx.end() && *it != '}')
        {
            throw std::format_error("Invalid format specifier for Error");
        };

        return it;
    };

    template<class Ctx> constexpr auto format(const xxas::Error<En, From...>& error, Ctx& ctx) const
    {
        auto type_str = error.type.visit([](const auto& val)
            -> std::string
        {
            using T = std::remove_cvref_t<decltype(val)>;
 
            if constexpr(std::is_enum_v<T>)
            {
                return std::to_string(static_cast<std::underlying_type_t<T>>(val));
            }
            else
            {
                return std::to_string(val);
            };
        });

        return std::format_to(ctx.out(), "Error {{ type: {}, message: {} }}", type_str, error.message);
    };
};

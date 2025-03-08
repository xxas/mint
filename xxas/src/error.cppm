export module xxas: error;

import std;
import :meta;

namespace xxas
{   // Simple error, and error propagating container.
    export template<class Err = std::uint8_t, class... Errs> struct Error
    {   // Drain previous possible error types into a single variant.
        using Enum = meta::DedupExtend_t<std::variant<Err>, Errs...>;

        Enum        type{};
        std::string message{};

        constexpr Error(meta::same_as<Err, Errs...> auto&& type, std::string&& message) noexcept
            : type{std::move(type)}, message{std::move(message)} {};

        constexpr Error(std::string&& message)
            : type{}, message{std::move(message)} {};

        template<class... From> explicit constexpr Error(Error<From...>&& from) noexcept
            : message{std::move(from.message)}
        {
            constexpr auto default_initialize = [](auto&&...) noexcept
            {
                return Enum{};
            };

            constexpr auto move_initialize = [](auto&& type) noexcept
            {
                return std::move(type);
            };

            this->type = from.type.visit(meta::Overloads
            {
                default_initialize, move_initialize,
            });
        };
    };

    export template<class T> auto error(T&& type, std::string&& message)
        -> Error<T>
    {
        return Error<T>{std::move(type), std::move(message)};
    };

    export template<class T, class... Errs> struct Result: std::expected<T, Error<Errs...>>
    {
        using Base  = std::expected<T, Error<Errs...>>;
        using Type  = T;
        using Err   = Error<Errs...>;
        using Base::Base;

        // Construction from std::expected<T, Error>.
        constexpr Result(const Base& other) noexcept : Base(other) {};
        constexpr Result(Base&& other) noexcept      : Base(std::move(other)) {};

        // Construction from alternative error types.
        template<class... From> constexpr Result(const Error<From...>& err) noexcept : Base(std::unexpected(err)) {};
        template<class... From> constexpr Result(Error<From...>&& err) noexcept      : Base(std::unexpected(std::move(err))) {};

        template<std::invocable<T> F> constexpr auto and_then(F&& funct)
            -> Result<std::invoke_result_t<F, T>, Errs...>
        {
            if(!*this)
            {
                return std::unexpected(this->error());
            };


            if constexpr(std::same_as<std::invoke_result_t<F, T>, void>)
            {
                std::invoke(std::forward<F>(funct), this->value());
                return {};
            }
            else
            {
                auto result = std::invoke(std::forward<F>(funct), this->value());
                return result;
            };
        };

        template<std::invocable F> constexpr auto and_then(F&& funct)
            -> Result<void, Errs...>
        {
            if(!*this)
            {
                return std::unexpected(this->error());
            };


            if constexpr(std::same_as<std::invoke_result_t<F>, void>)
            {
                std::invoke(std::forward<F>(funct));
                return {};
            }
            else
            {
                auto result = std::invoke(std::forward<F>(funct));
                return result;
            };
        };
    };
};

// mint::Error formatting.
template<class En, class... From>
struct std::formatter<xxas::Error<En, From...>>
{
    constexpr formatter() = default;
    constexpr formatter(const formatter&) = default;

    constexpr auto parse(std::format_parse_context& ctx)
        -> decltype(ctx.begin())
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

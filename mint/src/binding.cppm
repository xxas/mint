export module mint: binding;

import :memory;
import :cpu;

import :traits;
import :scalar;
import :context;

/*** **
 **
 **  module:   xxas: binding
 **  purpose:  Low-level function bindings from scalar operands.
 **
 *** **/

namespace mint
{
    export struct Binding
    {
        enum class CreateErrs: std::uint8_t
        {
            BytesLen,
        };

        using CreateErr     = xxas::Error<CreateErrs>;
        using CreateResult  = std::expected<Binding, CreateErr>;

        enum class FunctErrs: std::uint8_t
        {
            Invocation,
        };

        using FunctErr      = xxas::Error<FunctErrs>;
        using FunctResult   = std::expected<std::void_t<>, FunctErr>;

        template<class... Args> using FunctionFor = std::function<FunctResult(Args...)>;
        using Function                            = FunctionFor<>;

        Function function;

        template<class... Args> constexpr auto operator()(Args&&... args) const
            -> FunctResult
        {
            if (!this->function) [[unlikely]]
            {
                return FunctErr::err(FunctErrs::Invocation, "Function is not set");
            };

            return std::invoke(this->function, std::forward<Args>(args)...);
        };

        // Creates a low-level argument bound function for direct execution from a function and a range of byte slices.
        template<class... Args> constexpr static auto create(FunctionFor<Args...>& funct, std::ranges::range auto& spans)
            -> CreateResult
        {   // Not enough byte spans provided to extract arguments from.
            if(spans.size() < sizeof...(Args))
            {
                std::size_t types_size   = (0uz + ... + sizeof(Args));
                auto typename_str        = xxas::format::demangled_typename<std::tuple<Args...>>();

                return CreateErr::err(CreateErrs::BytesLen, std::format("Not enough data ({} bytes) provided for {} ({}bytes)", spans.size(), typename_str, types_size));
            };

            auto check_alignment = [&]<auto... In>(std::index_sequence<In...>)
                -> bool
            {
                return ((reinterpret_cast<std::uintptr_t>(spans.at(In).data()) % alignof(Args) == 0) && ...);
            };

            if (!check_alignment(std::index_sequence_for<Args...>{}))
            {
                return CreateErr::err(CreateErrs::BytesLen, "Memory spans are not properly aligned for argument types.");
            };

            auto get_tuple_refs = [&]<auto... In>(std::index_sequence<In...>)
                -> std::tuple<Args&...>
            {
                return
                {
                    *reinterpret_cast<std::remove_reference_t<Args>*>(spans[In].data())...
                };
            };

            // Reinterpret the provided byte slices as the provided arguments.
            auto arg_refs = get_tuple_refs(std::index_sequence_for<Args...>{});

            return Binding
            {    // Capture the original function as a reference and the arguments moved into the function.
                .function = [funct = std::ref(funct), args = std::move(arg_refs)]()
                    -> FunctResult
                {   // Unpack the arguments and perfectly forward them to the function.
                    return std::apply([&funct](auto&&... unpacked_args)
                    {
                        return std::invoke(funct, std::forward<Args>(unpacked_args)...);
                    }, args);
                },
            };
        };
    };
};

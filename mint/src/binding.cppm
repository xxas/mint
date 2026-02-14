export module mint: binding;

import :memory;
import :cpu;
import :traits;
import :scalar;
import :context;
import :instruction;

/*** **
 **
 **  module:   mint: binding
 **  purpose:  Low-level function bindings from scalar operands.
 **
 *** **/

namespace mint
{
    export struct Binding
    {
        enum class CreateErr: std::uint8_t
        {
            BytesLen,
        };

        using CreateResult  = xxas::Result<Binding, CreateErr>;

        enum class Err: std::uint8_t
        {
            Invocation, Missing
        };

        using Result   = xxas::Result<void, Err>;

        template<class... Args> using FunctionFor = std::function<Result(Args...)>;
        using Function                            = FunctionFor<>;

        Function function;

        template<class... Args> constexpr auto operator()(Args&&... args) const
            -> Result
        {
            if(!this->function) [[unlikely]]
            {
                return xxas::error(Err::Invocation, "Function is not set");
            };

            return std::invoke(this->function, std::forward<Args>(args)...);
        };

        // Creates a low-level argument bound function for direct execution from a function and a range of byte slices.
        template<class... Args> constexpr static auto create(const FunctionFor<Args...>& funct, std::ranges::range auto& spans)
            -> CreateResult
        {   // Not enough byte spans provided to extract arguments from.
            if(spans.size() < sizeof...(Args))
            {
                std::size_t types_size   = (0uz + ... + sizeof(Args));
                auto typename_str        = xxas::format::demangled_typename<std::tuple<Args...>>();

                return xxas::error(CreateErr::BytesLen, std::format("Not enough data ({} bytes) provided for {} ({}bytes)", spans.size(), typename_str, types_size));
            };

            auto is_aligned = [&]<auto... In>(std::index_sequence<In...>)
                -> bool
            {
                return ((reinterpret_cast<std::uintptr_t>(spans.at(In).data()) % alignof(Args) == 0) && ...);
            };

            if(!is_aligned(std::index_sequence_for<Args...>{}))
            {
                return xxas::error(CreateErr::BytesLen, "Memory spans are not properly aligned for argument types");
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
                .function = [funct = std::cref(funct), args = std::move(arg_refs)]
                    -> Result
                {   // Unpack the arguments and perfectly forward them to the function.
                    return std::apply([&funct](auto&&... unpacked_args)
                    {
                        return std::invoke(funct, std::forward<Args>(unpacked_args)...);
                    }, args);
                },
            };
        };
    };

    enum Err: std::uint8_t
    {
        Missing,
    };

    // Jit compiler output bindings.
    using Output = std::vector<Binding>;
    using Result = xxas::Result<Output, Err>;

    // Just-in-time compiles loose instruction information into direct function calls for a thread.
    template<const auto& arch> constexpr static auto function(const Insns& input, const ThreadContext<arch>& ctx)
        -> Result
    {   // JIT output bindings.
        Output output{};
        for(const auto& insn: input)
        {   // Evaluate each operand.
            auto results = std::ranges::transform(insn.operands, [&](Operand& operand)
            {
                return operand.evaluate(ctx);
            });

            // Return the first error found in the evaluated operands.
            if(auto it = std::ranges::find_if(results, std::not_fn(&Operand::Result::has_value)); it != results.end())
            {
                return it->error();
            };

            // Aggregate successfully evaluated results as scalars.
            auto scalars = std::ranges::transform(results, [&](auto& result)
            {
                return *result;
            });

            // Find the iterator for the instruction.
            auto funct_it  = arch.insns.find(insn.opcode);

            if(funct_it == arch.insns.cend())
            {
                return xxas::error(Err::Missing, std::format("Cannot find a matching function for opcode: {}", insn.opcode));
            };

            // Create a new binding for the function.
            auto binding = funct_it->visit([&scalars](auto& funct)
            {
                return Binding::create(funct, scalars);
            });

            output.push_back(binding);
        };
        // Return our built functions.
        return output;
    };
};

module;
#include <cxxabi.h>
export module xxas: tests;

import std;
import :format;
import :meta;

namespace xxas
{
    struct TestError: std::exception
    {
        TestError(std::string name, std::string message)
            : name{ name }, message{ message } {};

        std::string name;
        std::string message;
    };

    // Asserts if `condition` evaluates to true with `message`.
    export auto assert(const bool& condition, const std::string& message = "",
            std::source_location source = std::source_location::current())
    {
        if(!condition)
        {
            throw TestError(source.function_name(), message.empty()
                    ? std::format("Condition at L{} doesn't evaluate to true", source.line())
                    : message);
        };
    };

    // Asserts `A` equal to `B`.
    export auto assert_eq(const auto& A, const auto& B,
            std::source_location source = std::source_location::current())
    {
        assert(A == B, std::format("L{}: {} == {}", source.line(), format::value_or_typename(A), format::value_or_typename(B)), source);
    };

    // Asserts `A` not equal to `B`.
    export auto assert_ne(const auto& A, const auto& B,
            std::source_location source = std::source_location::current())
    {
        assert(A != B, std::format("L{}: {} != {}", source.line(), format::value_or_typename(A), format::value_or_typename(B)), source);
    };

    struct Case
    {
        using FunctionType = void(*)();
        FunctionType function;

        constexpr Case(FunctionType&& funct)
            : function{ std::move(funct) } {};

        auto operator()() const
            -> std::pair<bool, std::optional<TestError>>
        {
            try
            {
                this->function();
            }
            catch(const TestError& err)
            {
                return { false, err };
            };

            return { true, std::nullopt };
        };
    };


    export template<class... Fns> struct Tests
    {
        using ArrayType = std::array<Case, sizeof...(Fns)>;

        ArrayType cases;

        constexpr Tests(Fns&&... fns)
            : cases{ Case{ fns }... } {};

        auto operator()(std::source_location source = std::source_location::current()) const
            -> bool
        {
            if(this->cases.empty())
            {   // Ignore empty test suites.
                std::println("ignoring... no present test cases (path: \"{}\")\n", source.file_name());

                // No test to pass.
                return true;
            };

            auto start = std::chrono::high_resolution_clock::now();

            // Total passed cases.
            std::size_t passed{};

            // Print header.
            std::println("running {} test{}... (path: \"{}\")\n",
                this->cases.size(), this->cases.size() != 1 ? "s" : "", source.file_name());

            for(auto& test: this->cases)
            {   // Perform the test, print the result of it.
                if(auto result = std::invoke(test); result.first)
                {
                    std::println("performing test... ok");
                    ++passed;
                }
                else if(auto what = result.second; what.has_value())
                {
                    std::println("performing test... FAILED ({})", what->name);
                    std::println("          failed with assertion: \"{}\"", what->message);
                };
            };

            // Stop measuring time; duration between start and end; duration formatted as seconds.
            auto end              = std::chrono::high_resolution_clock::now();
            auto duration         = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            auto formatted_secs   = format::significant_digits(duration.count() / 1'000'000.0);

            // Print footer.
            std::println("\ntest results: {} total; {} passed; {} failed; finished in {}s\n",
                this->cases.size(), passed, this->cases.size() - passed, formatted_secs);

            // Return if all test passed.
            return passed != this->cases.size();
        };
    };
};

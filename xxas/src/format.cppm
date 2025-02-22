module;
#include<cxxabi.h>
export module xxas: format;

import std;

namespace xxas
{
    namespace format
    {   // Formats a double as the most significant digits dynamically.
        export template<std::floating_point T> auto significant_digits(T value)
            -> std::string
        {
            if(value == 0)
            {
                return "0";
            };

            // Calculate the number of significant digits needed.
            int precision = std::max(0, static_cast<int>(-std::log10(value)) + 2);

            // Construct the format string dynamically.
            std::string format_str = std::format("{{:.{}f}}", precision);

            // Use std::format with the constructed format string.
            return std::vformat(format_str, std::make_format_args(value));
        };

        // Returns the demangled type name for `T`.
        export template<class T> constexpr auto demangled_typename()
            -> std::string
        {
            int status = 0;

            // We use a std::unique_ptr to manage the allocation
            // returned from cxa_demangle.
            std::unique_ptr<char, void(*)(void*)> res
            {
                abi::__cxa_demangle(typeid(T).name(), NULL, NULL, &status),
                std::free
            };

            if (status != 0)
            {
                return typeid(T).name();
            };

            return res.get();
        };

        // Format the value as either its underlying value,
        // or if the value is not formattable, format the typename of it.
        export template<class T> constexpr auto value_or_typename(T& value)
            -> std::string
        {
            if constexpr(std::formattable<T, char>)
            {
                return std::format("{}", value);
            };

            return demangled_typename<T>();
        };
    };
};

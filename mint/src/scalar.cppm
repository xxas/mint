export module mint: scalar;

import std;
import xxas;
import :traits;

/*** **
 **
 **  module:   mint: scalar
 **  purpose:  Represents the underlying byte slice of a provided argument.
 **
 *** **/

namespace mint
{
    export struct Scalar
    {
        using Bytes = std::span<std::byte>;

        Bytes bytes;

        template<class T> constexpr static auto from(T& value)
            -> Scalar
        {
            return Scalar
            {
                .bytes
                {
                    reinterpret_cast<std::byte*>(std::addressof(value)), sizeof(T)
                },
            };
        };

        template <typename T> constexpr auto as() const
            -> const T&
        {
            return *reinterpret_cast<const T*>(bytes.data());
        };

        template <typename T> constexpr auto as()
            -> T&
        {
            return *reinterpret_cast<T*>(bytes.data());
        };
    };
};

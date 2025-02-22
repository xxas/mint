export module mint: scalar;

import std;
import xxas;

import :traits;
import :context;

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
            -> T
        {
            return *reinterpret_cast<const T*>(bytes.data());
        };
    };
};

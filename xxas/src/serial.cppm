export module xxas: serial;

import std;

namespace xxas
{   // Serializes T to a range of std::byte using std::bit_cast.
    export template<class T> constexpr auto encode(const T& data)
        -> std::array<std::byte, sizeof(T)>
      requires std::is_trivially_copyable_v<T>
    {
        using Bytes = std::array<std::byte, sizeof(T)>;
        return std::bit_cast<Bytes>(data);
    };

    // Constexpr deserialization from a range of bytes to T.
    export template<class T, std::ranges::contiguous_range R> constexpr auto decode(const R& range)
        -> T
      requires std::convertible_to<std::ranges::range_value_t<R>, std::byte>
            && std::is_trivially_copyable_v<T>
    {
        using Bytes = std::array<std::byte, sizeof(T)>;

        Bytes bytes{};
        std::memcpy(bytes.data(), std::ranges::data(range), sizeof(T));

        return std::bit_cast<T>(bytes);
    };

    // Basic serializer context.
    export struct Serializer
    {
        using Bytes = std::vector<std::byte>;
        Bytes bytes;

        constexpr auto append(std::span<const std::byte> next)
            -> Serializer&
        {
            this->bytes.insert(this->bytes.end(), next.begin(), next.end());
            return *this;
        };

        template<class T> constexpr auto write(const T& value)
            -> Serializer&
        {
            auto encoded = encode(value);
            this->bytes.insert(this->bytes.end(), encoded.begin(), encoded.end());
            return *this;
        };

        constexpr auto size() const
        {
            return this->bytes.size();
        };
    };

    // Basic deserializer context with offset tracking.
    export struct Deserializer
    {
        using Bytes = std::span<const std::byte>;

        Bytes       bytes;
        std::size_t offset = 0;

        constexpr Deserializer(Bytes bytes)
            : bytes{ bytes } {};

        // Read bytes as type T.
        template<class T> auto read()
            -> std::optional<T>
          requires std::is_trivially_copyable_v<T>
        {
            if(this->offset + sizeof(T) > this->bytes.size())
            {
                return std::nullopt;
            };

            auto subspan = this->bytes.subspan(this->offset, sizeof(T));
            this->offset = this->offset + sizeof(T);

            return decode<T>(subspan);
        };

        // Returns if the Deserializer has read all the bytes in view.
        constexpr auto has_more() const
            -> bool
        {
            return this->offset < this->bytes.size();
        };
    };
};

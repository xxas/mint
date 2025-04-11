import std;
import xxas;

namespace xxas_test
{
    void different_hashes()
    {   // Digest of `hello` and `world` should be different.
        constexpr std::string hello = "hello";
        constexpr std::string world = "world";

        constexpr std::uint64_t hash_0 = xxas::fnv1a_64(hello);
        constexpr std::uint64_t hash_1 = xxas::fnv1a_64(world);

        xxas::assert_ne(hash_0, hash_1);
    };

    void consistent_hash()
    {   // `test 123!@#` should produce the same digest twice.
        constexpr std::string string = "test 123!@#";

        constexpr std::uint64_t hash_0 = xxas::fnv1a_64(string);
        constexpr std::uint64_t hash_1 = xxas::fnv1a_64(string);

        xxas::assert_eq(hash_0, hash_1);
    };

    void known_hash()
    {   // `example` should digest to 0x430b1483c8d66041 as a 64-bit FNV-1a.
        constexpr std::string string = "example";

        constexpr std::uint64_t hash_64     = xxas::fnv1a_64(string);
        constexpr std::uint64_t expected_64 = 0x430b1483c8d66041;

        xxas::assert_eq(hash_64, expected_64);

        // `example` should output 0x8bf23ea1 as a 32-bit hash.
        constexpr std::uint32_t hash_32     = xxas::fnv1a_32(string);
        constexpr std::uint32_t expected_32 = 0x8bf23ea1;

        xxas::assert_eq(hash_32, expected_32);
    };

    void cstring_hash()
    {   // C-style strings should be digestable.
        constexpr const char    hello_world[]   = "hello world!";
        constexpr std::uint64_t hash            = xxas::fnv1a_64(hello_world);

        xxas::assert_ne(hash, xxas::fnv1a_64.offset());
    };

    void numeric_hash()
    {   // Arrays of data should be digestable.
        constexpr int           numbers[] = { 1, 2, 3, 4 };
        constexpr std::uint64_t hash      = xxas::fnv1a_64(numbers);

        xxas::assert_ne(hash, xxas::fnv1a_64.offset());
    };

    void empty_range_hash()
    {   // Empty ranges should result in the digest being equal to the offset.
        const std::string empty;
        auto hash = xxas::fnv1a_64(empty);

        xxas::assert_eq(hash, xxas::fnv1a_64.offset());
    };

    void hash_bitness()
    {   // Hash bitness should affect the consistency of the digest.
        constexpr std::string string = "hello world";

        std::uint32_t hash_32 = xxas::fnv1a_32(string);
        std::uint64_t hash_64 = xxas::fnv1a_64(string);

        xxas::assert_ne(hash_32, hash_64);
    };

    constexpr inline auto fnv1a = xxas::Tests
    {
        different_hashes, consistent_hash, known_hash,
        cstring_hash, numeric_hash, empty_range_hash,
        hash_bitness,
    };
};


int main()
{
    return xxas_test::fnv1a();
};

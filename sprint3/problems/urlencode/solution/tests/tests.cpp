#include <gtest/gtest.h>

#include "../src/urlencode.h"

using namespace std::literals;

TEST(UrlEncodeTestSuite, EmptyString) {
    EXPECT_EQ(UrlEncode(""sv), ""s);
    EXPECT_EQ(UrlEncode(std::string_view{}), ""s);
}

TEST(UrlEncodeTestSuite, OrdinaryCharsAreNotEncoded) {
    EXPECT_EQ(
        UrlEncode("abcdefghijklmnopqrstuvwxyz"sv),
        "abcdefghijklmnopqrstuvwxyz"s
    );

    EXPECT_EQ(
        UrlEncode("ABCDEFGHIJKLMNOPQRSTUVWXYZ"sv),
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"s
    );

    EXPECT_EQ(UrlEncode("0123456789-._~"sv), "0123456789-._~"s);
}

TEST(UrlEncodeTestSuite, SpacesAreReplacedWithPlus) {
    EXPECT_EQ(UrlEncode(" "sv), "+"s);
    EXPECT_EQ(UrlEncode("   "sv), "+++"s);
    EXPECT_EQ(UrlEncode(" Hello World "sv), "+Hello+World+"s);
}

TEST(UrlEncodeTestSuite, ExamplesFromTask) {
    EXPECT_EQ(UrlEncode("Hello World!"sv), "Hello+World%21"s);
    EXPECT_EQ(UrlEncode("abc*"sv), "abc%2A"s);
}

TEST(UrlEncodeTestSuite, AllReservedCharsAreEncoded) {
    EXPECT_EQ(
        UrlEncode("!#$&'()*+,/:;=?@[]"sv),
        "%21%23%24%26%27%28%29%2A%2B%2C%2F%3A%3B%3D%3F%40%5B%5D"s
    );
}

TEST(UrlEncodeTestSuite, PlusAndSpaceAreDistinguished) {
    EXPECT_EQ(UrlEncode("a+b c"sv), "a%2Bb+c"s);
}

TEST(UrlEncodeTestSuite, ControlCharsAreEncoded) {
    EXPECT_EQ(
        UrlEncode("\t\n\r"sv),
        "%09%0A%0D"s
    );

    EXPECT_EQ(
        UrlEncode("\x01\x0F\x10\x1F"sv),
        "%01%0F%10%1F"s
    );
}

TEST(UrlEncodeTestSuite, EmbeddedNullIsEncoded) {
    EXPECT_EQ(UrlEncode("\0"sv), "%00"s);
    EXPECT_EQ(UrlEncode("a\0b"sv), "a%00b"s);
}

TEST(UrlEncodeTestSuite, AllControlBytesAreEncoded) {
    for (unsigned int code = 0; code < 32; ++code) {
        SCOPED_TRACE(code);

        const std::string input(1, static_cast<char>(code));

        // Отдельная таблица ожидаемых результатов:
        // тест не повторяет алгоритм формирования %HH.
        constexpr std::string_view expected[] = {
            "%00", "%01", "%02", "%03",
            "%04", "%05", "%06", "%07",
            "%08", "%09", "%0A", "%0B",
            "%0C", "%0D", "%0E", "%0F",
            "%10", "%11", "%12", "%13",
            "%14", "%15", "%16", "%17",
            "%18", "%19", "%1A", "%1B",
            "%1C", "%1D", "%1E", "%1F"
        };

        EXPECT_EQ(UrlEncode(input), std::string(expected[code]));
    }
}

TEST(UrlEncodeTestSuite, BoundaryBytesAreHandledCorrectly) {
    const std::string input{
        static_cast<char>(31),
        static_cast<char>(32),
        static_cast<char>(33),
        static_cast<char>(126),
        static_cast<char>(127),
        static_cast<char>(128),
        static_cast<char>(255)
    };

    std::string expected = "%1F+%21~";
    expected.push_back(static_cast<char>(127));
    expected += "%80%FF";

    EXPECT_EQ(UrlEncode(input), expected);
}

TEST(UrlEncodeTestSuite, HighBytesAreEncoded) {
    EXPECT_EQ(
        UrlEncode("\x80\x9F\xA0\xFE\xFF"sv),
        "%80%9F%A0%FE%FF"s
    );
}

TEST(UrlEncodeTestSuite, Utf8IsEncodedByteByByte) {
    // UTF-8 для слова "Привет", без зависимости
    // от кодировки исходного файла.
    constexpr auto input =
        "\xD0\x9F\xD1\x80\xD0\xB8"
        "\xD0\xB2\xD0\xB5\xD1\x82"sv;

    EXPECT_EQ(
        UrlEncode(input),
        "%D0%9F%D1%80%D0%B8%D0%B2%D0%B5%D1%82"s
    );
}

TEST(UrlEncodeTestSuite, OtherPrintableCharsAreNotEncoded) {
    EXPECT_EQ(
        UrlEncode("%\"<>\\^`{|}"sv),
        "%\"<>\\^`{|}"s
    );
}

TEST(UrlEncodeTestSuite, PercentSequencesAreNotInterpreted) {
    EXPECT_EQ(UrlEncode("%20%2A"sv), "%20%2A"s);
}

TEST(UrlEncodeTestSuite, StringViewLengthIsRespected) {
    const std::string source = "abc!def";

    const std::string_view view(source.data(), 3);

    EXPECT_EQ(UrlEncode(view), "abc"s);
}

TEST(UrlEncodeTestSuite, MixedInput) {
    EXPECT_EQ(
        UrlEncode("Hello World! a+b/c?x=1&y=2\n"sv),
        "Hello+World%21+a%2Bb%2Fc%3Fx%3D1%26y%3D2%0A"s
    );
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
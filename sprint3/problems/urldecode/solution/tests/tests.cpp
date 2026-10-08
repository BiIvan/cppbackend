#define BOOST_TEST_MODULE UrlDecodeTests

#include <boost/test/unit_test.hpp>

#include <stdexcept>
#include <string>

#include "../src/urldecode.h"

// 1. Пустая строка.
BOOST_AUTO_TEST_CASE(EmptyString) {
    BOOST_CHECK_EQUAL(UrlDecode(""), "");
}

// 2. Символы, которым не требуется %-кодирование.
BOOST_AUTO_TEST_CASE(UnencodedOrdinaryCharacters) {
    const std::string input =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789-_.~";

    BOOST_CHECK_EQUAL(UrlDecode(input), input);
}

// Незакодированные символы сохраняются.
// Плюс проверяется отдельно, поскольку обозначает пробел.
BOOST_AUTO_TEST_CASE(PreserveUnencodedCharacters) {
    BOOST_CHECK_EQUAL(UrlDecode("Hello World !"), "Hello World !");

    BOOST_CHECK_EQUAL(
        UrlDecode("!#$&'()*,/:;=?@[]"),
        "!#$&'()*,/:;=?@[]"
    );

    BOOST_CHECK_EQUAL(
        UrlDecode("Hello%20World !"),
        "Hello World !"
    );

    BOOST_CHECK_EQUAL(
        UrlDecode("/path%2Fto/file?name=Hello+World&x=1!"),
        "/path/to/file?name=Hello World&x=1!"
    );
}

// 3. Все перечисленные зарезервированные символы:
// %-последовательности в верхнем регистре.
BOOST_AUTO_TEST_CASE(ReservedCharactersUppercaseHex) {
    BOOST_CHECK_EQUAL(
        UrlDecode(
            "%21%23%24%26%27%28%29%2A%2B"
            "%2C%2F%3A%3B%3D%3F%40%5B%5D"
        ),
        "!#$&'()*+,/:;=?@[]"
    );
}

// Те же символы: %-последовательности в нижнем регистре.
BOOST_AUTO_TEST_CASE(ReservedCharactersLowercaseHex) {
    BOOST_CHECK_EQUAL(
        UrlDecode(
            "%21%23%24%26%27%28%29%2a%2b"
            "%2c%2f%3a%3b%3d%3f%40%5b%5d"
        ),
        "!#$&'()*+,/:;=?@[]"
    );
}

// Буквы допустимы в обоих разрядах, в том числе
// в смешанном регистре. Проверяем результат как байт.
BOOST_AUTO_TEST_CASE(MixedCaseHexDigits) {
    const std::string expected(1, static_cast<char>(0xAB));

    BOOST_CHECK_EQUAL(UrlDecode("%AB"), expected);
    BOOST_CHECK_EQUAL(UrlDecode("%ab"), expected);
    BOOST_CHECK_EQUAL(UrlDecode("%Ab"), expected);
    BOOST_CHECK_EQUAL(UrlDecode("%aB"), expected);
}

// Последовательности в начале, середине и конце,
// а также несколько последовательностей подряд.
BOOST_AUTO_TEST_CASE(PercentSequencesAtDifferentPositions) {
    BOOST_CHECK_EQUAL(UrlDecode("%41bc"), "Abc");
    BOOST_CHECK_EQUAL(UrlDecode("a%42c"), "aBc");
    BOOST_CHECK_EQUAL(UrlDecode("ab%43"), "abC");

    BOOST_CHECK_EQUAL(UrlDecode("%41%42%43"), "ABC");
    BOOST_CHECK_EQUAL(UrlDecode("Hello%20world%21"), "Hello world!");
}

// 4. Некорректные %-последовательности.
BOOST_AUTO_TEST_CASE(InvalidPercentSequences) {
    // Ошибка в первом, втором или обоих разрядах.
    BOOST_CHECK_THROW(UrlDecode("%G0"), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%0G"), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%GG"), std::invalid_argument);

    // Знаки вне диапазонов 0–9, A–F, a–f.
    BOOST_CHECK_THROW(UrlDecode("%/0"), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%0:"), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%@0"), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%0`"), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%0g"), std::invalid_argument);

    BOOST_CHECK_THROW(UrlDecode("% 0"), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%+0"), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%%20"), std::invalid_argument);

    // Ошибка после обычных символов или валидного кода.
    BOOST_CHECK_THROW(UrlDecode("abc%XZdef"), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%41%G0"), std::invalid_argument);
}

// 5. Неполные %-последовательности.
BOOST_AUTO_TEST_CASE(IncompletePercentSequences) {
    BOOST_CHECK_THROW(UrlDecode("%"), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%2"), std::invalid_argument);

    BOOST_CHECK_THROW(UrlDecode("abc%"), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("abc%2"), std::invalid_argument);

    BOOST_CHECK_THROW(UrlDecode("%41%"), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%41%2"), std::invalid_argument);
}

// 6. Незакодированный плюс обозначает пробел.
BOOST_AUTO_TEST_CASE(PlusBecomesSpace) {
    BOOST_CHECK_EQUAL(UrlDecode("+"), " ");
    BOOST_CHECK_EQUAL(UrlDecode("a+b"), "a b");
    BOOST_CHECK_EQUAL(UrlDecode("+abc+"), " abc ");
    BOOST_CHECK_EQUAL(UrlDecode("a++b"), "a  b");
    BOOST_CHECK_EQUAL(UrlDecode("+++"), "   ");

    // Оба способа записи пробела.
    BOOST_CHECK_EQUAL(UrlDecode("+%20+"), "   ");
}

// Плюс, полученный из %-последовательности,
// не должен повторно преобразовываться в пробел.
BOOST_AUTO_TEST_CASE(EncodedPlusRemainsPlus) {
    BOOST_CHECK_EQUAL(UrlDecode("%2B"), "+");
    BOOST_CHECK_EQUAL(UrlDecode("%2b"), "+");
    BOOST_CHECK_EQUAL(UrlDecode("C%2B%2B"), "C++");
    BOOST_CHECK_EQUAL(UrlDecode("a+%2B+b"), "a + b");
}

// Декодирование выполняется однократно.
BOOST_AUTO_TEST_CASE(DecodeOnlyOnce) {
    BOOST_CHECK_EQUAL(UrlDecode("%25"), "%");
    BOOST_CHECK_EQUAL(UrlDecode("%2520"), "%20");
    BOOST_CHECK_EQUAL(UrlDecode("%252B"), "%2B");
}

// %00 должен сохраняться как байт внутри std::string.
BOOST_AUTO_TEST_CASE(EncodedNullByte) {
    const std::string expected("a\0b", 3);
    const std::string actual = UrlDecode("a%00b");

    BOOST_CHECK_EQUAL(actual.size(), expected.size());
    BOOST_CHECK(actual == expected);
}

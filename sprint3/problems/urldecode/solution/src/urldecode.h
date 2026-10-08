#pragma once

#include <string>
#include <string_view>

/*
Возвращает URL-декодированное представление строки str.
Пример: "Hello+World%20%21" превращается в "Hello World !".
В случае неполной или некорректной %-последовательности
выбрасывает std::invalid_argument.
*/
std::string UrlDecode(std::string_view str);
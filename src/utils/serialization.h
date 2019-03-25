#include <tuple>
#include <utility>
#include <array>
#include <vector>
#include <set>
#include <typeinfo>
#include <typeindex>
#include <memory>

template <typename T, typename tp_enabled = std::void_t<>>
constexpr bool is_tuple_like = false;

template <typename T>
constexpr bool is_tuple_like
<T, std::void_t <decltype(std::tuple_size<T>::value),
decltype(std::declval<T&>().template get<std::size_t(0)>())>> = true;


template<typename T>
auto encode(T const& t, std::ostream& os)->
std::enable_if_t<is_tuple_like<T>> {
    encode_impl(t, os, std::make_index_sequence<std::tuple_size<T>::value>{ });
}

template <typename T>
auto encode_one(T const& t, std::ostream& os)->
std::enable_if_t<!is_tuple_like<T>> {
    os.write((char*)&t, sizeof(T));
}

template <typename T>
auto encode_one(std::vector<T> const& vec, std::ostream& os) {
    size_t size = vec.size();
    os.write((char*)&size, sizeof(size_t));
    os.write(reinterpret_cast<const char*>(&vec[0]), sizeof(T) * size);
}

template <typename T>
auto encode_one(T const& t, std::ostream& os)->
std::enable_if_t<is_tuple_like<T>> {
    encode_impl(t, os, std::make_index_sequence<std::tuple_size<T>::value>{ });
}

template <typename T, std::size_t... I>
void encode_impl(T const& t, std::ostream& os, std::index_sequence<I...> const) {
    [[maybe_unused]] int const temp[] = { 
        (encode_one(t.template get<I>(), os), 0)... 
    };
}

template <typename T>
std::string raw_encode_to_string(T const& t) {
    std::stringstream s;
    encode(t, s);
    std::string result = s.str();
    return result;
}

template<typename T>
std::enable_if_t<is_tuple_like<T> && std::tuple_size<T>::value == 1> decode() {
    // using t = typename std::tuple_element<0, T>::type;
    // static_assert(std::is_same<t, Bytes>::value, "!");
}

template<typename T>
std::enable_if_t<is_tuple_like<T> && std::tuple_size<T>::value == 2> decode() {}

template<typename T>
std::enable_if_t<is_tuple_like<T> && std::tuple_size<T>::value == 3> decode() {}

template<typename T>
std::enable_if_t<is_tuple_like<T> && std::tuple_size<T>::value == 4> decode() {}

template<typename T>
std::enable_if_t<is_tuple_like<T> && std::tuple_size<T>::value == 5> decode() {}

template<typename T>
std::enable_if_t<is_tuple_like<T> && std::tuple_size<T>::value == 6> decode() {}

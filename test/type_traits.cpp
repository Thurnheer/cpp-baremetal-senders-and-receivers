#include "detail/common.hpp"

#include <async/concepts.hpp>
#include <async/inline_scheduler.hpp>
#include <async/type_traits.hpp>

#include <catch2/catch_test_macros.hpp>

#include <concepts>
#include <type_traits>

TEST_CASE("connect_result_t", "[type_traits]") {
    auto s = async::inline_scheduler{}.schedule();
    auto r = receiver{[] {}};
    static_assert(async::operation_state<
                  async::connect_result_t<decltype(s), decltype(r)>>);
}

namespace {
struct typed_sender1 {
    using completion_signatures =
        async::completion_signatures<async::set_value_t(int),
                                     async::set_error_t(float),
                                     async::set_stopped_t()>;
};
struct typed_sender2 {
    using completion_signatures = async::completion_signatures<>;
};
} // namespace

TEST_CASE("completion signatures with exposed type", "[type_traits]") {
    static_assert(
        std::same_as<typename typed_sender1::completion_signatures,
                     async::completion_signatures_of_t<typed_sender1>>);
    static_assert(
        std::same_as<typename typed_sender2::completion_signatures,
                     async::completion_signatures_of_t<typed_sender2>>);
}

TEST_CASE("typed completion signatures by channel", "[type_traits]") {
    static_assert(
        std::same_as<async::completion_signatures<async::set_value_t(int)>,
                     async::value_signatures_of_t<typed_sender1>>);
    static_assert(
        std::same_as<async::completion_signatures<async::set_error_t(float)>,
                     async::error_signatures_of_t<typed_sender1>>);
    static_assert(
        std::same_as<async::completion_signatures<async::set_stopped_t()>,
                     async::stopped_signatures_of_t<typed_sender1>>);
}

namespace {
struct queryable_sender1 {
    [[nodiscard]] friend constexpr auto
    tag_invoke(async::get_completion_signatures_t, queryable_sender1 const &,
               auto &&) noexcept
        -> async::completion_signatures<async::set_value_t(int),
                                        async::set_error_t(float),
                                        async::set_stopped_t()> {
        return {};
    }
};

struct queryable_sender2 {
    [[nodiscard]] friend constexpr auto
    tag_invoke(async::get_completion_signatures_t, queryable_sender2 const &,
               auto &&) noexcept -> async::completion_signatures<> {
        return {};
    }

    // query takes precedence over the exposed type
    using completion_signatures =
        async::completion_signatures<async::set_value_t(int),
                                     async::set_error_t(float),
                                     async::set_stopped_t()>;
};
} // namespace

TEST_CASE("completion signatures with exposed query", "[type_traits]") {
    static_assert(
        std::same_as<async::completion_signatures_of_t<queryable_sender1>,
                     async::completion_signatures<async::set_value_t(int),
                                                  async::set_error_t(float),
                                                  async::set_stopped_t()>>);
    static_assert(
        std::same_as<async::completion_signatures_of_t<queryable_sender2>,
                     async::completion_signatures<>>);
}

TEST_CASE("queryable completion signatures by channel", "[type_traits]") {
    static_assert(
        std::same_as<async::completion_signatures<async::set_value_t(int)>,
                     async::value_signatures_of_t<queryable_sender1>>);
    static_assert(
        std::same_as<async::completion_signatures<async::set_error_t(float)>,
                     async::error_signatures_of_t<queryable_sender1>>);
    static_assert(
        std::same_as<async::completion_signatures<async::set_stopped_t()>,
                     async::stopped_signatures_of_t<queryable_sender1>>);
}

namespace {
template <typename T> struct dependent_env {
    using type = T;
};

struct queryable_sender3 {
    template <typename Env>
    [[nodiscard]] friend constexpr auto
    tag_invoke(async::get_completion_signatures_t, queryable_sender3 const &,
               Env const &) noexcept
        -> async::completion_signatures<
            async::set_value_t(typename Env::type)> {
        return {};
    }
};
} // namespace

TEST_CASE("queryable completion signatures dependent on environment",
          "[type_traits]") {
    static_assert(
        std::same_as<async::completion_signatures_of_t<queryable_sender3,
                                                       dependent_env<int>>,
                     async::completion_signatures<async::set_value_t(int)>>);
}

namespace {
template <typename...> struct variant;
template <typename...> struct tuple;
template <typename> struct optional;
} // namespace

TEST_CASE("types by channel (exposed types)", "[type_traits]") {
    static_assert(
        std::same_as<variant<tuple<int>>,
                     async::value_types_of_t<typed_sender1, async::empty_env,
                                             tuple, variant>>);
    static_assert(
        std::same_as<variant<tuple<float>>,
                     async::error_types_of_t<typed_sender1, async::empty_env,
                                             tuple, variant>>);
    static_assert(
        std::same_as<variant<tuple<>>,
                     async::stopped_types_of_t<typed_sender1, async::empty_env,
                                               tuple, variant>>);
    static_assert(async::sends_stopped<typed_sender1>);

    static_assert(
        std::same_as<variant<>,
                     async::value_types_of_t<typed_sender2, async::empty_env,
                                             tuple, variant>>);
    static_assert(
        std::same_as<variant<>,
                     async::error_types_of_t<typed_sender2, async::empty_env,
                                             tuple, variant>>);
    static_assert(
        std::same_as<variant<>,
                     async::stopped_types_of_t<typed_sender2, async::empty_env,
                                               tuple, variant>>);
    static_assert(not async::sends_stopped<typed_sender2>);
}

TEST_CASE("types by channel (queries with empty env)", "[type_traits]") {
    static_assert(
        std::same_as<variant<tuple<int>>,
                     async::value_types_of_t<
                         queryable_sender1, async::empty_env, tuple, variant>>);
    static_assert(
        std::same_as<variant<tuple<float>>,
                     async::error_types_of_t<
                         queryable_sender1, async::empty_env, tuple, variant>>);
    static_assert(
        std::same_as<variant<tuple<>>,
                     async::stopped_types_of_t<
                         queryable_sender1, async::empty_env, tuple, variant>>);
    static_assert(async::sends_stopped<queryable_sender1>);

    static_assert(
        std::same_as<variant<>,
                     async::value_types_of_t<
                         queryable_sender2, async::empty_env, tuple, variant>>);
    static_assert(
        std::same_as<variant<>,
                     async::error_types_of_t<
                         queryable_sender2, async::empty_env, tuple, variant>>);
    static_assert(
        std::same_as<variant<>,
                     async::stopped_types_of_t<
                         queryable_sender2, async::empty_env, tuple, variant>>);
    static_assert(not async::sends_stopped<queryable_sender2>);
}

TEST_CASE("types by channel (queries with dependent env)", "[type_traits]") {
    static_assert(std::same_as<
                  variant<tuple<int>>,
                  async::value_types_of_t<queryable_sender3, dependent_env<int>,
                                          tuple, variant>>);
    static_assert(
        std::same_as<
            variant<tuple<float>>,
            async::value_types_of_t<queryable_sender3, dependent_env<float>,
                                    tuple, variant>>);
}

TEST_CASE("types by channel (non-variadic templates)", "[type_traits]") {
    static_assert(
        std::same_as<tuple<int>,
                     async::value_types_of_t<typed_sender1, async::empty_env,
                                             tuple, std::type_identity_t>>);
    static_assert(
        std::same_as<tuple<float>,
                     async::error_types_of_t<typed_sender1, async::empty_env,
                                             tuple, std::type_identity_t>>);
    static_assert(
        std::same_as<variant<optional<int>>,
                     async::value_types_of_t<typed_sender1, async::empty_env,
                                             optional, variant>>);

    static_assert(
        std::same_as<int, async::value_types_of_t<
                              typed_sender1, async::empty_env,
                              std::type_identity_t, std::type_identity_t>>);
}

namespace {
template <typename T> struct unary_tuple {
    using type = T;
};
template <typename T> using unary_tuple_t = typename unary_tuple<T>::type;

template <typename T> struct unary_variant {
    using type = T;
};
template <typename T> using unary_variant_t = typename unary_variant<T>::type;

template <typename S, typename Tag>
concept single_sender = requires {
    typename async::detail::gather_signatures<Tag, S, async::empty_env,
                                              unary_tuple_t, unary_variant_t>;
};
} // namespace

TEST_CASE("non-variadic templates in concept", "[type_traits]") {
    static_assert(single_sender<typed_sender1, async::set_value_t>);
    static_assert(single_sender<typed_sender1, async::set_error_t>);
    static_assert(not single_sender<typed_sender2, async::set_value_t>);
}

TEST_CASE("channel holder (values)", "[type_traits]") {
    int value{};
    auto r = receiver{[&](auto i) { value = i; }};
    auto h = async::value_holder<int>{42};
    h(r);
    CHECK(value == 42);
}

TEST_CASE("channel holder (error)", "[type_traits]") {
    int value{};
    auto r = error_receiver{[&](auto i) { value = i; }};
    auto h = async::error_holder<int>{42};
    h(r);
    CHECK(value == 42);
}

TEST_CASE("channel holder (stopped)", "[type_traits]") {
    int value{};
    auto r = stopped_receiver{[&] { value = 42; }};
    auto h = async::stopped_holder<>{};
    h(r);
    CHECK(value == 42);
}
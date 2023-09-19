#include "detail/common.hpp"

#include <async/concepts.hpp>
#include <async/just.hpp>
#include <async/sync_wait.hpp>
#include <async/then.hpp>
#include <async/thread_scheduler.hpp>
#include <async/type_traits.hpp>
#include <async/when_any.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iterator>
#include <mutex>
#include <random>
#include <thread>
#include <utility>

namespace {
[[maybe_unused]] auto get_rng() -> auto & {
    static auto rng = [] {
        std::array<int, std::mt19937::state_size> seed_data;
        std::random_device r;
        std::generate_n(seed_data.data(), seed_data.size(), std::ref(r));
        std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
        return std::mt19937{seq};
    }();
    return rng;
}
} // namespace

TEST_CASE("when_any advertises what it sends", "[when_any]") {
    auto s1 = async::just(42);
    auto s2 = async::just(17);
    [[maybe_unused]] auto w = async::when_any(s1, s2);
    static_assert(async::sender_of<decltype(w), async::set_value_t(int)>);
    static_assert(not async::sender_of<decltype(w), async::set_error_t()>);
}

TEST_CASE("when_any advertises errors", "[when_any]") {
    auto s1 = async::just(42);
    auto s2 = async::just_error(17);
    [[maybe_unused]] auto w = async::when_any(s1, s2);
    static_assert(async::sender_of<decltype(w), async::set_error_t(int)>);
}

TEST_CASE("complete with first success", "[when_any]") {
    int value{};
    auto s1 = async::just(42);
    auto s2 = async::just(17);
    auto w = async::when_any(s1, s2);

    auto op = async::connect(w, receiver{[&](auto i) {
                                 CHECK(value == 0);
                                 value = i;
                             }});
    op.start();
    CHECK(value == 42);
}

TEST_CASE("complete with first error", "[when_any]") {
    int value{};
    auto s1 = async::just_error(42);
    auto s2 = async::just_error(17);
    auto w = async::when_any(s1, s2);

    auto op = async::connect(w, error_receiver{[&](auto i) {
                                 CHECK(value == 0);
                                 value = i;
                             }});
    op.start();
    CHECK(value == 42);
}

TEST_CASE("complete with all stopped", "[when_any]") {
    int value{};
    auto s1 = async::just_stopped();
    auto s2 = async::just_stopped();
    auto w = async::when_any(s1, s2);

    auto op = async::connect(w, stopped_receiver{[&]() {
                                 CHECK(value == 0);
                                 value = 42;
                             }});
    op.start();
    CHECK(value == 42);
}

TEST_CASE("complete with first success (void)", "[when_any]") {
    int value{};
    auto s1 = async::just();
    auto s2 = async::just(17);
    auto w = async::when_any(s1, s2);

    auto op = async::connect(w, receiver{[&](auto...) {
                                 CHECK(value == 0);
                                 value = 42;
                             }});
    op.start();
    CHECK(value == 42);
}

TEST_CASE("move-only value", "[when_any]") {
    int value{};
    auto s = async::just(move_only{42});
    auto w = async::when_any(std::move(s));
    static_assert(async::singleshot_sender<decltype(w), universal_receiver>);
    auto op = async::connect(
        std::move(w), receiver{[&](move_only<int> mo) { value = mo.value; }});
    op.start();
    CHECK(value == 42);
}

TEST_CASE("copy sender", "[when_any]") {
    int value{};
    auto const s = async::just(42);
    auto w = async::when_any(s);
    static_assert(async::multishot_sender<decltype(w), universal_receiver>);
    auto op = async::connect(w, receiver{[&](auto i) { value = i; }});
    op.start();
    CHECK(value == 42);
}

TEST_CASE("move sender", "[when_any]") {
    int value{};
    auto s = async::just(42);
    auto w = async::when_any(s);
    static_assert(async::multishot_sender<decltype(w), universal_receiver>);
    auto op =
        async::connect(std::move(w), receiver{[&](auto i) { value = i; }});
    op.start();
    CHECK(value == 42);
}

TEST_CASE("when_any with thread scheduler", "[when_any]") {
    std::uniform_int_distribution<> dis{5, 10};
    auto const d1 = std::chrono::milliseconds{dis(get_rng())};
    auto const d2 = std::chrono::milliseconds{dis(get_rng())};

    auto s1 = async::thread_scheduler::schedule() | async::then([&] {
                  std::this_thread::sleep_for(d1);
                  return 42;
              });
    auto s2 = async::thread_scheduler::schedule() | async::then([&] {
                  std::this_thread::sleep_for(d2);
                  return 17;
              });
    auto const result = async::when_any(s1, s2) | async::sync_wait();
    REQUIRE(result.has_value());
    auto const [i] = *result;
    CHECK(((i == 42) or (i == 17)));
}

TEST_CASE("first_successful policy", "[when_any]") {
    int value{};
    auto s1 = async::just_error(42);
    auto s2 = async::just(17);
    auto w = async::first_successful(s1, s2);

    auto op = async::connect(w, receiver{[&](auto i) {
                                 CHECK(value == 0);
                                 value = i;
                             }});
    op.start();
    CHECK(value == 17);
}

TEST_CASE("first_complete policy", "[when_any]") {
    int value{};
    auto s1 = async::just_stopped();
    auto s2 = async::just(17);
    auto w = async::stop_when(s1, s2);

    auto op = async::connect(w, stopped_receiver{[&] {
                                 CHECK(value == 0);
                                 value = 42;
                             }});
    op.start();
    CHECK(value == 42);
}

TEST_CASE("when_any cancellation (before start)", "[when_any]") {
    bool success{};
    bool fail{};
    phase_control ctrl{};

    auto s =
        async::thread_scheduler::schedule() | async::then([&] { fail = true; });
    auto const w =
        async::when_any(s) | async::upon_stopped([&] { success = true; });

    auto r = stoppable_receiver{[&] { ctrl.advance(); }};
    auto op = async::connect(w, r);

    r.request_stop();
    op.start();

    ctrl.wait_for(1);
    CHECK(success);
    CHECK(not fail);
}

TEST_CASE("when_any cancellation (during operation)", "[when_any]") {
    bool success{};
    phase_control ctrl{};

    auto s = async::thread_scheduler::schedule() |
             async::then([&] { ctrl.advance_and_wait(); });
    auto const w =
        async::when_any(s) | async::upon_stopped([&] { success = true; });

    auto r = stoppable_receiver{[&] { ctrl.advance(); }};
    auto op = async::connect(w, r);

    op.start();
    ctrl.wait_for(1);
    r.request_stop();

    ctrl.advance_and_wait();
    CHECK(success);
}

TEST_CASE("stop_when is pipeable", "[when_any]") {
    int value{};
    auto w = async::just(42) | async::stop_when(async::just(17));
    auto op = async::connect(w, receiver{[&](auto i) {
                                 CHECK(value == 0);
                                 value = i;
                             }});
    op.start();
    CHECK(value == 42);
}

TEST_CASE("when_any with zero args never completes", "[when_any]") {
    int value{};
    [[maybe_unused]] auto w = async::when_any();
    static_assert(std::same_as<async::completion_signatures_of_t<decltype(w)>,
                               async::completion_signatures<>>);

    auto op = async::connect(w, receiver{[&] { value = 42; }});
    op.start();
    CHECK(value == 0);
}

TEST_CASE("when_any with zero args can be stopped (before start)",
          "[when_any]") {
    int value{};
    [[maybe_unused]] auto w = async::when_any();
    auto r = only_stoppable_receiver{[&] { value = 42; }};
    static_assert(
        std::same_as<async::completion_signatures_of_t<
                         decltype(w), async::env_of_t<decltype(r)>>,
                     async::completion_signatures<async::set_stopped_t()>>);

    auto op = async::connect(w, r);
    r.request_stop();
    op.start();
    CHECK(value == 42);
}

TEST_CASE("when_any with zero args can be stopped (after start)",
          "[when_any]") {
    int value{};
    [[maybe_unused]] auto w = async::when_any();
    auto r = only_stoppable_receiver{[&] { value = 42; }};
    static_assert(
        std::same_as<async::completion_signatures_of_t<
                         decltype(w), async::env_of_t<decltype(r)>>,
                     async::completion_signatures<async::set_stopped_t()>>);

    auto op = async::connect(w, r);
    op.start();
    CHECK(value == 0);

    r.request_stop();
    CHECK(value == 42);
}

TEST_CASE("when_any nests", "[when_any]") {
    [[maybe_unused]] auto w = async::when_any(async::when_any());
    [[maybe_unused]] auto op = async::connect(w, receiver{[] {}});
}
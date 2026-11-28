#include <catch2/catch_test_macros.hpp>

#include "app.h"

using namespace wordlock;

namespace {

struct SettingsGuard {
    AppSettings settings = g_settings;
    int nextIndex = g_nextTaskIndex;

    ~SettingsGuard() {
        g_settings = settings;
        g_nextTaskIndex = nextIndex;
    }
};

void EnableOnly(TaskType task) {
    g_settings.tasks = TaskSettings{};
    g_settings.tasks.englishWord = task == TaskType::English;
    g_settings.tasks.multipleChoice = task == TaskType::Mcq;
    g_settings.tasks.japaneseWord = task == TaskType::Japanese;
    g_settings.tasks.dumbbellWorkout = task == TaskType::Dumbbell;
}

}  // namespace

TEST_CASE("GetNextTaskType cycles through enabled tasks", "[unit][tasks]") {
    SettingsGuard guard;
    g_nextTaskIndex = 0;

    REQUIRE(GetNextTaskType() == TaskType::English);
    REQUIRE(GetNextTaskType() == TaskType::Mcq);
    REQUIRE(GetNextTaskType() == TaskType::Japanese);
    REQUIRE(GetNextTaskType() == TaskType::Dumbbell);
    REQUIRE(GetNextTaskType() == TaskType::English);
    REQUIRE(g_nextTaskIndex == 5);
}

TEST_CASE("GetNextTaskType skips disabled tasks", "[unit][tasks]") {
    SettingsGuard guard;
    EnableOnly(TaskType::Japanese);
    g_nextTaskIndex = 7;

    REQUIRE(GetNextTaskType() == TaskType::Japanese);
    REQUIRE(GetNextTaskType() == TaskType::Japanese);
}

TEST_CASE("GetNextTaskType falls back to English when nothing is enabled",
          "[unit][tasks]") {
    SettingsGuard guard;
    g_settings.tasks.englishWord = false;
    g_settings.tasks.multipleChoice = false;
    g_settings.tasks.japaneseWord = false;
    g_settings.tasks.dumbbellWorkout = false;
    g_nextTaskIndex = 3;

    REQUIRE(GetNextTaskType() == TaskType::English);
    REQUIRE(g_nextTaskIndex == 3);  // No task was consumed.
}

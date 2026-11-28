#include "app.h"

namespace wordlock {

TaskType GetNextTaskType() {
    std::vector<TaskType> enabled;
    if (g_settings.tasks.englishWord) enabled.push_back(TaskType::English);
    if (g_settings.tasks.multipleChoice) enabled.push_back(TaskType::Mcq);
    if (g_settings.tasks.japaneseWord) enabled.push_back(TaskType::Japanese);
    if (g_settings.tasks.dumbbellWorkout) enabled.push_back(TaskType::Dumbbell);

    if (enabled.empty()) {
        return TaskType::English;
    }

    const TaskType next = enabled[static_cast<size_t>(g_nextTaskIndex) % enabled.size()];
    g_nextTaskIndex = (g_nextTaskIndex + 1) % 1000000;
    return next;
}

}  // namespace wordlock

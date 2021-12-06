// tasklib3.cpp : Defines the entry point for the application.
//

#include <tasklib3.h>

using namespace std;

void test_task() {
	this_thread::sleep_for(chrono::microseconds(50 + rand() % 1000));
}

TaskSet make_test_set() {
	auto MAX_TASKS = 20;
	auto MIN_TASKS = 10;
	auto num_tasks = MIN_TASKS + rand() % (MAX_TASKS - MIN_TASKS);

	// make random names
	auto names = vector<string> {};
	for (int i = 0; i < num_tasks; i++) {
		names.emplace_back("task." + to_string(i));
	}

	auto builder = TaskSetBuilder();
	for (int i = 0; i < num_tasks; i++) {
		// can depend on previous tasks with 
		auto deps = unordered_set<string> {};
		for (int j = 0; j < i - 1; j++) {
			if (rand() % 100 < 50) {
				deps.insert(names[j]);
			}
		}

		builder.add(names[i], deps, test_task);
	}

	return builder.build();
}

int i1 = 0;
int i2 = 0;
int i3 = 0;
int result = 0;

void task1() {
	i1 = 1 * 1;
}
void task2() {
	i2 = 2 * 2;
}
void task3() {
	i3 = 3 * 3;
}
void dependentTask() {
	result = i1 + i2 + i3;
}

TaskSet sdf() {
	return TaskSetBuilder()
		.add("Task1", {}, task1)
		.add("Task2", {}, task2)
		.add("Task3", {}, task3)
		.add("FinalCalc", { "Task1", "Task2", "Task3" }, dependentTask)
		.build();
}

int main() {
	auto engine = unique_ptr<TaskEngine>(new TaskEngine(7));

	for (int i = 0; i < 100; i++) {
		auto set = make_test_set();
		cout << "running test: " << i << "...";
		engine->run(set);
		cout << "done" << endl;
	}

	engine.reset();

	printf("press enter to continue...\n");
	getchar();

	return 0;
}

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

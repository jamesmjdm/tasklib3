# tasklib3

A simple parallel work queue implementation
	- The standalone project is cmake based
	- Very small amount of code and only depends on the standard library
	- Can be dropped easily into any project

Allows to group "tasks" declaratively into a "workflow" with dependency relationships, and run workflow in parallel with a thread pool

Improved version of tasklib2
	- The task queue is now just a `vector` and uses atomic_uint to determine the next task - no locking and no allocations while tasks are running which would require "stop the world" on task runners
	- `atomic_flag` and the wrapper `simple_flag` used as much as possible, this is guaranteed to be lock-free unlike the semaphore implementation in tasklib2. Also simplified the code a lot
	- The engine class `run` method runs tasks as well and blocks until all tasks are complete, this is more intuitive and probably easier to code against

Other changes:
	- `Workflow` and `WorkflowBuilder` renamed to `TaskSet` and `TaskSetBuilder`
	- Snake case now
	- Some methods renamed
	- When constructing TaskEngine, if you want to run tasks on N threads, pass N-1 as the number of worker threads because the calling thread will do work in run()


### Example code

Runs three functions to calculate 3 intermediate results, then runs a final calculation using the 3 intermediate results

The final calculation will not run until all intermediate results are ready. This is achieved by marking the "Final" task as dependent on the other 3

The `TaskSetBuilder` class topologically sorts the subtasks that are added, so no task will be executed before any of its dependencies

```
#include <iostream>
#include "tasklib3.h"
using namespace std;

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


int main(int argc, char* argv[]) {
	auto task_set = TaskSetBuilder()
		.add("Task1", {}, task1)
		.add("Task2", {}, task2)
		.add("Task3", {}, task3)
		.add("FinalCalc", { "Task1", "Task2", "Task3" }, dependentTask)
		.build();

    auto engine = unique_ptr<TaskEngine>(3); // N-1 background threads
    engine->run(task_set);
	cout << "Result: " << result << endl;
    return 0;
}

```


### why?

- several low hanging fruit worth picking such as using atomics instead of the (rather pedestrian) locking concurrent queue in tasklib3
- tasklib2 was intended for a directx11 renderer, which needs the main thread to be available when run() is called as there was a distinction between immediate and deferred contexts
- tasklib3 is intended for use in a vulkan renderer, which does not need special privileges for the main thread so it is better for the main thread to behave like an extra worker thread when run() is called
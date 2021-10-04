#include "tasklib3.h"

#include <sstream>

simple_flag::simple_flag() {
	flag.store(false);
}

simple_flag::simple_flag(simple_flag&& s) noexcept {
	flag.store(s.flag.load());
}
simple_flag& simple_flag::operator=(simple_flag&& s) noexcept {
	flag.store(s.flag.load()); return *this;
}

void simple_flag::wait() {
	auto lock = unique_lock(mtx);
	while (!flag.load()) {
		cv.wait(lock);
	}
}
void simple_flag::set() {
	//auto lock = unique_lock(mtx);
	flag.store(true);
	cv.notify_all();
}
void simple_flag::clear() {
	auto lock = unique_lock(mtx);
	flag.store(false);
}


Task::Task(const string& name, vector<size_t>&& deps, const TaskFunction& tf): name(name), dependencies(deps), func(tf) {}
TaskSet::TaskSet(vector<Task>&& tasks) : tasks(tasks) {}

TaskSetBuilder& TaskSetBuilder::add(const string& name, const unordered_set<string>& deps, const TaskFunction& func) {
	if (tasks.find(name) != tasks.end()) {
		throw runtime_error("task already exists: " + name);
	}

	// insert into the tasks map
	tasks.emplace(pair(name, func));

	// insert into the dependencies
	forward_deps.emplace(name, deps);

	// insert into reverse dependencies
	for (const auto& d : deps) {
		reverse_deps[d].emplace(name);
	}

	return *this;
}

TaskSet TaskSetBuilder::build() const {
	// topological sort! from wikipedia
	// https://en.wikipedia.org/wiki/Topological_sorting
	// kahn's algo simpler to implement and probably faster than recursive approach
	// recursive approach can be used to log detected cycles, kahn's algo implies but does not pinpoint

	/*
	L - Empty list that will contain the sorted elements
	S - Set of all nodes with no dependencies

	while S is not empty do
		remove a node n from S
		add n to L
		for each node m that depends on n
			remove dependency on n from m
			if m has no other dependencies then
				insert m into S

	if graph has edges then
		return error   (graph has at least one cycle)
	else
		return L   (a topologically sorted order)
	*/

	// check reverse deps for missing tasks
	for (const auto& r : reverse_deps) {
		if (tasks.find(r.first) == tasks.end()) {
			auto ss = stringstream {};
			ss << "unknown task '" << r.first << "'";
			if (r.second.size()) {
				auto iter = r.second.begin();
				ss << " dependency of '" << *(iter++);
				for (; iter != r.second.end(); iter++) {
					ss << "', '" << *iter;
				}
				ss << "'";
			}
			throw runtime_error(ss.str());
		}
	}

	auto final_index = unordered_map<string, size_t> {}; final_index.reserve(tasks.size()); // name to final index
	auto fd = forward_deps; // a copy that can be modified
	auto sorted = vector<Task> {}; sorted.reserve(tasks.size()); // the final sorted list
	auto roots = vector<pair<string, TaskFunction>> {}; roots.reserve(tasks.size());
	auto processed_count = 0;

	// construct the initial roots list
	for (const auto& [n, f] : tasks) {
		if (fd[n].empty()) {
			roots.emplace_back(n, f);
		}
	}

	while (processed_count < tasks.size()) {
		// remove node from S
		// const auto& t = roots[processed_count++];
		if (processed_count >= roots.size()) {
			throw runtime_error("Cycle detected!");
		}
		const auto& node = roots[processed_count++];

		// crystallize the dependencies
		const auto& node_fd = forward_deps.find(node.first)->second; // can't use [] because non-const; don't use fd because it gets erased!
		auto deps = vector<size_t> {}; deps.reserve(node_fd.size());
		for (const auto& d : node_fd) {
			deps.emplace_back(final_index[d]);
		}

		// add task to sorted list
		final_index[node.first] = sorted.size();
		sorted.emplace_back(node.first, move(deps), node.second);

		// for each node m that depends on n (if any)
		const auto& rd = reverse_deps.find(node.first);
		if (rd != reverse_deps.end()) {
			for (const auto& m : rd->second) {
				// remove dependency on n from m
				auto& m_fd = fd[m];
				m_fd.erase(node.first);
				// if m has no other dependencies, insert into roots
				if (m_fd.size() == 0) {
					roots.emplace_back(m, tasks.at(m));
				}
			}
		}
	}

	return TaskSet(move(sorted));
}


TaskEngine::TaskEngine(size_t num_threads) {
	for (auto i = 0; i < num_threads; i++) {
		threads.emplace_back(&TaskEngine::thread_main, this, i+1);
	}
}
TaskEngine::~TaskEngine() {

	// push one "exit" task per worker thread so worker threads progress
	should_exit = true;
	add_tasks(TaskSet(move(vector<Task>(threads.size(), { "exit", move(vector<size_t>{}), [] {
		printf("EXIT TASK\n");
	} }))));

	// wait for worker threads to exit (no need to wait for exit task completion, it's implicit)
	for (auto& t: threads) {
		t.join();
	}
}

void TaskEngine::add_tasks(const TaskSet& task_set) {
	// TODO: mutex and cvar creation is expensive, see if we can reuse/pool
	// rather than continually destroying and recreating as here
	task_queue.clear();
	for (const auto& t : task_set.tasks) {
		task_queue.emplace_back(t.func, t.dependencies);
	}

	queue_pos.store(0);
	has_tasks.set();
}

void TaskEngine::run(const TaskSet& task_set) {
	add_tasks(task_set);
	
	// run some tasks on this thread
	while (true) {
		auto task_index = queue_pos.fetch_add(1);
		if (task_index < task_queue.size()) {
			do_task(task_index);
		} else {
			has_tasks.clear();
			break;
		}
	}

	// wait for all tasks to complete
	for (auto i = 0; i < task_queue.size(); i++) {
		// wait_for_task_complete(i);
		task_queue[i].is_complete.wait();
	}
}

void TaskEngine::thread_main(size_t thread_id) {
	// thread main is also called from run()
	// so if thread_id == 0, we are in the calling thread
	// (probably not important)

	while (!should_exit) {
		// wait for tasks to appear
		has_tasks.wait();
		auto task_index = queue_pos.fetch_add(1);
		if (task_index < task_queue.size()) {
			do_task(task_index);
		} else {
			has_tasks.clear();
		}
	}
}

void TaskEngine::do_task(size_t task_index) {
	auto& task = task_queue[task_index];

	// wait for dependencies
	for (auto s : task.dependencies) {
		task_queue[s].is_complete.wait();
	}

	// run task
	if (task.func) {
		task.func();
	}

	// notify threads that depend on this task
	task.is_complete.set();
}

TaskEngine::QueueTask::QueueTask(const TaskFunction& func, const vector<size_t>& deps): func(func), dependencies(deps) {}

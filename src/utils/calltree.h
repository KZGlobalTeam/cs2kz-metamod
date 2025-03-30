#include <iostream>
#include <string>
#include <vector>
#include <stack>
#include <set>
#include <map>

class FunctionTracker
{
private:
	struct FunctionNode
	{
		std::string name;
		int depth;
		int call_count;
		std::vector<FunctionNode *> children;

		FunctionNode() : call_count(0) {}
	};

	std::vector<FunctionNode *> roots;
	std::stack<FunctionNode *> call_stack;
	std::map<std::string, FunctionNode *> unique_nodes;
	bool enabled;

public:
	FunctionTracker() : enabled(true) {}

	~FunctionTracker()
	{
		clear();
	}

	class ScopedTracker
	{
	private:
		FunctionTracker &tracker;
		std::string func_name;
		bool active;

	public:
		ScopedTracker(FunctionTracker &t, const std::string &name) : tracker(t), func_name(name), active(true)
		{
			tracker.enter_function(func_name);
		}

		~ScopedTracker()
		{
			if (active)
			{
				tracker.exit_function(func_name);
			}
		}
	};

	void enter_function(const std::string &func_name)
	{
		if (!enabled)
		{
			return;
		}

		FunctionNode *node;
		std::string path = get_current_path() + func_name;

		auto it = unique_nodes.find(path);
		if (it != unique_nodes.end())
		{
			// Reuse existing node
			node = it->second;
		}
		else
		{
			// Create new node
			node = new FunctionNode();
			node->name = func_name;
			node->depth = call_stack.size();
			unique_nodes[path] = node;

			if (!call_stack.empty())
			{
				FunctionNode *parent = call_stack.top();
				// Check if this function is already a child
				bool exists = false;
				for (auto child : parent->children)
				{
					if (child->name == func_name)
					{
						exists = true;
						break;
					}
				}
				if (!exists)
				{
					parent->children.push_back(node);
				}
			}
			else
			{
				// Check if this root already exists
				bool exists = false;
				for (auto root : roots)
				{
					if (root->name == func_name)
					{
						exists = true;
						break;
					}
				}
				if (!exists)
				{
					roots.push_back(node);
				}
			}
		}

		// Increment call count
		node->call_count++;

		call_stack.push(node);
	}

	std::string get_current_path()
	{
		if (call_stack.empty())
		{
			return "";
		}

		std::string path;
		std::stack<FunctionNode *> temp_stack = call_stack;
		std::vector<std::string> names;

		while (!temp_stack.empty())
		{
			names.push_back(temp_stack.top()->name);
			temp_stack.pop();
		}

		for (auto it = names.rbegin(); it != names.rend(); ++it)
		{
			path += *it + ">";
		}

		return path;
	}

	void exit_function(const std::string &func_name)
	{
		if (!enabled)
		{
			return;
		}

		if (call_stack.empty())
		{
			printf("Error: Function exit without matching entry: %s\n", func_name.c_str());
			return;
		}

		FunctionNode *node = call_stack.top();
		call_stack.pop();

		if (node->name != func_name)
		{
			printf("Warning: Function exit mismatch. Expected %s but got %s\n", node->name.c_str(), func_name.c_str());
		}
	}

	void enable()
	{
		enabled = true;
	}

	void disable()
	{
		enabled = false;
	}

	void clear()
	{
		// Free memory only for unique nodes
		for (auto &pair : unique_nodes)
		{
			delete pair.second;
		}

		roots.clear();
		unique_nodes.clear();
		while (!call_stack.empty())
		{
			call_stack.pop();
		}
	}

	void reset()
	{
		// Keep the data structure but reset all counts
		for (auto &pair : unique_nodes)
		{
			pair.second->call_count = 0;
		}

		while (!call_stack.empty())
		{
			call_stack.pop();
		}
		printf("Function call counts reset\n");
	}

	void print_node(FunctionNode *node, int depth, std::set<FunctionNode *> &visited)
	{
		if (!node || visited.find(node) != visited.end())
		{
			return;
		}

		visited.insert(node);
		printf("%s%s [%d calls]\n", std::string(depth * 2, ' ').c_str(), node->name.c_str(), node->call_count);

		for (auto child : node->children)
		{
			print_node(child, depth + 1, visited);
		}
	}

	void print_summary()
	{
		printf("\n===== Function Call Summary =====\n");

		if (roots.empty())
		{
			printf("No functions tracked\n");
			return;
		}

		printf("Function call hierarchy (unique paths with call counts):\n");

		std::set<FunctionNode *> visited;
		for (auto root : roots)
		{
			print_node(root, 0, visited);
		}
	}
};

// Global instance for easy access
inline FunctionTracker &GetTracker()
{
	static FunctionTracker instance;
	return instance;
}

// Macro to easily track functions
#define TRACK_FUNCTION()           FunctionTracker::ScopedTracker _function_tracker_instance(GetTracker(), __func__)
#define TRACK_FUNCTION_STR(name)   FunctionTracker::ScopedTracker _function_tracker_instance(GetTracker(), name)
#define TRACK_FUNCTION_NAMED(name) FunctionTracker::ScopedTracker _function_tracker_instance(GetTracker(), #name)

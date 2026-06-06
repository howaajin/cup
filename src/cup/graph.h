#pragma once

#include "core/allocator.h"
#include "cup/node.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct Node Node;
typedef struct Graph Graph;
typedef struct PtrHash PtrHash;
typedef struct Set Set;
typedef struct StringHash StringHash;
typedef char const* FnOutputFilter(Node* cmd, Allocator* allocator, char const* line);
typedef bool FnCheckDirty(Node* node);
typedef void FnVisitNode(Node* node);
typedef void FnBeforeExecute(Node* node);
typedef void FnAfterExecute(Node* node);

struct Graph
{
    Allocator* allocator;
    Node** sources;
    PtrHash* hash_node_to_next_set;
    PtrHash* hash_node_to_prev_set;
    PtrHash* hash_node_to_b_finished;
    Node** stack;
};

Graph* graph_create(Allocator* allocator, Node** targets, size_t num_targets);
void graph_destroy(Graph* graph);
void graph_add_target(Graph* graph, Node* node);
void graph_add_dynamic_edge(Graph* graph, Node* tail, Node* head);
void graph_set_node_processed(Graph* graph, Node* node);
Node* graph_pop(Graph* graph);
Node** graph_get_unreachable_nodes(Graph* graph, Allocator* allocator);
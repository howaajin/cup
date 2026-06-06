#include "cup/graph.h"
#include "core/array.h"
#include "core/hash.h"
#include "core/os.h"
#include "cup/cache.h"

extern Node** nodes;

extern Allocator* node_allocator;

Graph* graph_create(Allocator* allocator, Node** targets, size_t num_targets)
{
    Graph* graph = allocator_calloc(allocator, 1, sizeof(Graph));
    graph->allocator = allocator;
    graph->sources = NULL;
    graph->hash_node_to_next_set = allocator_calloc(allocator, 1, sizeof(PtrHash));
    graph->hash_node_to_prev_set = allocator_calloc(allocator, 1, sizeof(PtrHash));
    graph->hash_node_to_b_finished = allocator_calloc(allocator, 1, sizeof(PtrHash));
    graph->hash_node_to_next_set->allocator = allocator;
    graph->hash_node_to_prev_set->allocator = allocator;
    graph->hash_node_to_b_finished->allocator = allocator;

    for (size_t i = 0; i != num_targets; i++)
    {
        graph_add_target(graph, targets[i]);
    }
    return graph;
}

static void graph_free_set(PtrHash* h)
{
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        Set* set = (Set*)hash_value(h, i);
        hash_free(set);
    }
}

void graph_destroy(Graph* graph)
{
    array_free(graph->allocator, graph->sources);
    array_free(graph->allocator, graph->stack);
    graph_free_set(graph->hash_node_to_next_set);
    graph_free_set(graph->hash_node_to_prev_set);
    hash_free(graph->hash_node_to_next_set);
    hash_free(graph->hash_node_to_prev_set);
    hash_free(graph->hash_node_to_b_finished);
    allocator_free(graph->allocator, graph);
}

static PtrHash* graph_get_node_next_set(Graph* graph, Node* node)
{
    uint32_t i = hash_insert(graph->hash_node_to_next_set, node);
    PtrHash* next_set = hash_value(graph->hash_node_to_next_set, i);
    if (next_set == NULL)
    {
        next_set = allocator_calloc(graph->allocator, 1, sizeof(PtrHash));
        next_set->allocator = graph->allocator;
        hash_value(graph->hash_node_to_next_set, i) = next_set;
    }
    return next_set;
}

static PtrHash* graph_get_node_prev_set(Graph* graph, Node* node)
{
    uint32_t i = hash_insert(graph->hash_node_to_prev_set, node);
    PtrHash* prev_set = (PtrHash*)hash_value(graph->hash_node_to_prev_set, i);
    if (prev_set == NULL)
    {
        prev_set = allocator_calloc(graph->allocator, 1, sizeof(PtrHash));
        prev_set->allocator = graph->allocator;
        hash_value(graph->hash_node_to_prev_set, i) = prev_set;
    }
    return prev_set;
}

void graph_add_target(Graph* graph, Node* node)
{
    array_resize(graph->allocator, graph->stack, 0);
    array_push(graph->allocator, graph->stack, node);
    while (array_size(graph->stack))
    {
        Node* next = graph->stack[0];
        array_remove_unordered(graph->stack, 0);
        bool b_existed;
        hash_insert_check(graph->hash_node_to_b_finished, next, &b_existed);
        if (b_existed)
        {
            continue;
        }
        PtrHash* prev_set = graph_get_node_prev_set(graph, next);
        size_t num_prev = array_size(next->dependencies);
        for (size_t i = 0; i != num_prev; i++)
        {
            Node* prev = next->dependencies[i];
            uint32_t j = hash_index(graph->hash_node_to_b_finished, prev);
            if (j == HASH_INVALID_INDEX || hash_value(graph->hash_node_to_b_finished, j) == false)
            {
                array_push(graph->allocator, graph->stack, prev);
                hash_insert(prev_set, prev);
                PtrHash* next_set = graph_get_node_next_set(graph, prev);
                hash_insert(next_set, next);
            }
        }
        if (hash_size(prev_set) == 0 && !next->b_dynamic_indegree)
        {
            array_push(graph->allocator, graph->sources, next);
        }
    }
}

void graph_add_dynamic_edge(Graph* graph, Node* tail, Node* head)
{
    PtrHash* head_next_set = graph_get_node_next_set(graph, head);
    hash_insert(head_next_set, tail);
    PtrHash* tail_prev_set = graph_get_node_prev_set(graph, tail);
    hash_insert(tail_prev_set, head);
    graph_add_target(graph, head);
}

void graph_set_node_processed(Graph* graph, Node* node)
{
    hash_put(graph->hash_node_to_b_finished, node, (void*)(uintptr_t)true);
    PtrHash* next_set = hash_get(graph->hash_node_to_next_set, node);
    if (!next_set)
    {
        return;
    }
    for (uint32_t i = next_set->begin; i != next_set->end; i = hash_next(next_set, i))
    {
        Node* next = (Node*)hash_key(next_set, i);
        PtrHash* prev_set = hash_get(graph->hash_node_to_prev_set, next);
        uint32_t j = hash_index(prev_set, node);
        if (j != HASH_INVALID_INDEX)
        {
            hash_remove(prev_set, j);
        }
        if (hash_size(prev_set) == 0)
        {
            array_push(graph->allocator, graph->sources, next);
        }
    }
    hash_reset(next_set);
}

Node* graph_pop(Graph* graph)
{
    if (array_size(graph->sources) == 0)
    {
        return NULL;
    }
    Node* node = graph->sources[0];
    array_remove_unordered(graph->sources, 0);
    return node;
}

Node** graph_get_unreachable_nodes(Graph* graph, Allocator* allocator)
{
    Node** unreachable_nodes = NULL;
    PtrHash* h = graph->hash_node_to_b_finished;
    for (uint32_t i = h->begin; i != h->end; i = hash_next(h, i))
    {
        bool b_finished = (bool)(uintptr_t)hash_value(h, i);
        Node* node = hash_key(h, i);
        if (!b_finished)
        {
            array_push(allocator, unreachable_nodes, node);
        }
    }
    return unreachable_nodes;
}
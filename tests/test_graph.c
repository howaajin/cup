
#include "core/hash.h"
#include "cup/graph.h"
#include "cup/test.h"

TEST(test_graph_dynamic_target, graph)
{
    Allocator* allocator = allocator_create_chained();

    Graph* graph = graph_create(allocator, NULL, 0);

    Node* a = node_create(NODE_TYPE_VIRTUAL, NULL, sizeof(Node));
    Node* b = node_create(NODE_TYPE_VIRTUAL, NULL, sizeof(Node));
    Node* c = node_create(NODE_TYPE_VIRTUAL, NULL, sizeof(Node));

    node_add_dependency(b, a);
    node_add_dependency(c, b);

    Node* node;
    node = graph_pop(graph);
    ASSERT(node == NULL);

    graph_add_target(graph, a);
    node = graph_pop(graph);
    graph_set_node_processed(graph, node);
    ASSERT(node == a);

    graph_add_target(graph, c);
    node = graph_pop(graph);
    graph_set_node_processed(graph, node);
    ASSERT(node == b);

    node = graph_pop(graph);
    graph_set_node_processed(graph, node);
    ASSERT(node == c);

    node = graph_pop(graph);
    ASSERT(node == NULL);
}

TEST(test_modify_dag_dynamic, graph)
{
    Allocator* allocator = allocator_create_chained();
    Graph* graph = graph_create(allocator, NULL, 0);

    Node* virtual = node_create(NODE_TYPE_VIRTUAL, NULL, sizeof(Node));
    virtual->b_dynamic_indegree = true;
    Node* a = node_create(NODE_TYPE_VIRTUAL, NULL, sizeof(Node));

    node_add_dependency(a, virtual);

    Node* node;
    graph_add_target(graph, a);
    node = graph_pop(graph);
    ASSERT(node == NULL);

    Node* b = node_create(NODE_TYPE_VIRTUAL, NULL, sizeof(Node));
    graph_add_dynamic_edge(graph, a, b);

    graph_set_node_processed(graph, virtual);
    node = graph_pop(graph);
    ASSERT(node == b);
    graph_set_node_processed(graph, b);

    node = graph_pop(graph);
    graph_set_node_processed(graph, node);
    ASSERT(node == a);

    node = graph_pop(graph);
    ASSERT(node == NULL);
}

TEST(graph_empty, graph)
{
    Allocator* allocator = allocator_create_chained();
    Graph* graph = graph_create(allocator, NULL, 0);
    ASSERT(graph_pop(graph) == NULL);
    allocator_destroy(allocator);
}

TEST(graph_single_target_no_deps, graph)
{
    Allocator* allocator = allocator_create_chained();
    Graph* graph = graph_create(allocator, NULL, 0);
    Node* t = FILE("single_target_no_deps");
    graph_add_target(graph, t);
    ASSERT(graph_pop(graph) == t);
    ASSERT(graph_pop(graph) == NULL);
    allocator_destroy(allocator);
}

TEST(graph_linear, graph)
{
    Allocator* allocator = allocator_create_chained();
    Graph* graph = graph_create(allocator, NULL, 0);
    Node* a = FILE("linear_a");
    Node* b = FILE("linear_b");

    node_add_dependency(b, a);
    graph_add_target(graph, b);
    ASSERT(graph_pop(graph) == a);
    graph_set_node_processed(graph, a);
    ASSERT(graph_pop(graph) == b);
    graph_set_node_processed(graph, b);
    ASSERT(graph_pop(graph) == NULL);
    allocator_destroy(allocator);
}

TEST(graph_target_with_no_deps_added_directly, graph)
{
    Allocator* allocator = allocator_create_chained();
    Graph* graph = graph_create(allocator, NULL, 0);
    Node* a = FILE("source_target");
    graph_add_target(graph, a);
    ASSERT(graph_pop(graph) == a);
    graph_set_node_processed(graph, a);
    ASSERT(graph_pop(graph) == NULL);
    allocator_destroy(allocator);
}

TEST(graph_diamond, graph)
{
    Allocator* allocator = allocator_create_chained();
    Graph* graph = graph_create(allocator, NULL, 0);
    Node* a = FILE("diamond_a");
    Node* b = FILE("diamond_b");
    Node* c = FILE("diamond_c");
    Node* d = FILE("diamond_d");
    node_add_dependency(b, a);
    node_add_dependency(c, a);
    node_add_dependency(d, b);
    node_add_dependency(d, c);
    graph_add_target(graph, d);
    ASSERT(graph_pop(graph) == a);
    graph_set_node_processed(graph, a);
    Node* x = graph_pop(graph);
    Node* y = graph_pop(graph);
    ASSERT((x == b && y == c) || (x == c && y == b));
    graph_set_node_processed(graph, x);
    graph_set_node_processed(graph, y);
    ASSERT(graph_pop(graph) == d);
    graph_set_node_processed(graph, d);
    ASSERT(graph_pop(graph) == NULL);
    allocator_destroy(allocator);
}

TEST(graph_multiple_sources, graph)
{
    Allocator* allocator = allocator_create_chained();
    Graph* graph = graph_create(allocator, NULL, 0);
    Node* a = FILE("ms_a");
    Node* b = FILE("ms_b");
    Node* c = FILE("ms_c");
    graph_add_target(graph, a);
    graph_add_target(graph, b);
    graph_add_target(graph, c);
    Set set = {.allocator = allocator_temp()};
    hash_insert(&set, (uintptr_t)a);
    hash_insert(&set, (uintptr_t)b);
    hash_insert(&set, (uintptr_t)c);

    uint32_t i;
    i = hash_index(&set, (uintptr_t)graph_pop(graph));
    ASSERT(i != HASH_INVALID_INDEX);
    hash_remove(&set, i);
    i = hash_index(&set, (uintptr_t)graph_pop(graph));
    ASSERT(i != HASH_INVALID_INDEX);
    hash_remove(&set, i);
    i = hash_index(&set, (uintptr_t)graph_pop(graph));
    ASSERT(i != HASH_INVALID_INDEX);
    hash_remove(&set, i);
    ASSERT(graph_pop(graph) == NULL);
    allocator_destroy(allocator);
}

TEST(graph_set_target_processed_not_in_graph, graph)
{
    Allocator* allocator = allocator_create_chained();
    Graph* graph = graph_create(allocator, NULL, 0);
    Node* a = FILE("not_in_graph");
    graph_set_node_processed(graph, a);
    ASSERT(graph_pop(graph) == NULL);
    allocator_destroy(allocator);
}

TEST(graph_add_target_after_processing, graph)
{
    Allocator* allocator = allocator_create_chained();
    Graph* graph = graph_create(allocator, NULL, 0);
    Node* a = FILE("add_after_a");
    Node* b = FILE("add_after_b");
    node_add_dependency(b, a);
    graph_add_target(graph, b);
    ASSERT(graph_pop(graph) == a);
    graph_set_node_processed(graph, a);
    ASSERT(graph_pop(graph) == b);
    graph_set_node_processed(graph, b);
    Node* c = FILE("add_after_c");
    node_add_dependency(b, c);
    graph_add_target(graph, c);
    ASSERT(graph_pop(graph) == c);
}

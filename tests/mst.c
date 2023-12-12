#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Vertex {
	int id;
	int neighborCount;
    struct Vertex* neighbors;
	int parentId;
};

struct Edge {
	int srcId;
	int dstId;
};

struct Graph {
	int vertexCount;
	int edgeCount;
    struct Vertex* vertices;
	struct Edge* edges;
};

struct Graph* createGraph(int vertexCount, int edgeCount) {
	struct Graph* graph = (struct Graph*) malloc(sizeof(struct Graph));
	graph->vertexCount = vertexCount;

	graph->vertices = (struct Vertex*) malloc(graph->vertexCount * sizeof(struct Vertex));
	graph->edges = (struct Edge*) malloc(graph->edgeCount * sizeof(struct Edge));

	return graph;
}

// subset for union-find
struct subset {
	int parent;
	int rank;
};

int find(struct subset subsets[], int i) {
	if (subsets[i].parent != i) {
		subsets[i].parent = find(subsets, subsets[i].parent);
	}

	return subsets[i].parent;
}

void Union(struct subset subsets[], int x, int y) {
	int xroot = find(subsets, x);
	int yroot = find(subsets, y);

	if (subsets[xroot].rank < subsets[yroot].rank) {
		subsets[xroot].parent = yroot;
	}
	else if (subsets[xroot].rank > subsets[yroot].rank) {
		subsets[yroot].parent = xroot;
	}
	else {
		subsets[yroot].parent = xroot;
		subsets[xroot].rank++;
	}
}

void constructEdge(struct Graph* graph, struct Vertex* v, int* numEdges, int currNode) {
	if (currNode == v->neighborCount) {
		return;
	}

	struct Edge edge;
	edge.srcId = v->id;
	edge.dstId = v->neighbors[currNode].id;
	graph->edges[*numEdges] = edge;
	++(*numEdges);
	constructEdge(graph, v, numEdges, currNode + 1);
}

void KruskalMST(struct Graph* graph) {
	int vertexCount = graph->vertexCount;
	struct Edge result[vertexCount];

	// allocate
	struct subset *subsets = (struct subset*) malloc(vertexCount * sizeof(struct subset));

	int x = 0;
	int* numEdges = &x;
	for (int i = 0 ; i < vertexCount; i++) {
		constructEdge(graph, &graph->vertices[i], numEdges, 0);
	}

	
	// ID --> V
	for (int id = 0; id < vertexCount; ++id) {
		subsets[id].parent = id;
		subsets[id].rank = 0;
	}




	
	int e = 0; 
	int i = 0;
	while (e < vertexCount - 1) {
		struct Edge next_edge = graph->edges[i++];
		

		int x = find(subsets, next_edge.srcId);
		int y = find(subsets, next_edge.dstId);

		if (x != y) {
			result[e++] = next_edge;
			Union(subsets, x, y);
		}
	}

	
	printf("MST Constructed with the following Edges:\n");
	for (i = 0; i < e; ++i) {
		printf("%d -- %d\n", result[i].srcId, result[i].dstId);
	}
	return;
}

// Driver program to test above functions
int main() {
	int vertexCount = 4;
	int edgeCount = 4;
	struct Graph* graph = createGraph(vertexCount, edgeCount);

	// add edge 0-1
	graph->vertices[0].id = 0;
	graph->vertices[1].id = 1;
	graph->vertices[2].id = 2;
	graph->vertices[3].id = 3;

	graph->vertices[0].neighbors = &graph->vertices[1];
	struct Vertex* temp = ++graph->vertices[0].neighbors;
	temp = &graph->vertices[3];

	graph->vertices[1].neighbors = &graph->vertices[2];

	graph->vertices[1].neighbors = &graph->vertices[3];

	graph->vertices[0].neighborCount = 2;
	graph->vertices[0].neighborCount = 2;

	KruskalMST(graph);
	return 0;
}
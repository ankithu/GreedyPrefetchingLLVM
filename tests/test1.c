#include <stdio.h>
#include <stdlib.h>

const unsigned int MAX_MESSAGE_SIZE = 1024;
const unsigned int MAX_NODES = 100000;

typedef struct Data {
  int size;
  char message[MAX_MESSAGE_SIZE];
  int flag; //just some data for examples sake
} Data;

// Example with a Heterogenous k-ary RDS (may contain cycles)
typedef struct DataNode {
  struct DataNode* children[10];
  Data* data;
  unsigned int index; //for easy visited checking
  unsigned int numChildren;
} DataNode;

//almost definitely leaks (unless generated graph has exactly 1 component)
//its ok though its just an example (just don't use really large n and expect to
//be able to have lots of free memory afterwards)
DataNode* getRandomDataLayout(unsigned int n) {
  DataNode* nodes[n];
  for (unsigned int i = 0; i < n; ++i){
    nodes[i] = malloc(sizeof(DataNode));
    nodes[i]->data = malloc(sizeof(Data));
    nodes[i]->index = i;
    nodes[i]->data->size = 1;
    nodes[i]->data->message[0] = i;
    nodes[i]->data->flag = i;
  }
  for (unsigned int i = 0; i < n; ++i){
    unsigned int numChildren = 1 + (rand() % 10);
    for (size_t j = 0; j < numChildren; ++j){
      unsigned int childNum = rand() % n;
      nodes[i]->children[j] = nodes[childNum];
    }
    nodes[i]->numChildren = numChildren;
  }
  return nodes[0];
}

void traverseDataLayout(DataNode* root, int visited[MAX_NODES]){
  if (!root){
    return;
  }
  srand(100);
  printf("%d", root->data->flag);
  for (size_t i = 0; i < root->numChildren; ++i){
    if (!visited[root->children[i]->index]){
      visited[root->children[i]->index] = 1;
      traverseDataLayout(root->children[i], visited);
    }
  }
}


int main()
{
  DataNode* dl = getRandomDataLayout(MAX_NODES);
  int* visited = malloc(sizeof(int)*MAX_NODES);
  for (unsigned int i = 0; i < MAX_NODES; ++i){
    visited[i] = 0;
  }
  traverseDataLayout(dl, visited);
  return 0;
}

#include <stdio.h>

static const unsigned int CHILDREN = 10;

static const unsigned int PAGE_SIZE = 4096;

typedef struct Node {
  int val;
  struct Node* children[CHILDREN];
} Node;

void leakMemory(unsigned int bytes){
  malloc(bytes);
}

Node* buildTree(unsigned int size){
  Node* q[size + 1];
  unsigned int tailPtr = 1;
  q[0] = malloc(sizeof(Node));
  leakMemory(PAGE_SIZE * 10);
  for (unsigned int i = 0; i < size; ++i){
    q[i]->val = i;
    for (unsigned int j = 0; j < CHILDREN; ++j){
      q[i]->children[j] = malloc(sizeof(Node));
      leakMemory(PAGE_SIZE * 10);
      if (tailPtr < size){
        q[tailPtr] = q[i]->children[j];
        ++tailPtr;
      }
    }
  }
  return q[0];
}

int sum(Node* root){
  if (!root){
    return 0;
  }
  int x = root->val;
  for (unsigned int i = 0; i < CHILDREN; ++i){
    x += sum(root->children[i]);
  }
  return x;
}

int main() {
  Node* tree = buildTree(100000);
  leakMemory(PAGE_SIZE * 200);
  printf("%d \n", sum(tree));
  return 0;
}

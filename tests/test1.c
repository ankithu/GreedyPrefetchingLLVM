#include <stdio.h>

// Example with a 5-ary Tree
struct QuintupleTree {
  int val;
  struct QuintupleTree* n1;
  struct QuintupleTree* n2;
  struct QuintupleTree* n3;
  struct QuintupleTree* n4;
  struct QuintupleTree* n5;
};

void sumPreorder(struct TreeNode* tree){
    if (!tree){
      return;
    }
    printf("%d", tree->val);
    sumPreorder(tree->n1);
    sumPreorder(tree->n2);
    sumPreorder(tree->n3);
    sumPreorder(tree->n4);
    sumPreorder(tree->n5);
}


int main()
{
  struct QuintupleTree n;
  sumPreorder(&n);
  return 0;
}

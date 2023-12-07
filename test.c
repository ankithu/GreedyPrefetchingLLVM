#include <stdio.h>

struct TreeNode {
  int val;
  struct TreeNode* l;
  struct TreeNode* r;
};

int foo(struct TreeNode* tree){
    if (!tree){
      return 0;
    }
    // return foo(tree->l);
    int x = foo(tree->l) + foo(tree->r);
    return x + tree->val;
}


int main()
{
  struct TreeNode n;
  foo(&n);

  
  return 1;
}

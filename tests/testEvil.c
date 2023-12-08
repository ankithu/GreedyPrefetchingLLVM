#include <stdio.h>

struct TreeNode {
  int val;
  struct TreeNode* children[10];
};

void foo(struct TreeNode* tree){
    if (!tree){
      return;
    }
    int location = 0;
    if (tree->val % 2){
        location = 5;
    }
    else{
        location = 7;
    }
    for ()
}


int main()
{
  struct TreeNode n;
  sumPreorder(&n);
  return 0;
}

#include <stdio.h>

struct TreeNode {
  int val;
  struct TreeNode* l;
  struct TreeNode* r;
};

void sumPreorder(struct TreeNode* tree){
    if (!tree){
      return;
    }
    printf("%d", tree->val);
    sumPreorder(tree->l);
    sumPreorder(tree->r);
}


int main()
{
  struct TreeNode n;
  sumPreorder(&n);
  return 0;
}

#include <stdio.h>

struct TreeNode {
  int val;
  struct TreeNode* l;
  struct TreeNode* r;
};

void sumPreorder(struct TreeNode* tree) {
    if (tree == NULL) {
      return;
    }
    printf("%d\n", tree->val);
    sumPreorder(tree->l);
    sumPreorder(tree->r);
}


int main()
{
  struct TreeNode left = {2, NULL, NULL};
  struct TreeNode right = {3, NULL, NULL};
  struct TreeNode n;
  n.val = 1;
  n.l = &left;
  n.r = &right;
  sumPreorder(&n);
  return 0;
}

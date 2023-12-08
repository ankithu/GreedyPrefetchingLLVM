#include <stdio.h>

struct TreeNode {
  int val;
  struct TreeNode* l;
  struct TreeNode* r;
};

void preorder(struct TreeNode* tree) {
    if (!tree) {
      return;
    }
    printf("%d", tree->val);
    preorder(tree->l);
    preorder(tree->r);
}


int main() {
  struct TreeNode n;
  preorder(&n);
  return 0;
}

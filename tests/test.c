#include <stdio.h>

struct TreeNode {
  int val;
  struct TreeNode* l;
  struct TreeNode* r;
};

typedef struct TreeNode TreeNode;

void sumPreorder(TreeNode* tree) {
    if (tree == NULL) {
      return;
    }
    printf("%d\n", tree->val);
    sumPreorder(tree->l);
    sumPreorder(tree->r);
}

TreeNode* btn(int val){
  TreeNode* tn;
  tn = malloc(sizeof(TreeNode));
  tn->l = NULL;
  tn->r = NULL;
  tn->val = val;
  return tn;
}


int main()
{
  TreeNode* l = btn(2);
  TreeNode* r = btn(5);
  TreeNode* ll = btn(3);
  TreeNode* lr = btn(4);
  TreeNode* rr = btn(6);
  TreeNode* n = btn(1);
  n->l = l;
  n->r = r;
  l->l = ll;
  l->r = lr;
  r->r = rr;

  sumPreorder(n);
  return 0;
}

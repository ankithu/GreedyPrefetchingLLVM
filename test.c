#include <stdio.h>

struct TreeNode {
  int val;
  struct TreeNode* l;
  struct TreeNode* r;
};

int foo(struct TreeNode* tree){
    return foo(tree->l);
}

int bar(int y){
    printf("%d", y);
    return 2;
}

int main()
{

  int in[1000]; 
  int i,j;
  struct TreeNode n;
  foo(&n);
  for (i = 0; i < 1000; i++)
  {
    in[i] = 0;
  }   

  for (j = 100; j < 1000; j++)
  {
   in[j]+= 10;
  }

  bar(2023);
  for (i = 0; i< 1000; i++)
    fprintf(stdout,"%d\n", in[i]);
  
  return 1;
}

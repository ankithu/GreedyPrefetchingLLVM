/* For copyright information, see olden_v1.0/COPYRIGHT */

/* tree-alloc.c
 */

#include "mem-ref.h"
#include "future-cell.h"
#include "tree.h"


tree_t *TreeAlloc (level, lo, proc)
    int		level, lo, proc;
{

  if (level == 0)
    return NULL;
  else {
#ifdef FUTURES    
    struct tree *new, *right;
    int mid, lo_tmp;
    future_cell_int fleft;
    
    new = (struct tree *) ALLOC(lo, sizeof(tree_t));
    NOTEST();
    FUTURE(level -1, lo+proc/2, proc/2,TreeAlloc,&fleft);
    right=TreeAlloc(level-1,lo,proc/2);
    new->val = 1;
    TOUCH(&fleft);
    new->left = (struct tree *) fleft.value;
    new->right = (struct tree *) right;
    RETEST();
    return new;
#else
    struct tree *new, *right, *left;
    int mid, lo_tmp;

    
    new = (struct tree *) mymalloc(sizeof(tree_t));
    left = TreeAlloc(level -1, lo+proc/2, proc/2);
    right=TreeAlloc(level-1,lo,proc/2);
    new->val = 1;
    new->left = (struct tree *) left;
    new->right = (struct tree *) right;
    return new;
#endif
  }

}

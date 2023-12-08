/* For copyright information, see olden_v1.0/COPYRIGHT */

#include "mst.h"

#define CONST_m1 10000
#define CONST_b 31415821
#define RANGE 2048
static int HashRange;

static int mult(int p, int q)
{
   int p1, p0, q1, q0;

   p1=p/CONST_m1; p0=p%CONST_m1;
   q1=q/CONST_m1; q0=q%CONST_m1;
   return (((p0*q1+p1*q0) % CONST_m1)*CONST_m1+p0*q0);
}

static int random(int seed)
{
  int tmp;
  tmp = (mult(seed,CONST_b)+1);
  return tmp;
}

static int compute_dist(int i,int j, int numvert)
{
  int less, gt;
  if (i<j) {less = i; gt = j;} else {less = j; gt = i;}
  return (random(less*numvert+gt) % RANGE)+1;
}

static int hashfunc(unsigned int key)
{
#ifndef NEWCACHE
  return ((key>>3) % HashRange);
#else
  return ((key>>10) % HashRange);
#endif
}

static void AddEdges(int count1, Graph retval, int numproc, 
                     int perproc, int numvert, int j) 
{
  Vertex tmp;
  Vertex helper[MAXPROC];
  int i;

  for (i=0; i<numproc; i++) {
    helper[i] = retval->vlist[i];
  }

  for (tmp = retval->vlist[j]; tmp; tmp=tmp->next) 
    {
      MLOCAL(tmp);
      for (i=0; i<numproc*perproc; i++) 
        {
          int pn,offset,dist;
          Vertex dest;
          Hash hash;
          
          if (i!=count1) 
            {
              dist = compute_dist(i,count1,numvert);
              pn = i/perproc;
              offset = i % perproc;
              dest = ((helper[pn])+offset);
              hash = tmp->edgehash;
              HashInsert((void *) dist,(unsigned int) dest,hash);
            }
        } /* for i... */
      count1++;
    } /* for tmp... */
}

Graph MakeGraph(int numvert, int numproc) 
{
  int perproc = numvert/numproc;
  int i,j;
  int count1;
  Vertex v,tmp;
  Vertex block;
  Graph retval;
#ifdef FUTURES
  future_cell_int fc[MAXPROC];
#endif

  retval = (Graph) ALLOC(0,sizeof(*retval));
  for (i=0; i<MAXPROC; i++) 
    {
      retval->vlist[i]=NULL;
    }
  chatting("Make phase 2\n");
  for (j=numproc-1; j>=0; j--) 
    {
      block = (Vertex) ALLOC(j,perproc*(sizeof(*tmp)));
      v = NULL;
      for (i=0; i<perproc; i++) 
        {
          tmp = block+(perproc-i-1);
          HashRange = numvert/4;
          tmp->mindist = 9999999;
          tmp->edgehash = MakeHash(numvert/4,hashfunc);
          tmp->next = v;
          v=tmp;
        }
      retval->vlist[j] = v;
    }

  chatting("Make phase 3\n");
  for (j=numproc-1; j>=0; j--) 
    {
      count1 = j*perproc;
#ifndef FUTURES
      AddEdges(count1, retval, numproc, perproc, numvert, j);
#else
      FUTURE(count1,retval,numproc,perproc,numvert,j,AddEdges,&fc[j]);
#endif
    } /* for j... */
  chatting("Make phase 4\n");
#ifdef FUTURES
  for (j=0; j<numproc; j++) 
    {
      TOUCH(&fc[j]);
    }
#endif

  chatting("Make returning\n");
  return retval;
}

  

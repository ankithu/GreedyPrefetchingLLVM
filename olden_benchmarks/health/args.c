/* For copyright information, see olden_v1.0/COPYRIGHT */

/*****************************************************************
 * args.c:  Handles arguments to command line.                   *
 *          To be used with health.c.                            *
 *****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "health.h"
#ifndef PLAIN
#include "mem-ref.h"
#include "future-cell.h"
#else
#define MIGRPH();
#endif

int dealwithargs(int argc, char *argv[]) { 
  /*MIGRPH();*/
  if (argc < 5) {
    fprintf(stderr, "usage:  health <# levels> <time> <seed> <# proc>\n");
    exit(1); }
  
  max_level = atoi(argv[1]);
  max_time = atol(argv[2]);
  seed = atol(argv[3]);
  __NumNodes = atoi(argv[4]);
}







#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


 int myvar = 0;


 int main()
  {
     fork();
     fork();

               myvar++;

     printf("val %d\n",myvar);
 }
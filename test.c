#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

int main()
{
	FILE *fp;
		
		char new_entry[200];
		struct stat st;


		fp = popen("cachedata.txt","r");
		if(fp!=NULL)
		{

			int file_size = 0;
			stat("cachedata.txt",&st);
			file_size = st.st_size;
			printf("file size %d\n",file_size);

			char *new_cache = (char*)malloc(file_size);
			if(new_cache == NULL)
			{
				printf("No enough size for new_cache in memory\n");
			}
			else
			{
			

				sprintf(new_entry,"\nsteve  ");
				printf("---->Making new entry into cache \n%s\n",new_entry);
				fread(new_cache,1,file_size,fp);

				fp = popen("cachedata.txt","w");
				fwrite(new_cache,1,file_size,fp);

				for(int i = 0; i < strlen(new_entry);i++)
				{
       				fprintf (fp,"%c" ,new_entry[i]);
				}
				pclose(fp);
				fp = NULL;

			}
		}
		else
		{
		printf("Could not locate cachedata.txt file\n");
		}
	}

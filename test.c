
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{

//char *command="ls -ltr";
		FILE *fp,*fw;
		char new_entry[200];
		int file_size=0;
		int pid = fork();

		if(pid==0)
		{
			fp = popen("cat cachedata.txt","r");
			if(fp!=NULL)
			{

				struct stat st;
				stat("cachedata.txt", &st);
				file_size = st.st_size;

				char *new_cache = (char*)malloc(file_size);
				if(new_cache == NULL)
				{
					printf("No enough size for new_cache in memory\n");
				}
				else
				{
			
					sprintf(new_entry,"\ntest steve ");
					fread(new_cache,1,file_size,fp);
					pclose(fp);
					fp = NULL;
					fw = popen("cat cachedata.txt","w");

					if(fw!=NULL)
					{
						printf("new_cache %s\n",new_cache);
						printf("new_entry %s\n",new_entry);
						printf("----------------------------\n");

						fwrite(new_cache,1,file_size,fw);

						fwrite(new_entry,1,strlen(new_entry),fw);
					
						pclose(fw);
						fw = NULL;
					}
					else
					{
						printf("not able to open in write mode\n");
					}

				}
			}
			else
			{
				printf("Could not locate cachedata.txt file\n");
			}
		}
}
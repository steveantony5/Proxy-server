/*-------------------------------------------------------------------------------
 *File name : proxy_server.c
 *Author    : Steve Antony Xavier Kennedy

 *Description: This file implements HTTP proxy server for handing GET requests

-------------------------------------------------------------------------------*/

/**************************************************************************************
*                                   Includes
**************************************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <netdb.h>
#include <dirent.h>

/**************************************************************************************
*                                   Macros
**************************************************************************************/
/*Header length*/
#define HEADER (500)

/*Number of clients to listen*/
#define LISTEN_MAX (10)

/*For debug purpose*/
#define DEBUG

/**************************************************************************************
*                                   Global declaration
**************************************************************************************/
typedef enum
{
	SUCCESS = 1,
	ERROR = -1,
	BLOCKED = -2,
	IP_NOT_FOUND = -3,
}status;

/*creating the socket*/
int proxy_server_socket, new_socket, remote_socket;

/*For request header*/
char request[HEADER];
char method[10];
char version[10];        
char url[500];
char content_length[10];
char content_type[10];
int port_number;
char ip[50];
char main_url[500];
char filename[1000];

/*For response header*/
char response[HEADER];
char request_remote[HEADER];
char response_remote[HEADER];
int cache_timeout;

/*For setting socket timeout in Pipelining*/
struct timeval tv;

socklen_t clilen;
struct sockaddr_in proxy_server_address, to_address, remote_address;
int file_size = 0;
int use_cache = 0;
int entry_present_in_cache = 0;

/*****************************************************************
*						Local function prototypes
*****************************************************************/
void socket_creation_proxy(int);
void parse_request(void);
void Get_request(void);
void error_request(char []);
status get_ip_and_blockstatus();
status is_cache_data_avail();
int difference_time(int, int, int, int,int, int);
void init();
void get_data_from_server();
void get_data_from_cache();
void get_curr_time(int *,int *, int *);
status make_DNS_request(void);
void get_filename(void);
void socket_creation_remote();
/***************************************************************
*                      Main Function
**************************************************************/

int main(int argc, char *argv[])
{
	status status_returned;

	if(argc<3)// passing port number as command line argument
	{
	    perror("Please provide port number");
	    exit(EXIT_FAILURE);
    }
        
    //Storing the port number for proxy server from command line vaiable
	int port_proxy = atoi(argv[1]);

	//Storing timout for data cache from command line variable
	cache_timeout = atoi(argv[2]);

	//Creating socket for proxy server
    socket_creation_proxy(port_proxy);
	
	
	while(1)
	{
		new_socket = 0;
		clilen = sizeof(to_address);

		/*Accepting Client connection*/
		new_socket = accept(proxy_server_socket,(struct sockaddr*) &to_address, &clilen);
		if(new_socket<0)
		{
			perror("error on accepting client");
		}
		
		/*Clearing the request variable*/
		memset(request,0,HEADER);

		/*Creating child processes*/
		/*Returns zero to child process if there is successful child creation*/
		int32_t child_id = fork();
		if(child_id < 0)
		{
			perror("error on creating child\n");
		}

		while((!child_id) && ( recv(new_socket,request, HEADER,0) > 0))
		{

			// blocking /favicon.ico request
			if(strstr(request, "favicon.ico"))
			{
				printf("\nReceived favicon request : Blocked by Proxy\n");
				goto end;

			}
			

			//Function for initilizing the variables used in the loop
			init();

			parse_request();
			
			if(((strcmp(method,"GET")==0))&&((strcmp(version,"HTTP/1.1")==0)||(strcmp(version,"HTTP/1.0")==0)) && (strncmp(url,"http://",7) == 0))
			{

				status_returned = is_cache_data_avail();
				if (status_returned == ERROR)
				{
					printf("Error occured in function is_cache_data_avail()");
					exit(1);
				}
			
				status_returned = get_ip_and_blockstatus();
				if(status_returned == BLOCKED)
				{
					error_request("Error: 403 Forbidden");
				}
				else if (status_returned == IP_NOT_FOUND)
				{
					error_request("Error: 404 Host name not resolved");
				}
				else
				{
					printf("\n---->Page not blocked\n");

					//gets the requested page from Cache folder
					if(use_cache == 1)
					{
						get_data_from_cache();
					}
					else
					{
						get_data_from_server();
					}
				}
			}
			else
			{
				printf("---->Error on Request- Not supported by Proxy server\n");
				error_request("Error: 400 Bad Request");
			}
			/*Checking connection inactive and iclosing the socket*/
			/*if connection is active, while loop is executed again without closing the socket*/
				
		end:
			printf("\n--------------------------------------------------------\n");

		}
		close(new_socket);


	}
	close(proxy_server_socket);
	exit(EXIT_SUCCESS);
}


/********************************************************************************************
*									Local Functions
********************************************************************************************/
//*****************************************************************************
// Name        : socket_creation_proxy
//
// Description : Funtion to create socket
//
// Arguments   : port - Port number
//
// return      : Not used
//
//****************************************************************************/

void socket_creation_proxy(int port)
{

	proxy_server_socket = socket(AF_INET,SOCK_STREAM,0);// setting the server socket
	if(proxy_server_socket < 0)
	{
		perror("error on proxy server socket creation");
		exit(EXIT_FAILURE);
	}
	memset(&proxy_server_address,0,sizeof(proxy_server_address));

	/*Setting socket so as to reuse the port number*/
	int true = 1;
	if(setsockopt(proxy_server_socket,SOL_SOCKET,SO_REUSEADDR,&true,sizeof(int)) < 0)
	{
		perror("error on setsocket of proxy server");
		exit(EXIT_FAILURE);
	}

	proxy_server_address.sin_family = AF_INET;
	proxy_server_address.sin_addr.s_addr	= INADDR_ANY;
	proxy_server_address.sin_port = htons(port);

	/*bind the server socket with the remote client socket*/
	if(bind(proxy_server_socket,(struct sockaddr*)&proxy_server_address,sizeof(proxy_server_address))<0)
	{
		perror("Binding failed in the proxy server");
		exit(EXIT_FAILURE);
	}

	/*Listening for clients*/
	if(listen(proxy_server_socket,LISTEN_MAX) < 0)
	{
		perror("Error on Listening");
		exit(EXIT_FAILURE);
	}
	else
		printf("\nlistening.....\n");


}

//*****************************************************************************
// Name        : init
//
// Description : Funtion to initialize the variables used
//
// Arguments   : none
//
// return      : Not used
//
//****************************************************************************/
void init()
{
	port_number = 0;
	memset(ip,0,sizeof(ip));
	entry_present_in_cache = 0;
	use_cache = 0;
	memset(filename,0,sizeof(filename));

}

//*****************************************************************************
// Name        : parse_request
//
// Description : Funtion to create socket
//
// Arguments   : none
//
// return      : Not used
//
//****************************************************************************/
void parse_request()
{
	memset(method,0,sizeof(method));
	memset(url,0,sizeof(url));
	memset(version,0,sizeof(version));
	memset(main_url,0,sizeof(main_url));

	printf("\nReceived Request to Proxy Server :\n%s",request);
	
				
	/*separating the parameters from request string*/
	sscanf(request,"%s%s%s",method, url, version);


	// Extracting the name of the requested url and port number for remote server
	char port_number_str[5];
	memset(port_number_str,0,sizeof(port_number_str));
	char *first_occ ,*last_occ;



	first_occ = strchr(url,':');
	last_occ  = strrchr(url,':');

	if(first_occ == last_occ) // Port number is not provided in the request
	{
		port_number = 80; //default port number
		printf("-->Default port 80 is used\n");
	}

	//if port number for remote server is provided in the request
	else
	{
		last_occ++;
		strcpy(port_number_str,last_occ);
		port_number = atoi(port_number_str);
		last_occ = strrchr(url,':');
		*last_occ = '\0' ;
		printf("-->Port %s is used\n",port_number_str);
	}

	//Getting the domain name from url
	char *search = NULL;
	search = strstr(url,"://");
	search = search +3;
	strcpy(main_url,search);
	search = strchr(main_url,'/');
	if(search!=NULL)
	{
		*search = '\0';
	}
	


		printf("Information extracted from the request :\n");
		printf("Method                       - %s\n",method);
		printf("URL                          - %s\n",url);
		printf("HTTP Version                 - %s\n",version);
		printf("Domain name                  - %s\n",main_url);
		printf("Port number for remote server- %d\n\n",port_number);

  	
}


//*****************************************************************************
// Name        : is_cache_data_avail
//
// Description : Funtion to check if the file is present in cache index
//
// Arguments   : none
//
// return      : status
//
//****************************************************************************/

status is_cache_data_avail()
{
	FILE *fp;
	fp = NULL;
	char *position;

	
    //For storing the current time
	int cur_min = 0, cur_sec = 0, cur_hr = 0;

	//Getting the current system time
	get_curr_time(&cur_hr,&cur_min,&cur_sec);

	
	fp = fopen("cachedata.txt","r");
	if(fp==NULL)
	{
		printf("Could not find cachedata.txt\n");
		return ERROR;
	}
	else
	{
		fseek(fp,0,SEEK_END);
		file_size = 0;
		file_size=ftell(fp);
		fseek(fp,0,SEEK_SET);

		char *cache = (char*)malloc(file_size);
		if(cache == NULL)
		{
			printf("No enough size for cache in memory\n");
			return ERROR;
		}

		fread(cache,1,file_size,fp);
		position = NULL;
		fclose(fp);
		fp = NULL;
		char check_url[30];
		char temp_url[30];

		//Padding a space after URL to get the exact match
		sprintf(temp_url,"%s ",url);

		position = strstr(cache,temp_url);

		if(position!= NULL)
		{
			printf("\n---->Match of URL found in cache index\n");

			int min = 0, sec = 0, hr = 0;
			sscanf(position,"%s%d%d%d",check_url,&hr,&min,&sec);
			if(strcmp(check_url,url)==0)
			{
				int diff = 0;
				diff = difference_time(cur_hr,cur_min,cur_sec,hr, min,sec);

				if(diff < cache_timeout)
				{
					use_cache = 1;
					entry_present_in_cache = 1;
					printf("---->The difference of time stamp of Cache file is %d secs\n",diff);
					printf("---->Hence, Cache file is used\n");
				}
				else
				{
					printf("---->The difference of time stamp of Cache file is %d secs\n",diff);
					printf("\n----> The entry in Cache is obsolete\n\n----> Updating entry in cache index\n");

					char new_cache_data[100];
					memset(new_cache_data,0,sizeof(new_cache_data));
					sprintf(new_cache_data,"%s %2d %2d %2d ",url,cur_hr, cur_min,cur_sec);

					int i = 0;
					while(new_cache_data[i]!='\0')
					{
						*position = new_cache_data[i];
						position++;
						i++;
					}
					fp = fopen("cachedata.txt","wb");
					if(fp!=NULL)
					{
						fwrite(cache,1,file_size,fp);
						entry_present_in_cache = 1;
						fclose(fp);

					}
					else
					{
						#ifdef DEBUG
						printf("\nCould not open cachedata.txt in write mode\n");
						#endif
					}
				}
			}
			else
			{
				#ifdef DEBUG
				printf("\nExact match not found\n");
				#endif
			}

		}
		else
		{
			printf("\n---->Match of URL not found in cache index\n");
		}

		free(cache);
		cache = NULL;
		position = NULL;
		fp = NULL;
		return SUCCESS;
	}
}

//*****************************************************************************
// Name        : difference_time
//
// Description : Funtion for calculating the time difference
//
// Arguments   : current min,sec, hr and time for which the difference has to be found from current time
//
// return      : difference in seconds
//
//****************************************************************************/
int difference_time(int cur_hr, int cur_min,int cur_sec,int hr, int min,int sec)
{
	int difference = abs(((cur_hr*60*60)+(cur_min*60)+ cur_sec) - ((hr*60*60)+(min*60)+ sec));
	printf("cur hr min sec %d %d %d \n",cur_hr,cur_min,cur_sec);
	printf("stm hr min sec %d %d %d \n",hr,min,sec);
	return difference;
}

//*****************************************************************************
// Name        : get_ip_and_blockstatus
//
// Description : Funtion for checking if the ip or domain name is blocked
//
// Arguments   : None
//
// return      : status
//
//****************************************************************************/

status get_ip_and_blockstatus()
{
	FILE *fp;
	char *position;

	fp = fopen("Blocked.txt","rb");
	if(fp ==NULL)
	{
		printf("\nBlocked.txt not present\n");
		return ERROR;
	}
	fseek(fp,0,SEEK_END);
	file_size = 0;
	file_size=ftell(fp);
	fseek(fp,0,SEEK_SET);


	char *blocked_data = (char*)malloc(file_size);
	if(blocked_data == NULL)
		return ERROR;

	fread(blocked_data,1,file_size,fp);

	//Checks if the requested domain name is present in blocked list
	position = strstr(blocked_data,main_url);
	if(position!= NULL)
	{
			char check[20];
			memset(check,0,20);
			sscanf(position,"%s",check);
			if(strcmp(check,main_url)==0)
			{
				printf("File blocked\n");
				return BLOCKED;	
			}
			
	}


	free(blocked_data);
	blocked_data = NULL;
	fclose(fp);
	fp = NULL;

	/*Checks if the request has domain name or IP*/
	int j=0,count = 0;
	for(j=0; j < strlen(main_url); j++)
	{
		if(main_url[j] == '.')
			count++;
		if(count == 4)
		{
			#ifdef DEBUG
			printf("Entered IP address instead of domain name\n");
			#endif
			goto end;
		}
			
	}
	

	/*--------------------Looking for IP in the IP Cache.txt-------------------*/
	memset(ip,0,sizeof(ip));
	fp = fopen("IP_Cache.txt","rb");
	if(fp ==NULL)
	{
		printf("\nIP_Cache.txt not present\n");
		return ERROR;
	}

	fseek(fp,0,SEEK_END);
	file_size = 0;
	file_size=ftell(fp);
	fseek(fp,0,SEEK_SET);


	char *Cached_data = (char*)malloc(file_size);
	if(Cached_data == NULL)
		return ERROR;

	fread(Cached_data,1,file_size,fp);	
	position = strstr(Cached_data,main_url);
	int found = 0;
	if(position!=NULL)
	{
			char check_name[20];
			sscanf(position,"%s",check_name);
			if(strcmp(check_name,main_url)==0)
			{
				sscanf(position,"%*s%s",ip);
				
				printf("\n---->IP for the domain name found in IPCache\n");
 
 				found = 1;
			}
	}

	//Makes DNS request when IP is not found in IPCache
	if((position == NULL) || (found == 0))
	{
		printf("\n---->IP for the domain name not found in IPCache\n");
		status DNS_status = make_DNS_request();
		if(DNS_status == IP_NOT_FOUND)
		{
			printf("\n---->IP for the domain name could not be found through DNS\n");
			return IP_NOT_FOUND;
		}
	}


	free(Cached_data);
	Cached_data = NULL;
	fclose(fp);
	fp = NULL;


end:


	/*Check if the IP is in the blocked list*/
	fp = fopen("Blocked.txt","rb");
	if(fp ==NULL)
	{
		printf("\nBlocked file not present\n");
		return ERROR;
	}
	fseek(fp,0,SEEK_END);
	file_size = 0;
	file_size=ftell(fp);
	fseek(fp,0,SEEK_SET);


	blocked_data = (char*)malloc(file_size);
	if(blocked_data == NULL)
		return ERROR;

	fread(blocked_data,1,file_size,fp);
	position = strstr(blocked_data,ip);
	if(position!= NULL)
	{
			char check[20];
			memset(check,0,20);
			sscanf(position,"%s",check);
			if(strcmp(check,ip)==0)
			{
				printf("File blocked\n");
				return BLOCKED;	
			}
			
	}


	free(blocked_data);
	blocked_data = NULL;

	fclose(fp);
	fp = NULL;
	printf("---->IP : %s\n",ip);

	return SUCCESS;
}

//*****************************************************************************
// Name        : make_DNS_request
//
// Description : Funtion for for making DNS request
//
// Arguments   : I/P - domain name
//
// return      : status
//
//****************************************************************************/
status  make_DNS_request()
{
	struct hostent *he;
	struct in_addr **addr_list;
	int i;
		
	if ( (he = gethostbyname( main_url ) ) == NULL) 
	{
		return IP_NOT_FOUND;
	}

	addr_list = (struct in_addr **) he->h_addr_list;
	
	for(i = 0; addr_list[i] != NULL; i++) 
	{
		//Return the first one;
		strcpy(ip , inet_ntoa(*addr_list[i]) );
	}
	FILE *dns;
	dns = fopen("IP_Cache.txt","a+");
	if(dns==NULL)
	{
		printf("Error on opening IP_Cache.txt\n");
		return IP_NOT_FOUND;
	}
	else
	{
		char new_entry[30];
		fseek(dns,0,SEEK_END);
		sprintf(new_entry,"\n%s %s ",main_url,ip);
		fwrite(new_entry,1,strlen(new_entry),dns);
		fclose(dns);
	}
	return SUCCESS;
}


//*****************************************************************************
// Name        : get_data_from_cache
//
// Description : Funtion for getting data from cache folder
//
// Arguments   : none
//
// return      : Not used
//
//****************************************************************************/

void get_data_from_cache()
{

	//Filename to retrive from Cache folder
	get_filename();

	#ifdef DEBUG
	printf("Filename %s\n",filename);
	#endif

	FILE *fp;
	fp = fopen(filename,"r");
	if(fp==NULL)
	{
		printf("\n---->Could not find cache file\n");
	}
	else
	{
		fseek(fp,0,SEEK_END);
		file_size = 0;
		file_size=ftell(fp);
		fseek(fp,0,SEEK_SET);
		char *data = (char*)malloc(file_size);
		if(data!=NULL)
		{
			fread(data,1,file_size,fp);
			printf("---->Retrived data from Cache\n");

			printf("\n---->File :\n\n%s\n",data);
			printf("End of FILE\n");

			send(new_socket,data,file_size,0);

			free(data);
			fclose(fp);
		}
	}

}

//*****************************************************************************
// Name        : get_filename
//
// Description : Funtion for forming the filename to be opened in Cache folder
//
// Arguments   : O/P
//
// return      : Not used
//
//****************************************************************************/
void get_filename()
{
	int len = strlen(url);
	int j,i = 0;

	char temp[300];
	for( i=0, j = 7; j<=len;i++,j++)
	{
		temp[i] = url[j];
		if(j==len)
		{
			temp[i]='\0';
		}
	}

	sprintf(filename,"Cache/%s",temp);
	char *find = strstr(temp,"/");

	if(find != NULL)
	{
		
		if(strstr(find,".") == NULL)
		{
			int len = strlen(filename);
			if(filename[len-1] == '/')
			{
				strcat(filename,"default.html");
			}
			else
			{
				strcat(filename,"/default.html");
			}

		}
	}
	else
	{
		strcat(filename,"/default.html");	
	}

}
//*****************************************************************************
// Name        : get_data_from_server
//
// Description : Funtion for obtaining data from server
//
// Arguments   : None
//
// return      : Not used
//
//****************************************************************************/

void get_data_from_server()
{
	printf("\n---->Getting data from remote server\n");

	if(entry_present_in_cache == 0)
	{
		FILE *fp;
		int cur_min, cur_sec, cur_hr;
		char new_entry[200];
		fp = fopen("cachedata.txt","r");
		if(fp!=NULL)
		{

			fseek(fp,0,SEEK_END);
			file_size = 0;
			file_size=ftell(fp);
			fseek(fp,0,SEEK_SET);

			char *new_cache = (char*)malloc(file_size);
			if(new_cache == NULL)
			{
				printf("No enough size for new_cache in memory\n");
			}
			else
			{
			
				get_curr_time(&cur_hr,&cur_min,&cur_sec);

				sprintf(new_entry,"\n%s %d %d %d ",url,cur_hr,cur_min,cur_sec);
				printf("---->Making new entry into cache \n%s\n",new_entry);
				fread(new_cache,1,file_size,fp);

				fp = fopen("cachedata.txt","w");
				fwrite(new_cache,1,file_size,fp);

				for(int i = 0; i < strlen(new_entry);i++)
				{
       				fprintf (fp,"%c" ,new_entry[i]);
				}
				fclose(fp);
				fp = NULL;

			}
		}
		else
		{
		printf("Could not locate cachedata.txt file\n");
		}
	}

	socket_creation_remote();

}
//*****************************************************************************
// Name        : socket_creation_remote
//
// Description : Funtion for creating socket for establishing connection to remote server
//
// Arguments   : None
//
// return      : Not used
//
//****************************************************************************/

void socket_creation_remote()
{
	char folder_path[200], folder_name[200], filename[200] ;
	remote_socket = socket(AF_INET,SOCK_STREAM,0);// setting the server socket
	if(remote_socket < 0)
	{

		perror("error on socket creation");
		exit(EXIT_FAILURE);
	}
	memset(&remote_address,0,sizeof(remote_address));

	/*Setting socket so as to reuse the port number*/
	int true = 1;
	if(setsockopt(remote_socket,SOL_SOCKET,SO_REUSEADDR,&true,sizeof(int)) < 0)
	{
		perror("error on setsocket");
		exit(EXIT_FAILURE);
	}

	printf("\nPort %d is used for remote connection\n",port_number);
	printf("\nConnecting to %s... \n",ip);


	remote_address.sin_family = AF_INET;
	remote_address.sin_addr.s_addr	= inet_addr(ip);
	remote_address.sin_port = htons(port_number);


    int status_val = connect(remote_socket,(struct sockaddr*)&remote_address,sizeof(struct sockaddr));

    if(status_val < 0)
    	printf("Error in establishing connection\n");

 	else
 	{
 		printf("\n---->Established connection with remote server\n");

 		//Getting the file_path from url
 		char *find_domain, *find_path;

 		find_domain = strstr(url,main_url);
 		if(find_domain!=NULL)
 		{
 			find_path = strstr(find_domain,"/");
 			if(find_path!=NULL)
 			{
 				char *first_occ;
 				first_occ = strchr(find_path,'.');
 				if(first_occ != NULL)
 				{
 					strcpy(folder_path,find_path);
 					char *temp;
 					temp = strrchr(folder_path,'/');
 					*temp ='\0';

 					/* http://morse.colorado.edu/images/eg.gif*/
 					sprintf(filename,"Cache/%s%s",main_url,find_path);
 					sprintf(folder_name,"Cache/%s%s",main_url,folder_path);
 				}
 				else
 				{
 					sprintf(folder_name,"Cache/%s%s",main_url,find_path);

 					int len = strlen(folder_name);
 					if(folder_name[len-1] == '/')
 					{
 						/* http://morse.colorado.edu/images/
 						   http://morse.colorado.edu/         */
 						sprintf(filename,"Cache/%s%sdefault.html",main_url,find_path);
 					}
 					else
 					{
 						/* http://morse.colorado.edu/images*/
 						sprintf(filename,"Cache/%s%s/default.html",main_url,find_path);
 					}
 				}
 				

 				
 			}
 			else
 			{
 				
 				/* http://morse.colorado.edu*/
 				sprintf(filename,"Cache/%s/default.html",main_url);
 				sprintf(folder_name,"Cache/%s",main_url);
 			}
 		}
 		else
 		{
 			printf("\nCould not find domain name in URL\n");
 		}

 		#ifdef DEBUG
 		printf("Filename : %s\n",filename);
 		printf("Foldername : %s\n",folder_name);
 		#endif

 		if(find_path == NULL)
 		{
 			 sprintf(request_remote,"GET / %s\nHost: %s\nConnection : close\n\n",version,main_url);
 		}
 		else
 		{
 			//sprintf(request_remote,"GET /~remzi/OSTEP/ HTTP/1.1\r\nHost: pages.cs.wisc.edu\r\nConnection: close\r\n\r\n");

 			sprintf(request_remote,"GET %s %s\nHost: %s\nConnection : close\n\n",find_path,version,main_url);
 		}


 		printf("\n---->Request : \n%s\n",request_remote);

 		if((send(remote_socket,request_remote,strlen(request_remote),0))<0)
 			printf("Error on sending to remote server\n");


 		else
 		{
 			printf("\nReceiving file...\n");
 			FILE *recv_data;
 			int characters;
	
			DIR *dir = opendir(folder_name);
			if(dir)
			{
				#ifdef DEBUG
				printf("Path already exists\n");
				#endif
			}
			else
			{
				char cmd[100];
				sprintf(cmd,"mkdir -p %s",folder_name);

				system(cmd);
				#ifdef DEBUG
				printf("Created Folder: %s\n",folder_name);
				#endif
			}

 			recv_data = fopen(filename,"w");
 			if(recv_data!=NULL)
 			{
 				printf("\n---->Received File :\n");
 				do
 				{
 					characters = 0;
 					memset(response_remote,0,sizeof(response_remote));
 					characters = recv(remote_socket,response_remote,HEADER,0);
 					fwrite(response_remote, 1, characters,recv_data);
 					send(new_socket,response_remote,characters,0);
 					printf("%s",response_remote);

 				}while(characters>0);
 				printf("\nEnd of File\n");
 				fclose(recv_data);
 			}
 			else
 			{
 				#ifdef DEBUG
 				printf("\nCannot create a file in Cache\n");
 				#endif
 			}

 			
 		}
 	}
}


//*****************************************************************************
// Name        : error_request
//
// Description : Funtion for handling errors
//
// Arguments   : none
//
// return      : Not used
//
//****************************************************************************/
void error_request(char message[100])
{

	char content[400];
	memset(content,0,sizeof(content));
	memset(response,0,sizeof(response));
	sprintf(content,"<html><body><h1> %s %s </h1></body></html>",message,url);
	int content_length = 0;
	content_length = strlen(content);

	sprintf(response,"%s %s\r\nContent-Type: .html\r\nContent-Length: %d\r\n\r\n%s",version,message,content_length,content);
	
	printf("\nResponse Header: \n%s",response);
	write(new_socket,response,strlen(response));

}




//*****************************************************************************
// Name        : get_curr_time
//
// Description : Funtion for getting the current system time
//
// Arguments   : o/p - min, hr, sec
//
// return      : Not used
//
//****************************************************************************/
void get_curr_time(int *hr_cur,int *min_cur, int *sec_cur)
{
	//get current time
	*hr_cur = 0;
	*sec_cur = 0;
	*min_cur = 0;
	time_t curr_time = time(NULL);
	struct tm *info = localtime(&curr_time);	
	*min_cur = (info->tm_min);
	*sec_cur = (info->tm_sec);
	*hr_cur = (info->tm_hour);
}

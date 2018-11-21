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
#include <sys/wait.h>

/**************************************************************************************
*                                   Macros
**************************************************************************************/
/*Header length*/
#define HEADER (1500)

#define BUFFER (2048)

/*Number of clients to listen*/
#define LISTEN_MAX (20)

/*For debug purpose*/
#define DEBUG


#define TIME_OUT_SOCKET (10)
/**************************************************************************************
*                                   Global declaration
**************************************************************************************/
typedef enum
{
	SUCCESS = 1,
	ERROR = -1,
	BLOCKED = -2,
	IP_NOT_FOUND = -3,
	SKIP_TO_END = -4,
}status;

/*creating the socket*/
int proxy_server_socket, new_socket, remote_socket;

/*For request header*/
char request[HEADER];
char method[100];
char version[100];        
char url[800];
char content_length[10];
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
status socket_creation_proxy(int);
status parse_request(void);
void error_request(char []);
status get_ip_and_blockstatus();
status is_cache_data_avail();
int difference_time(int, int, int, int,int, int);
void init();
status get_data_from_server();
status get_data_from_cache();
void get_curr_time(int *,int *, int *);
status make_DNS_request(void);
void get_filename(void);
status socket_creation_remote();
/***************************************************************
*                      Main Function
**************************************************************/

int main(int argc, char *argv[])
{
	//for knowing the status of each called function : error handling
	status status_returned;

	//for forking
	int32_t child_id = 0;

	if(argc<3)// passing port number as command line argument
	{
	    perror("Please provide port number, cache timeout");
	    exit(EXIT_FAILURE);
    }
        
    //Storing the port number for proxy server from command line vaiable
	int port_proxy = atoi(argv[1]);

	//Storing timout for data cache from command line variable
	cache_timeout = atoi(argv[2]);

	//Creating socket for proxy server
    status_returned = socket_creation_proxy(port_proxy);
	if(status_returned == ERROR)
	{
		printf("Error on socket creation for proxy\n");
	    exit(EXIT_FAILURE);
	}
	
	while(1)
	{
		new_socket = 0;
		clilen = sizeof(to_address);

		/*Accepting Client connection*/
		new_socket = accept(proxy_server_socket,(struct sockaddr*) &to_address, &clilen);
		if(new_socket<0)
		{
			perror("Error on accepting client");
			exit(1);

		}
		
		/*Clearing the request variable*/
		memset(request,0,HEADER);


		child_id = 0;
		/*Creating child processes*/
		/*Returns zero to child process if there is successful child creation*/
		child_id = fork();

		// error on child process
		if(child_id < 0)
		{
			perror("error on creating child\n");
			exit(1);
		}

		//closing the parent
		if (child_id > 0)
		{
			close(new_socket);
			waitpid(0, NULL, WNOHANG);	//Wait for state change of the child process
		}

		while((child_id == 0) && ( recv(new_socket,request, HEADER,0) > 0))
		{

			// blocking /favicon.ico request
			if(strstr(request, "favicon"))
			{
				printf("\nReceived favicon request : Blocked by Proxy\n");
				goto end;

			}
			

			//Function for initilizing the variables used in the loop
			init();

			// parsing the request
			status_returned = parse_request();
			{
				if(status_returned == ERROR)
				{
					goto end;
				}
			}
			
			if(((strcmp(method,"GET")==0))&&((strcmp(version,"HTTP/1.1")==0)||(strcmp(version,"HTTP/1.0")==0)) && (strncmp(url,"http://",7) == 0))
			{

				// checks if the request is available in cache
				status_returned = is_cache_data_avail();
				if (status_returned == ERROR)
				{
					printf("Error occured in function is_cache_data_avail()");
	    			exit(EXIT_FAILURE);
				}
			
			    // get the ip address and block status
				status_returned = get_ip_and_blockstatus();
				if(status_returned == BLOCKED)
				{
					error_request("Error: 403 Forbidden");
				}
				else if (status_returned == IP_NOT_FOUND)
				{
					error_request("Error: 404 Host name not resolved");
				}
				else if (status_returned == ERROR)
				{
					printf("Error occured in function get_ip_and_blockstatus()");
	    			exit(EXIT_FAILURE);
				}

				// enters if the page is not blocked and ip is found
				else
				{
					printf("\n---->Page not blocked\n");

					//gets the requested page from Cache folder
					if(use_cache == 1)
					{
						// fetching data from cache
						status_returned = get_data_from_cache();
						if(status_returned == ERROR)
						{
							printf("Error occured in function get_data_from_cache()");
	    					exit(EXIT_FAILURE);
						}
					}
					else
					{
						// fetching data from server
						status_returned = get_data_from_server();
						if(status_returned == ERROR)
						{
							printf("Error occured in function get_data_from_server()");
	    					exit(EXIT_FAILURE);
						}
						if(status_returned == SKIP_TO_END)
						{
							printf("\nIssue with connecting to remote server\n");
							goto end;
						}
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

			close(new_socket);
		}
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

status socket_creation_proxy(int port)
{

	proxy_server_socket = socket(AF_INET,SOCK_STREAM,0);// setting the server socket
	if(proxy_server_socket < 0)
	{
		perror("error on proxy server socket creation");
		return ERROR;
	}
	memset(&proxy_server_address,0,sizeof(proxy_server_address));

	/*Setting socket so as to reuse the port number*/
	int true = 1;
	if(setsockopt(proxy_server_socket,SOL_SOCKET,SO_REUSEADDR,&true,sizeof(int)) < 0)
	{
		perror("error on setsocket of proxy server");
		return ERROR;
	}

	proxy_server_address.sin_family = AF_INET;
	proxy_server_address.sin_addr.s_addr	= INADDR_ANY;
	proxy_server_address.sin_port = htons(port);

	/*bind the server socket with the remote client socket*/
	if(bind(proxy_server_socket,(struct sockaddr*)&proxy_server_address,sizeof(proxy_server_address))<0)
	{
		perror("Binding failed in the proxy server");
		return ERROR;
	}

	/*Listening for clients*/
	if(listen(proxy_server_socket,LISTEN_MAX) < 0)
	{
		perror("Error on Listening");
		return ERROR;
	}
	else
	{
		printf("\nlistening.....\n");
	}
	return SUCCESS;


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
status parse_request()
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


	// Checking if port number is provided in the request
	first_occ = strchr(url,':');
	last_occ  = strrchr(url,':');

	// if port number is not provided: default is 80
	if(first_occ == last_occ) // Port number is not provided in the request
	{
		port_number = 80; //default port number
		printf("-->Default port 80 is used\n");
	}

	//if port number for remote server is provided in the request
	else
	{
		last_occ++;
		if(((*last_occ) >= 48) && ((*last_occ) <=57))
		{
			strcpy(port_number_str,last_occ);
			port_number = atoi(port_number_str);
			last_occ = strrchr(url,':');
			*last_occ = '\0' ;
			printf("-->Port %s is used\n",port_number_str);
		}
		else
		{
			port_number = 80; //default port number
			printf("-->Default port 80 is used\n");
		}
	}

	//Getting the domain name from url
	char *search = NULL;
	search = strstr(url,"://");
	if(search != NULL)
	{
		search = search +3;
		strcpy(main_url,search);
		search = strchr(main_url,'/');
		if(search!=NULL)
		{
			*search = '\0';
		}
	}
	else
	{
		return ERROR;
	}
	


		printf("Information extracted from the request :\n");
		printf("Method                       - %s\n",method);
		printf("URL                          - %s\n",url);
		printf("HTTP Version                 - %s\n",version);
		printf("Domain name                  - %s\n",main_url);
		printf("Port number for remote server- %d\n\n",port_number);

  	return SUCCESS;
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
	FILE *cache_fp;
	cache_fp = NULL;
	char *position;

	
    //For storing the current time
	int cur_min = 0, cur_sec = 0, cur_hr = 0;

	//Getting the current system time
	get_curr_time(&cur_hr,&cur_min,&cur_sec);

	
	cache_fp = fopen("cachedata.txt","r");
	if(cache_fp==NULL)
	{
		printf("Could not find cachedata.txt\n");
		return ERROR;
	}
	else
	{
		file_size = 0;
		struct stat st_cache;

		stat("cachedata.txt", &st_cache);
		file_size = st_cache.st_size;
		
		char *cache = (char*)calloc(1, file_size);
		if(cache == NULL)
		{
			printf("No enough size for cache in memory\n");
			return ERROR;
		}

		fread(cache,1,file_size,cache_fp);
		position = NULL;
		fclose(cache_fp);
		cache_fp = NULL;
		char check_url[200];
		char temp_url[200];
		memset(check_url,0,sizeof(check_url));
		memset(temp_url,0,sizeof(temp_url));

		//Padding a space after URL to get the exact match
		sprintf(temp_url,"%s ",url);

		// searching url in cache
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

				// checks if the cache data is within timeout
				if(diff < cache_timeout)
				{
					use_cache = 1;
					entry_present_in_cache = 1;
					printf("---->The difference of time stamp of Cache file is %d secs\n",diff);
					printf("---->Hence, Cache file is used\n");
				}

				//if cache is obsolete
				else
				{
					printf("---->The difference of time stamp of Cache file is %d secs\n",diff);
					printf("\n----> The 	entry in Cache is obsolete\n\n----> Updating entry in cache index\n");

					char new_cache_data[400];
					memset(new_cache_data,0,sizeof(new_cache_data));
					sprintf(new_cache_data,"%s %2d %2d %2d ",url,cur_hr, cur_min,cur_sec);

					// updating caceh entry for outdated entries
					int i = 0;
					while(new_cache_data[i]!='\0')
					{
						*position = new_cache_data[i];
						position++;
						i++;
					}
					FILE *cache_fp2;
					cache_fp2 = fopen("cachedata.txt","w");
					if(cache_fp2!=NULL)
					{
						fwrite(cache,1,file_size,cache_fp2);
						entry_present_in_cache = 1;
						fclose(cache_fp2);
						cache_fp2 = NULL;

					}
					else
					{
						#ifdef DEBUG
						printf("\nCould not open cachedata.txt in write mode\n");
						#endif
						return ERROR;
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
	FILE *block_fp;
	FILE *block_fp2;
	char *position;

	block_fp = fopen("Blocked.txt","rb");
	if(block_fp ==NULL)
	{
		printf("\nBlocked.txt not present\n");
		return ERROR;
	}
	file_size = 0;
	struct stat st_block;

	stat("Blocked.txt", &st_block);
	file_size = st_block.st_size;


	char *blocked_data = (char*)calloc(1, file_size);
	if(blocked_data == NULL)
	{
		perror("Malloc failed for storing block data");
		return ERROR;
	}

	fread(blocked_data,1,file_size,block_fp);

	//Checks if the requested domain name is present in blocked list
	position = strstr(blocked_data,main_url);
	if(position!= NULL)
	{
			char check[500];
			memset(check,0,500);
			sscanf(position,"%s",check);
			if(strcmp(check,main_url)==0)
			{
				printf("File blocked\n");
				return BLOCKED;	
			}
			
	}


	free(blocked_data);
	blocked_data = NULL;
	fclose(block_fp);
	block_fp = NULL;

	/*Checks if the request has domain name or IP*/
	int j=0,count = 1;
	for(j=0; j < strlen(main_url); j++)
	{
		if((main_url[0]  >= 48) && (main_url[0] <=57))
		{
			if(main_url[j] == '.')
				count++;
		}
		
		if(count == 4)
		{
			#ifdef DEBUG
			printf("Entered IP address instead of domain name\n");
			#endif
			strcpy(ip,main_url);
			goto end;
		}
			
	}
	

	/*--------------------Looking for IP in the IP Cache.txt-------------------*/
	memset(ip,0,sizeof(ip));
	FILE *IP_Cache_fp;
	IP_Cache_fp = fopen("IP_Cache.txt","r");
	if(IP_Cache_fp ==NULL)
	{
		printf("\nIP_Cache.txt not present\n");
		return ERROR;
	}

	file_size = 0;

	struct stat st_IP;

	stat("IP_Cache.txt", &st_IP);
	file_size = st_IP.st_size;


	char *Cached_data = (char*)calloc(1, file_size);
	if(Cached_data == NULL)
	{
		perror("Malloc failed for storing cache data");
		return ERROR;
	}

	fread(Cached_data,1,file_size,IP_Cache_fp);	
	position = strstr(Cached_data,main_url);
	int found = 0;
	if(position!=NULL)
	{
			char check_name[500];
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
	fclose(IP_Cache_fp);
	IP_Cache_fp = NULL;


end:

	
	/*Check if the IP is in the blocked list*/
	
	block_fp2 = fopen("Blocked.txt","rb");
	if(block_fp2 ==NULL)
	{
		printf("\nBlocked file not present\n");
		return ERROR;
	}
	file_size = 0;
	struct stat st_B;

	stat("Blocked.txt", &st_B);
	file_size = st_B.st_size;


	blocked_data = (char*)calloc(1, file_size);
	if(blocked_data == NULL)
	{
		perror("Malloc failed for storing blocked data");
		return ERROR;
	}

	fread(blocked_data,1,file_size,block_fp2);
	position = strstr(blocked_data,ip);
	if(position!= NULL)
	{
			char check[500];
			memset(check,0,500);
			sscanf(position,"%s",check);
			if(strcmp(check,ip)==0)
			{
				printf("File blocked\n");
				return BLOCKED;	
			}
			
	}


	free(blocked_data);
	blocked_data = NULL;

	fclose(block_fp2);
	block_fp2 = NULL;
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
		return ERROR;
	}
	else
	{
		char new_entry[500];
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

status get_data_from_cache()
{

	//Filename to retrive from Cache folder
	get_filename();

	#ifdef DEBUG
	printf("Filename %s\n",filename);
	#endif

	FILE *fp_file_cache;
	fp_file_cache = fopen(filename,"r");
	if(fp_file_cache==NULL)
	{
		printf("\n---->Could not find cache file\n");
		return ERROR;
	}
	else
	{
		fseek(fp_file_cache,0,SEEK_END);
		file_size = 0;
		file_size=ftell(fp_file_cache);
		fseek(fp_file_cache,0,SEEK_SET);
		char *data = (char*)calloc(1, file_size);
		if(data!=NULL)
		{
			fread(data,1,file_size,fp_file_cache);
			printf("---->Retrived data from Cache\n");

			//printf("\n---->File :\n\n%s\n",data);
			printf("End of FILE\n");

			send(new_socket,data,file_size,0);

			free(data);
			fclose(fp_file_cache);
			fp_file_cache = NULL;
		}
		else
		{
			perror("malloc failed for storing file from cache");
			return ERROR;
		}
	}
	return SUCCESS;

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

	char temp[500];
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
				strcat(filename,"index.html");
			}
			else
			{
				strcat(filename,"/index.html");
			}

		}
	}
	else
	{
		strcat(filename,"/index.html");	
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

status get_data_from_server()
{
	printf("\n---->Getting data from remote server\n");

	if(entry_present_in_cache == 0)
	{
		FILE *fp_file_server;
		int cur_min, cur_sec, cur_hr;
		char new_entry[500];
		fp_file_server = fopen("cachedata.txt","a+");
		if(fp_file_server!=NULL)
		{

			file_size = 0;
			struct stat st;
			stat("cachedata.txt", &st);
			file_size = st.st_size;

			char *new_cache = (char*)calloc(1, file_size);
			if(new_cache == NULL)
			{
				perror("No enough size for new_cache in memory\n");
				return ERROR;
			}
			else
			{
			
				get_curr_time(&cur_hr,&cur_min,&cur_sec);

				sprintf(new_entry,"\n%s %d %d %d ",url,cur_hr,cur_min,cur_sec);
				printf("---->Making new entry into cache \n%s\n",new_entry);
				fprintf (fp_file_server,"%s" ,new_entry);
				fclose(fp_file_server);
				fp_file_server = NULL;
				
			}
		}
		else
		{
			perror("Could not locate cachedata.txt file\n");
			return ERROR;
		}
	}
	status status_socket;

	status_socket = socket_creation_remote();
	if(status_socket == ERROR)
	{
		perror("Error on communication between proxy and server");
		return ERROR;
	}

	return SUCCESS;
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

status socket_creation_remote()
{
	char folder_path[500], folder_name[500], filename[500] ;
	remote_socket = socket(AF_INET,SOCK_STREAM,0);// setting the server socket
	if(remote_socket < 0)
	{

		perror("error on socket creation with http server");
		return ERROR;
	}
	memset(&remote_address,0,sizeof(remote_address));

	/*Setting socket so as to reuse the port number*/
	int true = 1;
	if(setsockopt(remote_socket,SOL_SOCKET,SO_REUSEADDR,&true,sizeof(int)) < 0)
	{
		perror("error on setsocket with http server");
		return ERROR;
	}

	printf("\nPort %d is used for remote connection\n",port_number);
	printf("\nConnecting to %s... \n",ip);


	remote_address.sin_family = AF_INET;
	remote_address.sin_addr.s_addr	= inet_addr(ip);
	remote_address.sin_port = htons(port_number);


	// connecting to the remote server
    int status_val = connect(remote_socket,(struct sockaddr*)&remote_address,sizeof(struct sockaddr));

    if(status_val < 0)
    {
    	perror("Error in establishing connection\n");
    	return SKIP_TO_END;
    }

 	else
 	{
 		printf("\n---->Established connection with remote server\n");

 		//Getting the file_path from url
 		char *find_domain, *find_path;
 		find_path = NULL;
 		find_domain = NULL;

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

 						sprintf(filename,"Cache/%s%sindex.html",main_url,find_path);
 					}
 					else
 					{
 						/* http://morse.colorado.edu/images*/

 						sprintf(filename,"Cache/%s%s/index.html",main_url,find_path);
 					}
 				}
 				

 				
 			}
 			else
 			{
 				
 				/* http://morse.colorado.edu                 */
 				sprintf(filename,"Cache/%s/index.html",main_url);
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

 		memset(request_remote,0,sizeof(request_remote));

 		// sending GET request to the server

 		char *browser = strstr(request,"Accept");
 		if(browser == NULL)
 		{
 			if(find_path == NULL)
 			{
 				 sprintf(request_remote,"GET / %s\r\nHost: %s\r\nConnection : close\r\n\r\n",version,main_url);
 			}
 			else
 			{

 				sprintf(request_remote,"GET %s %s\r\nHost: %s\r\nConnection : close\r\n\r\n",find_path,version,main_url);
 			}


 			printf("\n---->Request : \n%s\n",request_remote);

 			if((send(remote_socket,request_remote,strlen(request_remote),0))<0)
 			{
 				perror("Error on sending to remote server\n");
 				return SKIP_TO_END;
 			}
 		}
 		else
 		{
 			 printf("\n---->Request : \n%s\n",request);
 			if((send(remote_socket,request,strlen(request),0))<0)
 			{
 				perror("Error on sending to remote server\n");
 				return SKIP_TO_END;
 			}
 		}

 		//receiving response from server
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
			char cmd[500];

			// creating new folder for cache storage
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
				// receiving data from server
				characters = 0;
				memset(response_remote,0,sizeof(response_remote));
				//setting socket timeout if more than 1 second

				/*struct timeval tv;// for socket timeout
				tv.tv_sec = 10;
				tv.tv_usec = 0;
				setsockopt(remote_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval));*/
				characters = recv(remote_socket,response_remote,BUFFER,0);

				/*tv.tv_sec = 0;
				tv.tv_usec = 0;
				setsockopt(remote_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval));*/

				if(characters < 0)
				{
					printf("Error on loading from server - Timeout\n");
					break;
				}
				fwrite(response_remote, 1, characters,recv_data);
				send(new_socket,response_remote,characters,0);
				//printf("%s",response_remote);

			}while(characters>0);

			printf("\nEnd of File\n");
			fclose(recv_data);
		}
		else
		{
			perror("\nCannot create a file in Cache\n");
			return ERROR;
		}

 			
	}
 	return SUCCESS;
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


#include <stdio.h>
#include <string.h>
#include <libxml/parser.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#define CAM_NUM		32
#define WIFI_NUM	32
#define TAG			"MVR:"
#define MIN(a,b)	((a)<(b)?(a):(b))

pthread_t pth_control;
int run;
int recEna, mosEna, uplEna;
int camcnt;
char *filename;
pid_t uploadPid;

const char *LAYER[] = {"SINGLE", "2x2", "3x3"};
typedef enum layer_s {SINGLE=1, QUAD, NINE} layer_t;
layer_t mosaic = NINE;

typedef struct _Camera
{
	char *name;
	char *url;
	char *decoder;
	char *recdir;
	char *rectime;
	char *latency;
	char *position;
	char *mosaic;
	pid_t recPid;
	pid_t mosPid;
} Camera;
Camera	*camera[CAM_NUM] = {NULL};

typedef struct _Wifi
{
	char *ssid;
	char *password;
} Wifi;

Wifi *wifi[WIFI_NUM] = {NULL};

void* control_func (void *arg);
void sig_handler(int signum);
int startRec(Camera *h);
int startMos(Camera *h);
int startUpload(void);
void change_mosaic(void);
void shift_mosaic(void);

void stopUpload(void)
{
	int ret;
	printf("\n\t%s Stop uploading\n", TAG);
	if(uploadPid != 0)
	{
		ret = kill(uploadPid, SIGINT);
		printf("\tKill uploadPid [%d] return %d\n",uploadPid, ret);
		uploadPid = 0;
	}
}

void free_cameras(void)
{
	int i;
	int ret;
	Camera *h;
	for(i = 0; i < CAM_NUM; i++)
	{
		if(camera[i] != NULL)
		{
			printf("\n\t%s Free camera[%d]\n", TAG, i);
			h = camera[i];
			if(h->recPid != 0)
			{
				ret = kill(h->recPid, SIGINT);
				printf("\tKill recPid [%d] return %d\n",h->recPid, ret);
				h->recPid = 0;
			}
			if(h->mosPid != 0)
			{
				ret = kill(h->mosPid, SIGINT);
				printf("\tKill mosPid [%d] return %d\n",h->mosPid, ret);
				h->mosPid = 0;
			}
			free(h->name);
			free(h->url);
			free(h->decoder);
			free(h->recdir);
			free(h->rectime);
			free(h->latency);
			free(h->mosaic);
			free(h->position);
			free(h);
		}
	}
}

void print_usage(char *arg)
{
	printf("Usage: %s [OPTIONS] xml file\n", arg);
	printf("-r disable recording\n");
	printf("-m disable mosaic\n");
	printf("-u disable upload\n");
	printf("\n######### Runtime options #########\n");
	printf("h - this help\n");
	printf("x - Exit\n");
	printf("l - switch layout (single, quad, nine)\n");
	printf("s - shift camera in layout\n");
	exit(0);
}

const char *get_filename_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

int main(int argc, char **argv)
{
    int			i, retVal;
	Camera		*h;
	Wifi		*w;
	xmlDocPtr	doc;
	xmlNodePtr	cur;
	
    recEna = mosEna = uplEna = 1;
    if(argc >= 2)
    {
		if(strcmp(argv[1],"help") == 0 || strcmp(argv[1],"-help") == 0 || strcmp(argv[1],"--help") == 0 || strcmp(argv[1],"-h") == 0)
			print_usage(argv[0]);

    	for(i = 1; i < argc; i++)
    	{
    		if(argv[i][0] == '-')
    		{
				if(strcmp(argv[i],"-r") == 0)
					recEna = 0;
				else if(strcmp(argv[i],"-m") == 0)
					mosEna = 0;
				else if(strcmp(argv[i],"-u") == 0)
					uplEna = 0;
				else
				{
					printf("\n\t%sERROR!!! Unsupported option\n\n", TAG);
					print_usage(argv[0]);
				}
			}
			else
			{
				filename = argv[i];
			}
    	}
    }
    else
    {
    	print_usage(argv[0]);
    }
    	
    if(filename == NULL)
    {
    	printf("\n\t%s ERROR!!! Please specify filename\n\n", TAG);
    	print_usage(argv[0]);
    }
	else
	{
		if(strcmp(get_filename_ext(filename), "xml") != 0)
		{
	    	printf("\n\t%s ERROR!!! Please specify file with xml extension\n\n", TAG);
	    	print_usage(argv[0]);
		}
	}
	
    printf("%s filename: %s recEna = %d mosEna = %d uplEna = %d\n", TAG, filename, recEna, mosEna, uplEna);

	doc = xmlParseFile(filename);
	if(doc == NULL)
	{
		printf("\t\n%s ERROR!!! Not a valid xml file\n\n", TAG);
		exit(1);
	}
	cur = xmlDocGetRootElement(doc);
    fprintf(stdout, "Root is <%s> (%i)\n", cur->name, cur->type);
	
	cur = cur->xmlChildrenNode;
	camcnt = 0;
	while (cur != NULL)
	{
//    	fprintf(stdout, "Child is <%s> (%i)\n", cur->name, cur->type);
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"Camera")))
		{
			for(i = 0; i < CAM_NUM; i++)
				if(camera[i] == NULL) break;
			h = malloc(sizeof(Camera));
			h->name			= (char*)xmlGetProp(cur, (const xmlChar*)"name");
			h->url			= (char*)xmlGetProp(cur, (const xmlChar*)"url");
			h->decoder		= (char*)xmlGetProp(cur, (const xmlChar*)"decoder");
			h->recdir		= (char*)xmlGetProp(cur, (const xmlChar*)"recdir");
			h->rectime		= (char*)xmlGetProp(cur, (const xmlChar*)"rectime");
			h->latency		= (char*)xmlGetProp(cur, (const xmlChar*)"latency");
			h->mosaic	 	= (char*)xmlGetProp(cur, (const xmlChar*)"mosaic");
			h->position 	= (char*)xmlGetProp(cur, (const xmlChar*)"position");
			h->recPid		= 0;
			h->mosPid		= 0;
			camera[i] 		= h;
			camcnt++;
		}
		if ((!xmlStrcmp(cur->name, (const xmlChar *)"WiFi")))
		{
			for(i = 0; i < WIFI_NUM; i++)
				if(wifi[i] == NULL) break;
			w = malloc(sizeof(Wifi));
			w->ssid 	= (char*)xmlGetProp(cur, (const xmlChar*)"ssid");
			w->password = (char*)xmlGetProp(cur, (const xmlChar*)"password");
			wifi[i] 	= w;
		}
		cur = cur->next;
	}
	
	signal(SIGCHLD, sig_handler);

	for(i = 0; i < CAM_NUM; i++)
	{
		if(camera[i] != NULL)
		{
			h = camera[i];
			if(recEna)
				startRec(h);
			if(mosEna)
				startMos(h);
			printf("\t%s Camera[%d]: %s\n", TAG, i, h->name);
			printf("\t\turl: %s decoder: %s\n", h->url, h->decoder);
			printf("\t\trecdir: %s\n", h->recdir);
			printf("\t\trectime: %s latency: %s mosaic %s position: %s\n", h->rectime, h->latency, h->mosaic, h->position);
			printf("\t\trecPid: %d mosPid: %d\n", h->recPid, h->mosPid);
		}
	}

	if(uplEna)
		startUpload();
	
	for(i = 0; i < WIFI_NUM; i++)
	{
		if(wifi[i] != NULL)
		{
			w = wifi[i];
			printf("\t%s WiFi: %s password %s\n", TAG, w->ssid, w->password);
		}
	}
	
	run = 1;
	retVal = pthread_create(&pth_control, NULL, &control_func, argv[0]);
	if (retVal != 0)
		printf("\n%s can't create thread :[%s]", TAG, strerror(retVal));
	else
	    printf("\n%s Control thread created successfully\n", TAG);

	while(run)
	{
		for(i = 0; i < CAM_NUM; i++)
		{
			if(camera[i] != NULL)
			{
				h = camera[i];
//				printf("Camera[%d]: rec - %d mosaic - %d\n", i, h->recPid, h->mosPid);
				if(h->recPid && recEna)
				{
					retVal = kill(h->recPid, 0);
//					printf("kill pid %d with 0 return %d\n", h->recPid, retVal);
					if(retVal == -1)//restart recording
						retVal = startRec(h);
				}
				if(h->mosPid && mosEna)
				{
					retVal = kill(h->mosPid, 0);
//					printf("kill pid %d with 0 return %d\n", h->mosPid, retVal);
					if(retVal == -1)//restart mosaic
						retVal = startMos(h);
				}
			}
		}
		//Monitoring upload process
		if(uploadPid && uplEna)
		{
			retVal = kill(uploadPid, 0);
//					printf("kill pid %d with 0 return %d\n", uploadPid, retVal);
			if(retVal == -1)//restart upload
				retVal = startUpload();
		}

		usleep(500000);
	}
	free_cameras();
	stopUpload();
	xmlFreeDoc(doc);
	return (0);
}

void print_help(char *argv)
{
	printf("h - this help\n");
	printf("x - Exit\n");
	printf("l - switch layout (single, quad, nine)\n");
	printf("s - shift camera in layout\n");
}

void* control_func (void *arg)
{
	char c;
	while(run)
	{
		c = getc(stdin);
		switch(c)
		{
			case 'h':
				print_help(arg);
				break;
			case 'x':
				run = 0;
				break;
			case 'l':
				if(mosEna)
					change_mosaic();
				else
					printf("\t\nMOSAIC DISABLED!!!\n");
				break;
			case 's':
				if(mosEna)
					shift_mosaic();
				else
					printf("\t\nMOSAIC DISABLED!!!\n");
				break;
		}
		usleep(100000);
	}
	return NULL;
}

/*
Get SIGCHLD
*/
void sig_handler(int signum)
{
	int wstatus;
	
//    printf("\nmvr Received signal %d\n", signum);
	wait(&wstatus);
//    printf("\twait return %d\n", wstatus);
}

int startRec(Camera *h)
{
	if(h->rectime && h->recdir)
	{
		printf("\n\t%s Start recordig for camera %s\n", TAG, h->name);

		h->recPid = fork();
		if(h->recPid == -1)
		{
			perror("fork"); /* произошла ошибка */
			exit(1); /*выход из родительского процесса*/
		}
		if(h->recPid == 0)
		{
			execl("record", " ", h->rectime, h->recdir, h->url, h->decoder, h->name, NULL);
		}
//		sleep(1);
	}
	return 0;
}

int startMos(Camera *h)
{
	if( strlen(h->mosaic) && (atoi(h->position) != 0) )
	{
		printf("\n\t%s Start mosaic %s pos %s for camera %s \n", TAG, h->mosaic, h->position, h->name);
		h->mosPid = fork();
		if(h->mosPid == -1)
		{
			perror("fork"); /* произошла ошибка */
			exit(1); /*выход из родительского процесса*/
		}
		if(h->mosPid == 0)
		{
			execl("mosaic", " ", h->url, h->decoder, h->mosaic, h->position, h->latency, h->name, NULL);
		}
	}
//	sleep(1);
	return 0;
}

int startUpload(void)
{
	printf("\n\t%s Start upload\n", TAG);
	uploadPid = fork();
	if(uploadPid == -1)
	{
		perror("fork"); /* произошла ошибка */
		exit(1); /*выход из родительского процесса*/
	}
	if(uploadPid == 0)
	{
		execl("upload", " ", NULL);
	}
	return 0;
}

static void stop_mosaic(bool clearPos)
{
	int i;
	int ret;
	Camera *h;
	for(i = 0; i < CAM_NUM; i++)
	{
		if(camera[i] != NULL)
		{
			h = camera[i];
			//Set new mosaic type for all cameras in list
			sprintf(h->mosaic, "%d", mosaic);
			//Clear positions for all cameras only when change mosaic type
			if(clearPos)
				sprintf(h->position, "%d", 0);
				
			if(h->mosPid != 0)
			{
				ret = kill(h->mosPid, SIGINT);
				printf("\tKill mosPid [%d] return %d\n",h->mosPid, ret);
				h->mosPid = 0;//For stop monitoring mosaic for this camera
			}
		}
	}
}

void change_mosaic(void)
{
	int i;
	Camera *h;
	mosaic++;
	if (mosaic > NINE) mosaic = SINGLE;
	printf("%s Change mosaic to %s\n", TAG, LAYER[mosaic-1]);
	stop_mosaic(true);
	//Assign positions for cameras
	for(i = 0; i < MIN(mosaic*mosaic, camcnt); i++)
	{
		if(camera[i] != NULL)
		{
			h = camera[i];
			sprintf(h->position, "%d", i + 1);
			startMos(h);
		}
	}
}

void shift_mosaic(void)//TODO with parameter
{
	int i, cnt;
	printf("%s %s\n", TAG, __func__);
	Camera *h;
	int wndcnt = mosaic*mosaic;

	stop_mosaic(false);

	//Search for camera with position 1	
	for(i = 0; i < CAM_NUM; i++)
	{
		if(camera[i] != NULL)
		{
			h = camera[i];
			if(atoi(h->position) == 1)
			{
				sprintf(camera[i]->position, "%d", 0);//Clear camera position 1 
				break;
			}
		}
	}
	cnt = 0;
	//Assign position in mosaic for cameras
	//Start from next camera in list
	for(i = i + 1; i < camcnt; i++)
	{
		h = camera[i];
		sprintf(h->position, "%d", ++cnt);
		startMos(h);
		if(cnt == wndcnt) break;
	}
	//Workaround in case of reaching the end of the list 
	//when not busy with all the mosaic window
	if(cnt < MIN(wndcnt, camcnt))//in case of camcnt < wndcnt
	{
		for(i = 0; i < camcnt; i++)
		{
			h = camera[i];
			sprintf(h->position, "%d", ++cnt);
			startMos(h);
			if(cnt == wndcnt || cnt == camcnt) break;//in case of camcnt < wndcnt
		}
	}
}


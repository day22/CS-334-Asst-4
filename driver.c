#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include "png.h"
#include <dirent.h>
#include <semaphore.h>
#include <pthread.h>
#include <errno.h>
#include "colorConvert.c"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>

#define KEY ftok("hw4", 65)

sem_t semaphore;


int thread_solution(DIR* directory, int n, char* folderName, int numThreads);
void thread_method(char* args[]);
void process_solution(DIR* directory, int n, char* folderName, int numThreads);

int main(int argc, char *argv[])
{
    clock_t start;
    clock_t end;

    // check for right number of arguments
    if (argc != 4)
    {
        perror("Usage: ./driver <n:int> <s:char> <folder:char>");
        return EXIT_FAILURE;
    }


    int n = atoi(argv[1]);
    char *selector = argv[2];
    const char *folderName = argv[3];

    sem_init(&semaphore, 0, n);


    DIR *directory = opendir(folderName);

    if (directory == NULL)
    {
        perror("Failed to open directory");
        return EXIT_FAILURE;
    }


    if (strcmp(selector, "t") == 0) {
        thread_solution(directory, n, folderName, n);
    } else if (strcmp(selector, "p") == 0) {
        start = clock();
        process_solution(directory, n, folderName, n);
        end = clock();           
    } else {
        perror("Usage: <s:char> must be p or t");
    }
    
    closedir(directory);

    double time_elapsed = (((double) end - (double) start)/ CLOCKS_PER_SEC);
    printf("Total time: %fs\n", time_elapsed);
    return EXIT_SUCCESS;
} 


int thread_solution(DIR* directory, int n, char* folderName, int numThreads) {
    
    char* root = "/home/day22/P_Drive/OS/hw4/"; 
    struct dirent *directory_ent;
    pthread_t threads[n];   
    int counter = 0;
    while ((directory_ent = readdir(directory)) != NULL)
    {
        // //create a new thread for each file while the semaphore is available
        // sem_wait(&semaphore);        

        //if parent dir or current dir skip
        if (!strcmp(directory_ent->d_name, ".") || !strcmp(directory_ent->d_name, ".."))
           continue;

        char dest[50], src[50];
        //strcpy(src, directory_ent->d_name);
        //create arguments for args struct 
        char* fileName = directory_ent->d_name;
        //input file name creation
        strcpy(src, "images/");
        strcat(src , fileName);
        //printf("%s\n", src);

        //output file name creation
        strcpy(dest, "images/");
        strcat(dest, "out_");
        strcat(dest, fileName);
        //printf("%s\n", dest);

        char* args[3];
        args[0] = "./colorConvert";
        args[1] = src;
        args[2] = dest;
        //colorConvert(3 , args);
        
        int rc = pthread_create(&threads[counter%numThreads], NULL, (void*)&thread_method, &args);
        counter++;
        // sem down number 
        // lock struct 
        // load struct 
        // thread create(strcat(folder, fn))
        //printf("[%s]\n", directory_ent->d_name);
        
    }
    void* ret;
    for (int i = 0; i < numThreads; i++)
    {
        pthread_join(threads[i],&ret);
    }
    pthread_exit(0);
}

void thread_method(char* args[]) {
    //create a new thread for each file while the semaphore is available
    sem_wait(&semaphore);
    printf("%s\n", args[2]);
    colorConvert(3 , args);
    sem_post(&semaphore);
}

void process_solution(DIR* directory, int n, char* folderName, int num_process) {

    int sh_id = shmget(KEY, sizeof(int), IPC_CREAT | 0666);
    if (sh_id < 0)
        perror("shmget");

    int* process_count;
    process_count = shmat(sh_id, NULL, 0);
    process_count = n;

    struct dirent *directory_ent;
    while ((directory_ent = readdir(directory)) != NULL)
    {     
        while (process_count < 0)
        {
            //wait until there is an available
            printf("%d", *process_count); 
        }
        pid_t cpid;
        
        if((cpid = fork()) < 0){ //create and check child
        perror("Fork Error");
        }
        if(cpid == 0){ 
            int* semaphore = shmat(sh_id, NULL, 0);
            if (semaphore < 0) 
                perror("c_data");

            semaphore--;
            //if parent dir or current dir skip
            if (!strcmp(directory_ent->d_name, ".") || !strcmp(directory_ent->d_name, ".."))
            continue;

            char dest[50], src[50];
            //strcpy(src, directory_ent->d_name);
            //create arguments for args struct 
            char* fileName = directory_ent->d_name;
            //input file name creation
            strcpy(src, "images/");
            strcat(src , fileName);
            //printf("%s\n", src);

            //output file name creation
            strcpy(dest, "images/");
            strcat(dest, "out_");
            strcat(dest, fileName);
            //printf("%s\n", dest);

            char* args[3];
            args[0] = "./colorConvert";
            args[1] = src;
            args[2] = dest;
            colorConvert(3 , args);  

            semaphore++;
            _Exit(EXIT_SUCCESS);          
        }
    }
    int wpid;
    int rd = 0;
    while ((wpid = wait(&rd)) > 0)
    {
        //do nothing and wait for all threads to return
    }
    shmdt(sh_id);
}
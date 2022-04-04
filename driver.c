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

//key for shared memory
#define KEY ftok("hw4", 65)

//thread pool
pthread_mutex_t mutexQueue;
pthread_cond_t condQueue;

//
typedef struct Task {
    void (*taskFunction)(int, char*[]);
    char* exec;
    char* src;
    char* dest;
} Task;

Task taskQueue[256];
int taskCount = 0;

sem_t semaphore;

int thread_solution(DIR* directory, int n, char* folderName);
void thread_method(char* args[]);
void process_solution(DIR* directory, int n, char* folderName, int numThreads);
void submitTask(Task task);
void executeTask(Task* task);
int threadpool_solution(DIR* directory, int n, char* folderName);

int main(int argc, char *argv[])
{
    //clocks for tracking speed
    clock_t start;
    clock_t end;

    // check for right number of arguments
    if (argc != 4)
    {
        perror("Usage: ./driver <n:int> <s:char> <folder:char>");
        return EXIT_FAILURE;
    }

    //convert cli vars to local
    int n = atoi(argv[1]);

    if (n < 0) 
        perror("Usage: <n:int> must have a greater that 0 value");
    char *selector = argv[2];
    const char *folderName = argv[3];


    //initialize semaphore with number of threads
    sem_init(&semaphore, 0, n);

    //open user defined directory
    DIR *directory = opendir(folderName);

    if (directory == NULL)
    {
        perror("Failed to open directory");
        return EXIT_FAILURE;
    }

    //flow control for selecting between light weight and heavy processes
    if (strcmp(selector, "t") == 0) {
        thread_solution(directory, n, folderName);
    } else if (strcmp(selector, "p") == 0) {
        start = clock();
        process_solution(directory, n, folderName, n);
        end = clock();           
    } else {
        perror("Usage: <s:char> must be p or t");
    }
    
    closedir(directory);

    //calculate and print time elapsed 
    double time_elapsed = (((double) end - (double) start)/ CLOCKS_PER_SEC);
    printf("Total time: %fs\n", time_elapsed);
    return EXIT_SUCCESS;
} 

// thread solution takes in a directory containing png images, the max number of
// threads, the name of the folder containing the images. It then creates up to  
// n light weight thread to execute the colorConver function.
int thread_solution(DIR* directory, int n, char* folderName) {
    // struct to hold file data
    struct dirent *directory_ent;

    //create thread pool 
    pthread_t threads[n];

    // counter to track what process to assign   
    int counter = 0;

    // while there are files left to read
    while ((directory_ent = readdir(directory)) != NULL)
    {    
        //if parent dir or current dir skip
        if (!strcmp(directory_ent->d_name, ".") || !strcmp(directory_ent->d_name, ".."))
           continue;

        char dest[50], src[50];

        //create arguments for args struct 
        char* fileName = directory_ent->d_name;

        //input file name creation
        strcpy(src, "images/");
        strcat(src , fileName);

        //output file name creation same as source with out_ preaprended 
        strcpy(dest, "images/");
        strcat(dest, "out_");
        strcat(dest, fileName);

        char* args[3];
        args[0] = "./colorConvert";
        args[1] = src;
        args[2] = dest;
        
        //create thread to run color convert on current image
        int rc = pthread_create(&threads[counter%n], 
                                NULL, 
                                (void*)&thread_method, &args);

        //increment to next thread
        counter++;
    }

    //join all threads once they have finished 
    void* ret;
    for (int i = 0; i < n; i++)
    {
        pthread_join(threads[i],&ret);
    }
    pthread_exit(0);
}

void* startThread(void* args) {
    while (1) {
        Task task;

        pthread_mutex_lock(&mutexQueue);
        while (taskCount == 0) {
            pthread_cond_wait(&condQueue, &mutexQueue);
        }

        task = taskQueue[0];
        int i;
        for (i = 0; i < taskCount - 1; i++) {
            taskQueue[i] = taskQueue[i + 1];
        }
        taskCount--;
        pthread_mutex_unlock(&mutexQueue);
        executeTask(&task);
    }
}

void submitTask(Task task) {
    pthread_mutex_lock(&mutexQueue);
    taskQueue[taskCount] = task;
    taskCount++;
    pthread_mutex_unlock(&mutexQueue);
    pthread_cond_signal(&condQueue);
}

void executeTask(Task* task) {
    char* args[] = {task->exec, task->src, task->dest}; 
    task->taskFunction(3, args);
}

int threadpool_solution(DIR* directory, int n, char* folderName) {

    pthread_t th[n];
    pthread_mutex_init(&mutexQueue, NULL);
    pthread_cond_init(&condQueue, NULL);
    int i;
    for (i = 0; i < n; i++) {
        if (pthread_create(&th[i], NULL, &startThread, NULL) != 0) {
            perror("Failed to create the thread");
        }
    }
    printf("pthreads created");
    // struct to hold file data
    struct dirent *directory_ent;
    while ((directory_ent = readdir(directory)) != NULL)
    {
        //if parent dir or current dir skip
        if (!strcmp(directory_ent->d_name, ".") || !strcmp(directory_ent->d_name, ".."))
           continue;

        char dest[50], src[50];

        //create arguments for args struct 
        char* fileName = directory_ent->d_name;

        //input file name creation
        strcpy(src, "images/");
        strcat(src , fileName);

        //output file name creation same as source with out_ preaprended 
        strcpy(dest, "images/");
        strcat(dest, "out_");
        strcat(dest, fileName);

        Task t = {
            .taskFunction = &colorConvert,
            .exec = "./colorConvert",
            .src = src,
            .dest = dest
        };
        printf("taskAdded %s", t.src);
        submitTask(t);

    }
    for (i = 0; i < n; i++) {
        if (pthread_join(th[i], NULL) != 0) {
            perror("Failed to join the thread");
    }
    pthread_mutex_destroy(&mutexQueue);
    pthread_cond_destroy(&condQueue);
    return 0;
        

    }
}

// thread method protects the concurrency by using the semaphore to track
// the number of threads that can run.
void thread_method(char* args[]) {
    //down the semaphore 
    sem_wait(&semaphore);

    // run color convert on file
    colorConvert(3 , args);

    //up the semaphore
    sem_post(&semaphore);
}

// proceses solution uses a shared memory int to track the number of child 
// processes currently active. The method will fork children to run the 
// colorConvert method but will block if the number of current children 
// exceeds the user defined amount 
void process_solution(DIR* directory, int n, char* folderName, int num_process) {

    // create shared token
    int sh_id = shmget(KEY, sizeof(int), IPC_CREAT | 0666);
    if (sh_id < 0)
        perror("shmget");

    // attach token to parent process
    int* process_count;
    process_count = shmat(sh_id, NULL, 0);
    process_count = n;

    // while there are more files to read
    struct dirent *directory_ent;
    while ((directory_ent = readdir(directory)) != NULL)
    {     
        // if processes are all used block
        while (process_count < 0)
        {
            //wait until there is an available
        }

        // child pid
        pid_t cpid;
        
        // fork a child
        if((cpid = fork()) < 0){ //create and check child
            perror("Fork Error");
        }

        //in child
        if(cpid == 0){ 

            // attach shared memory to child
            int* semaphore = shmat(sh_id, NULL, 0);
            if (semaphore < 0) 
                perror("c_data");

            // down the semaphore in new as new process is created 
            semaphore--;

            //if parent dir or current dir skip
            if (!strcmp(directory_ent->d_name, ".") || !strcmp(directory_ent->d_name, ".."))
            continue;

            //input file name creation
            char dest[50], src[50];
            char* fileName = directory_ent->d_name;
            
            strcpy(src, "images/");
            strcat(src , fileName);

            //output file name creation
            strcpy(dest, "images/");
            strcat(dest, "out_");
            strcat(dest, fileName);

            char* args[3];
            args[0] = "./colorConvert";
            args[1] = src;
            args[2] = dest;
            colorConvert(3 , args);  

            semaphore++;
            _Exit(EXIT_SUCCESS);          
        }
    }
    // wait for all child processes to complete before returning
    int wpid;
    int rd = 0;
    while ((wpid = wait(&rd)) > 0)
    {
        //do nothing and wait for all threads to return
    }
    shmdt(sh_id);
}
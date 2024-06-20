#include <string>
#include <iostream>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <stdlib.h>
#include <sys/stat.h>
#include <algorithm>
#include <sched.h>

#define inference_pipe "./dnn_inference_pipe"
#define response_pipe "./scheduling_pipe"
#define request_pipe "./request_pipe"
using namespace std;

typedef struct dnn{
    int pid;
    int priority;
    int request_fd;
    int response_fd;
    dnn* next;
}DNN;

typedef struct regist_msg{
    int pid;
    int priority;
}reg_msg;

typedef struct request_msg{
    int priority;
}req_msg;

typedef struct response_msg{
    int infer;
    int dnn;
}res_msg;

typedef struct DNN_queue{
    int size;
    DNN* front;
}dnn_queue;
int open_pipe(const char* pipe_name, int mode);
void enqueue(DNN* dnn, regist_msg* msg, dnn_queue* queue);
int new_fdset(fd_set *read_fd, int inference_fd, dnn_queue* queue);
void Registration(dnn_queue* queue, int inference_fd);
int dequeue(dnn_queue* queue);

int open_pipe(const char* pipe_name, int mode){
    int pipe_fd;
    if( access(pipe_name, F_OK) == 0){
        remove(pipe_name);
 
    }
    if( mkfifo(pipe_name, 0666) == -1){
        printf("[Error]Failed to make pipe\n");
        exit(1);
    }

    if( (pipe_fd = open(pipe_name, mode)) < 0){
        printf("[ERROR]Fail to open channel for %s\n", pipe_name);
        exit(-1);
    }
    if(mode == O_WRONLY){
        printf("Successfully opened File ! : Write\n");
    }else{
        printf("Successfully opened File ! : Read\n");
    }
    
    return pipe_fd;
}

int dequeue(dnn_queue* queue){
    DNN* tmp = queue->front;
    queue->front = tmp->next;
    queue->size--;
    printf("DNN(PID : %d) will inference !\n", tmp->pid);
    return tmp->response_fd;
}

int new_fdset(fd_set *read_fd, int inference_fd, dnn_queue* queue){
    FD_ZERO(read_fd);
    FD_SET(inference_fd, read_fd);
    if(queue->size > 0){
        DNN * dnn =  queue->front;
        while(dnn != NULL){
            FD_SET(dnn -> request_fd, read_fd);
            dnn = dnn -> next;
        }
        return queue -> front ->request_fd;
    }
    return inference_fd;
}
void Registration(dnn_queue* queue, int inference_fd){
    regist_msg * msg = new regist_msg();
    while(read(inference_fd, msg, 2*sizeof(int)) > 0){
        DNN * Dnn = new DNN();
        enqueue(Dnn, msg, queue);
    }
}

void enqueue(DNN* dnn, regist_msg* msg, dnn_queue* queue){
    dnn->pid = msg->pid;
    dnn->priority = msg->priority;
    string request_fd = "./request_fd_" + to_string(dnn->pid);
    string response_fd = "./response_fd_" + to_string(dnn->pid);
    dnn->request_fd = open_pipe(request_fd.c_str(), O_RDONLY);
    dnn->response_fd = open_pipe(response_fd.c_str(), O_WRONLY);
    if(queue->size == 0){
        queue->front = dnn;
        queue->size++;
        printf("Inference message from DNN(PID) : %d\n",dnn->pid);
        printf("-----------------------------------\n");
        return;
    }

    if(queue->front->priority <= dnn->priority){
        dnn->next = queue->front;
        queue->front = dnn;
    }else{
        DNN* tmp = queue->front;
        while(tmp->next != NULL && tmp->next->priority > dnn->priority){
            tmp = tmp->next;
        }
        dnn->next = tmp->next;
        tmp->next = dnn;
    }
    queue->size++;
    printf("Inference message from DNN(PID) : %d\n",dnn->pid);
    printf("-----------------------------------\n");
}

int main(){

    int inference_fd;
    int num_of_dnns;
    int max_fd;
    int target_dnn_fd;
    fd_set fdset;
    dnn_queue* Queue;
    DNN* dnn;
    inference_fd = open_pipe(inference_pipe, O_RDONLY | O_NONBLOCK); 
    printf("Inference start !\nScheduler is Running..\n\n");

    do{
        regist_msg* reg_msg;
        request_msg* req_msg;
        DNN* reg_dnn;
        
        max_fd = new_fdset(&fdset, inference_fd, Queue);
        if(select(max_fd +1, &fdset, NULL, NULL, NULL)>0){
            if(FD_ISSET(inference_fd, &fdset)) {
                Registration(Queue, inference_fd);
            }
        }
        
        for(dnn = Queue->front ; dnn != NULL ; dnn = dnn->next){
            FD_SET(dnn->request_fd, &fdset);
        }
        
        for(dnn = Queue->front ; dnn != NULL ; dnn = dnn->next){
            if(FD_ISSET(dnn->request_fd,&fdset)){     
                printf("Request from DNN(PID) : %d\n",dnn->pid);
            }
        }
        printf("\n");

        if(Queue->size > 1 && Queue->front->priority >=3){
            response_msg* res_msg = new response_msg();
            res_msg->dnn = Queue->front->pid;
            target_dnn_fd = dequeue(Queue);
            if(write(target_dnn_fd, res_msg ,sizeof(res_msg))<0){
                printf("Error!\n");
            }
            delete res_msg;

        }else{
            sleep(3);
        }
    }while(Queue->size>0); // DNN이 모두 inference를 끝날 때까지 반복
    
    return 0;
    
}

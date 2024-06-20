import torch
import torchvision.models as models
import os
from torch import nn
import struct
import time
import random
import fcntl

global register_fd
global response_fd
global request_fd
register_fd = None
request_fd = None
response_fd = None
class Add(nn.Module):
    def __init__(self, priority):
        super().__init__()
        self.priority = priority
    
    def get_priority(self):
        return self.priority

    def forward(self, x1, x2):
        output = torch.add(x1, x2)

        return output

# 모델 생성
add = Add(25)

def registration(self,input):
    global register_fd
    register_fd = os.open("./dnn_inference_pipe", os.O_WRONLY| os.O_NONBLOCK)
    flags = fcntl.fcntl(register_fd, fcntl.F_GETFL)
    flags |= os.O_NONBLOCK
    fcntl.fcntl(register_fd, fcntl.F_SETFL, flags)
    
    msg = [os.getpid(),random.randint(1,10)]
    print("My PID, Priority : ", msg[0], msg[1])
    bytemsg = msg[0].to_bytes(4, byteorder='little') + msg[1].to_bytes(4, byteorder='little')
    os.write(register_fd, bytemsg)
    
    print("DNN starts inference !")
    time.sleep(1)
    
def scheduling(module, input):
    global response_fd
    global request_fd

    msg = [random.randint(1,10)]
    
    while request_fd is None:
        try:
            request_fd = os.open("./request_fd_{:d}".format(os.getpid()), os.O_WRONLY)
        except:
            pass
    
    ## byte_msg = msg[1].to_bytes(4, byteorder='little')
    byte_msg = msg[0].to_bytes(4, byteorder='little')

    os.write(request_fd,byte_msg)
    
    print("DNN requests to Scheduler !")
    time.sleep(1)
    
    while response_fd == None:  
        try:
            response_fd = os.open("./response_fd_{:d}".format(os.getpid()), os.O_RDONLY)
        except:
            pass
    dnn_num = os.read(response_fd, 4)
    dnn_num = int.from_bytes(dnn_num, 'little')
    print("DNN receives the message from Scheduler")
    

    
x1 = torch.rand(1)
x2 = torch.rand(1)

add.register_forward_pre_hook(registration)

add.register_forward_pre_hook(scheduling)

output = add(x1, x2)



# Malloc Lab

### 成分说明： 	

显式空闲链表+LIFO，best-fit分配策略，以及替代版realloc。显式空闲链表中的指针改为偏移量，前驱和后继的占用空间改为4字节，最小块大小降为16，同时也使得本方法64/32位机器都适用。

- 最终分数：99


### 食用方法：

`gcc -Wall -O2 -c -o mm.o mm-true.c && gcc -Wall -O2  -o mdriver mdriver.o mm.o memlib.o fsecs.o fcyc.o clock.o ftimer.o`


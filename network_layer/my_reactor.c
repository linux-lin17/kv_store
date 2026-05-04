
#include "reactor.h"


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>




#define CONNECTION_SIZE     1024

#define MAX_PORTS           20

#define TIME_SUB_MS(tv1, tv2)  ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)


#if ENABLE_KVSTORE

typedef int (*msg_handler)(char *msg, int length, char *response);

static msg_handler kvs_handler;

int kvs_request(struct conn *c) {    // 在 recv_cb 中调用

	c->wlength = kvs_handler(c->rbuffer, c->rlength, c->wbuffer);

    return c->wlength;
}

int kvs_response(struct conn *c) {   // 在 send_cb 中调用

	
    return 0;
}


#endif


int accept_cb(int fd);
int recv_cb(int fd);
int send_cb(int fd);


int epfd = 0;

// 管理 sockfd (listenfd 和 clientfd)
static struct conn *conn_list = NULL;

struct timeval begin;


// 端口非阻塞

int set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return flags;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 通过 epoll --> 管理 fd (sockfd 和 clientfd)
int set_event(int fd, int event, int flag) {

    struct epoll_event ev;
    ev.events = event;
    ev.data.fd = fd;

    if (flag) {     // 非 0 --> add

        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    } else {        // 0 --> mod

        epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
    }
    return 0;
}

// 设置事件 --> 调用 set_event 接口 --> 向红黑树中添加 clientfd

int event_register(int fd, int event){

    if (fd < 0) return -1;

    conn_list[fd].fd = fd;
    conn_list[fd].r_action.recv_callback = recv_cb;   // clienfd --> r_action 是 recv_cb
    conn_list[fd].send_callback = send_cb;

    memset(conn_list[fd].rbuffer, 0, BUFFER_LENGTH);
    conn_list[fd].rlength = 0;

    memset(conn_list[fd].wbuffer, 0, BUFFER_LENGTH);
    conn_list[fd].wlength = 0;
    // 调用 set_event, 向 epoll 添加 clientfd
    set_event(fd, event, 1);

    return 0;
}

// listenfd(sockfd) 触发 --> 监听到 client 端连接请求 --> 调用 event_register 接口

#if 1
int accept_cb(int fd) {
    
    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(clientaddr);

    while(1) {

        int clientfd = accept(fd, (struct sockaddr*)&clientaddr, &len);

        if (clientfd < 0) {
            // 如果是因为没人排队了而返回的错误，说明掏空了，完美收工！
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; 
            } else if (errno == EINTR) {
                continue; // 被系统信号打断，继续接客
            } else {
                printf("accept errno: %d --> %s\n", errno, strerror(errno));
                break; // 真出错了，撤退
            }
        }

        // 连进来的新客户端，也必须立刻设为非阻塞！否则 recv 会卡死服务器！
        set_nonblock(clientfd);

        // 调用 event_register
        event_register(clientfd, EPOLLIN); // 注册新接受的客户端 clientfd，只关注可读事件

        if ((clientfd % 1000) == 0) {

            struct timeval current;
            gettimeofday(&current, NULL);

            int time_used = TIME_SUB_MS(current, begin);
            memcpy(&begin, &current, sizeof(struct timeval));
            

            printf("accept finished: %d, time_used: %d\n", clientfd, time_used);
        
        }
	}  

    return 0;
}
#elif 0

int accept_cb(int fd) {
	struct sockaddr_in  clientaddr;
	socklen_t len = sizeof(clientaddr);

	int clientfd = accept(fd, (struct sockaddr*)&clientaddr, &len);
	//printf("accept finshed: %d\n", clientfd);
	if (clientfd < 0) {
		printf("accept errno: %d --> %s\n", errno, strerror(errno));
		return -1;
	}
	
	event_register(clientfd, EPOLLIN);  // | EPOLLET

	if ((clientfd % 1000) == 0) {

		struct timeval current;
		gettimeofday(&current, NULL);

		int time_used = TIME_SUB_MS(current, begin);
		memcpy(&begin, &current, sizeof(struct timeval));
		

		printf("accept finshed: %d, time_used: %d\n", clientfd, time_used);

	}

	return 0;
}

#endif


// server 端接收
int recv_cb(int fd) {

    memset(conn_list[fd].rbuffer, 0, BUFFER_LENGTH);
    int count = recv(fd, conn_list[fd].rbuffer, BUFFER_LENGTH, 0);
    
    if (count == 0) {
        //printf("client disconnect: %d\n", fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
		close(fd);

        return -1;
    } else if (count < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return 0;
		printf("count: %d, errno: %d, %s\n", count, errno, strerror(errno));
        
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
        close(fd);
        return -2;
    }

    conn_list[fd].rlength = count; 


#if 0
	printf("[%d]RECV: %s\n", conn_list[fd].rlength, conn_list[fd].rbuffer);

    // rbuffer --> 拷贝到 wbuffer, 为 send 做准备
    conn_list[fd].wlength = conn_list[fd].rlength;
    memcpy(conn_list[fd].wbuffer, conn_list[fd].rbuffer, conn_list[fd].wlength);


#elif ENABLE_KVSTORE

    kvs_request(&conn_list[fd]);   //  具体逻辑由核心层控制

#endif

    set_event(fd, EPOLLOUT, 0);

    return count;
}


// server 向 client 发送
int send_cb(int fd) {

#if ENABLE_KVSTORE
    kvs_response(&conn_list[fd]);

#endif

    int count = 0;
    if (conn_list[fd].wlength != 0) {
        count = send(fd, conn_list[fd].wbuffer, conn_list[fd].wlength, 0);
    }

    set_event(fd, EPOLLIN, 0);

    return count;
}

// 初始化 sever 端
int r_init_server(unsigned short port) {

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    set_nonblock(sockfd);
    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    // 端口复用
	int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    if (-1 == bind(sockfd, (struct sockaddr*)&servaddr, sizeof(struct sockaddr))) {
        printf("bind failed: %s\n", strerror(errno));
        return -1;
    }

    // sockfd 开启监听
    listen(sockfd, 10);

    return sockfd;
}


int reactor_start(unsigned short port, msg_handler handler){

    printf("net_layer : reactor \n");

    kvs_handler = handler;

    conn_list = (struct conn*)malloc(CONNECTION_SIZE * sizeof(struct conn));
    if (!conn_list) { perror("malloc"); return -1; }


	//unsigned short port = 2000;

	epfd = epoll_create(1);    

    for (int i = 0; i < MAX_PORTS; i ++) {
        int sockfd = r_init_server(port + i);

        if (sockfd < 0) {  // ← 加这个检查
            printf("init_server failed for port %d\n", port + i);
            continue;
        }

        conn_list[sockfd].fd = sockfd;
        conn_list[sockfd].r_action.accept_callback = accept_cb;  // sockfd --> r_action 是 accept_cb

        set_event(sockfd, EPOLLIN, 1);   // 向 epoll 添加 sockfd

        printf("listenfd [%d] has been prepared \n", sockfd);
    }

    gettimeofday(&begin, NULL);

    struct epoll_event events[1024] = {0};
    while(1) {
        
        int nready = epoll_wait(epfd, events, 1024, -1);
        
        for (int i = 0; i < nready; i ++) {
            int connfd = events[i].data.fd;

            if (events[i].events & EPOLLIN) {
                conn_list[connfd].r_action.recv_callback(connfd);   // 具体调用 accept_cb 还是 recv_cb , 由 connfd 决定
            } 

            if (events[i].events & EPOLLOUT) {
                conn_list[connfd].send_callback(connfd);
            }
        }
    }   



    return 0;

}



#include <stdio.h>
#include <stdlib.h>
#include <liburing.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>


#define EVENT_ACCEPT   	0
#define EVENT_READ		1
#define EVENT_WRITE		2

#define BUFFER_LENGTH	1024

#define	MAX_PORTS		20	
#define	CONNECTION_SIZE		1024

typedef int (*msg_handler)(char *msg, int length, char *response);
static msg_handler kvs_handler;


// ===== 轻量级 user_data 结构：只存必要信息 =====
struct sqe_user_data {
    int fd;
    int event;
};

struct conn_info {
	struct sqe_user_data user_data;

	char rbuffer[BUFFER_LENGTH];
	int rlength;

	char wbuffer[BUFFER_LENGTH];
	int wlength;
};


// 管理 sockfd (listenfd 和 clientfd)
static struct conn_info *conn_list = NULL;


int p_init_server(unsigned short port) {	

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);	
    if (sockfd < 0) {
        return -1;
    }
	// 设置端口复用
	int opt = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		perror("setsockopt SO_REUSEADDR");
    }

	struct sockaddr_in serveraddr;	
	memset(&serveraddr, 0, sizeof(struct sockaddr_in));	
	serveraddr.sin_family = AF_INET;	
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);	
	serveraddr.sin_port = htons(port);	

	if (-1 == bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(struct sockaddr))) {		
		perror("bind");		
		return -1;	
	}	

	listen(sockfd, 10);
	
	return sockfd;
}



#define ENTRIES_LENGTH		1024
#define BUFFER_LENGTH		1024

int set_event_recv(struct io_uring *ring, int sockfd,
				      void *buf, size_t len, int flags) {
	
	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

	struct sqe_user_data recv_info = {
		.fd = sockfd,
		.event = EVENT_READ,
	};
	
	memset(conn_list[sockfd].rbuffer, 0, BUFFER_LENGTH);

	io_uring_prep_recv(sqe, sockfd, conn_list[sockfd].rbuffer, len, flags);
	memcpy(&sqe->user_data, &recv_info, sizeof(struct sqe_user_data));

    return 0;
}


int set_event_send(struct io_uring *ring, int sockfd,
				      void *buf, size_t len, int flags) {

	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

	struct sqe_user_data send_info = {
		.fd = sockfd,
		.event = EVENT_WRITE, 
	};

	memcpy(conn_list[sockfd].wbuffer, buf, len);
	
	io_uring_prep_send(sqe, sockfd, conn_list[sockfd].wbuffer, len, flags);
	memcpy(&sqe->user_data, &send_info, sizeof(struct sqe_user_data));

    return 0;
}



int set_event_accept(struct io_uring *ring, int sockfd, struct sockaddr *addr,
					socklen_t *addrlen, int flags) {

	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

	struct sqe_user_data accept_info = {
		.fd = sockfd,
		.event = EVENT_ACCEPT,
	};
	
	io_uring_prep_accept(sqe, sockfd, (struct sockaddr*)addr, addrlen, flags);
	memcpy(&sqe->user_data, &accept_info, sizeof(struct sqe_user_data));

    return 0;
}




int proactor_start(unsigned short port, msg_handler handler) {

    printf("net_layer : proactor \n");

    conn_list = (struct conn_info*)malloc(CONNECTION_SIZE * sizeof(struct conn_info));

	kvs_handler = handler;

	struct io_uring_params params;
	memset(&params, 0, sizeof(params));

	struct io_uring ring;
	io_uring_queue_init_params(ENTRIES_LENGTH, &ring, &params);

	for (int i = 0; i < MAX_PORTS; i ++) {
		int listenfd = p_init_server(port + i);
		conn_list[listenfd].user_data.fd = listenfd;
		
#if 0
		struct sockaddr_in clientaddr;	
		socklen_t len = sizeof(clientaddr);
		accept(sockfd, (struct sockaddr*)&clientaddr, &len);
#elif 0

		struct sockaddr_in clientaddr;	
		socklen_t len = sizeof(clientaddr);
#else
		set_event_accept(&ring, listenfd, NULL, NULL, 0);
		
#endif
		printf("listenfd [%d] has been prepared \n", listenfd);
	}


	// char buffer[BUFFER_LENGTH] = {0};
    // char response[BUFFER_LENGTH] = {0};

	while (1) {

		io_uring_submit(&ring);


		struct io_uring_cqe *cqe;
		io_uring_wait_cqe(&ring, &cqe);

		struct io_uring_cqe *cqes[128];
		int nready = io_uring_peek_batch_cqe(&ring, cqes, 128);  // epoll_wait

		int i = 0;
		for (i = 0;i < nready;i ++) {

			struct io_uring_cqe *entries = cqes[i];
			struct sqe_user_data result;
			memcpy(&result, &entries->user_data, sizeof(struct sqe_user_data));

            int res = entries->res;
            if (res < 0) {
				printf("发生错误了! event类型: %d, 错误码: %d\n", result.event, res);
				
                // 如果是读或者写报错了，说明这个客户端连接废了，直接关掉
				if (result.event == EVENT_READ || result.event == EVENT_WRITE) {
					close(result.fd);
				}
				
                continue;
            }

			if (result.event == EVENT_ACCEPT) {

				
				// 重新提交 listenfd , result 中的 fd 是之前注册过的 listenfd
				int listenfd = result.fd;

				set_event_accept(&ring, listenfd, NULL, NULL, 0);
				//printf("set_event_accept\n"); //

				// 建立连接的时候, entries->res 是一个新的 fd !!! 
				int clientfd = entries->res;    

				set_event_recv(&ring, clientfd, conn_list[clientfd].rbuffer, BUFFER_LENGTH, 0);

				
			} else if (result.event == EVENT_READ) {  //

				int ret = entries->res;   // read 的时候, entries->res 返回 recv() 的长度

				int clientfd = result.fd;

				//printf("set_event_send ret: %d, buffer[before] : %s, response : %s\n", ret, conn_list[clientfd].rbuffer, conn_list[clientfd].wbuffer); //

				if (ret == 0) {
					close(clientfd);
				} else if (ret > 0) {

					
					conn_list[clientfd].rlength = ret;

                    ret = kvs_handler(conn_list[clientfd].rbuffer, ret, conn_list[clientfd].wbuffer);

					set_event_send(&ring, result.fd, conn_list[clientfd].wbuffer, ret, 0);

					// 处理完之后, buffer 即使清空 --> 防止下一次的数据污染
					//memset(buffer, 0, BUFFER_LENGTH);
					
					//printf("set_event_send ret: %d, buffer[after] : %s, response : %s\n", ret, conn_list[clientfd].rbuffer, conn_list[clientfd].wbuffer); //
				}
			}  else if (result.event == EVENT_WRITE) {
  //
				int clientfd = result.fd;

				//int ret = entries->res;
				//printf("set_event_send ret: %d, %s\n", ret, buffer);

				set_event_recv(&ring, clientfd, conn_list[clientfd].rbuffer, BUFFER_LENGTH, 0);
				
			}
			
		}

		io_uring_cq_advance(&ring, nready);
	}

}



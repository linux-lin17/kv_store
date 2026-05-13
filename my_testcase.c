

#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#define BEGIN_TEST		1

#define MAX_MSG_LENGTH      1024

#define TIME_SUB_MS(tv1, tv2) \
	((long)(((tv1.tv_sec - tv2.tv_sec) * 1000000L + (tv1.tv_usec - tv2.tv_usec)) / 1000))


int send_msg(int connfd, char *msg, int length) {

	int res = send(connfd, msg, length, 0);
	if (res < 0) {
		perror("send");
		exit(1);
	}
	return res;
}

int recv_msg(int connfd, char *msg, int length) {

	int res = recv(connfd, msg, length, 0);
	if (res < 0) {
		perror("recv");
		exit(1);
	}
	return res;

}

int connect_tcpserver(const char *ip, unsigned short port) {

	int connfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(struct sockaddr_in));

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip);
	server_addr.sin_port = htons(port);

	if (0 !=  connect(connfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in))) {
		perror("connect");
		return -1;
	}
	
	return connfd;
	
}

void testcase(int connfd, char *msg, char *pattern, char *casename) {

	if (!msg || !pattern || !casename) return ;

	send_msg(connfd, msg, strlen(msg));

	char result[MAX_MSG_LENGTH] = {0};
	recv_msg(connfd, result, MAX_MSG_LENGTH);

	if (strcmp(result, pattern) == 0) {

#if BEGIN_TEST
		printf("==> PASS ->  %s\n", casename);
#endif

	} else {
		printf("==> FAILED -> %s, '%s' != '%s' \n", casename, result, pattern);
		exit(1);
	}

}

void array_testcase(int connfd) {

	testcase(connfd, "SET Teacher King", "OK\r\n", "SET-Teacher");
	testcase(connfd, "GET Teacher", "King\r\n", "GET-Teacher");
	testcase(connfd, "MOD Teacher Darren", "OK\r\n", "MOD-Teacher");
	testcase(connfd, "GET Teacher", "Darren\r\n", "GET-Teacher");
	testcase(connfd, "EXIST Teacher", "EXIST\r\n", "GET-Teacher");
	testcase(connfd, "DEL Teacher", "OK\r\n", "DEL-Teacher");
	testcase(connfd, "GET Teacher", "NO EXIST\r\n", "GET-Teacher");
	testcase(connfd, "MOD Teacher KING", "NO EXIST\r\n", "MOD-Teacher");
	testcase(connfd, "EXIST Teacher", "NO EXIST\r\n", "GET-Teacher");

}

void array_testcase_1w(int connfd) {
	printf("array_testcase_1w \n");

	int count = 10000;
	int i = 0;

	struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);

	for (i = 0;i < count;i ++) {

		testcase(connfd, "SET Teacher King", "OK\r\n", "SET-Teacher");
		testcase(connfd, "GET Teacher", "King\r\n", "GET-Teacher");
		testcase(connfd, "MOD Teacher Darren", "OK\r\n", "MOD-Teacher");
		testcase(connfd, "GET Teacher", "Darren\r\n", "GET-Teacher");
		testcase(connfd, "EXIST Teacher", "EXIST\r\n", "GET-Teacher");
		testcase(connfd, "DEL Teacher", "OK\r\n", "DEL-Teacher");
		testcase(connfd, "GET Teacher", "NO EXIST\r\n", "GET-Teacher");
		testcase(connfd, "MOD Teacher KING", "NO EXIST\r\n", "MOD-Teacher");
		testcase(connfd, "EXIST Teacher", "NO EXIST\r\n", "GET-Teacher");

	}

	struct timeval tv_end;
	gettimeofday(&tv_end, NULL);

	int time_used = TIME_SUB_MS(tv_end, tv_begin);
	int total_ops = count * 9;

	printf("array testcase --> time_used: %dms, total_ops: %d, qps: %d\n",
		time_used, total_ops,
		time_used > 0 ? total_ops * 1000 / time_used : 0);

}

void rbtree_testcase(int connfd) {

	testcase(connfd, "RSET Teacher King", "OK\r\n", "RSET-Teacher");
	testcase(connfd, "RGET Teacher", "King\r\n", "RGET-King-Teacher");
	testcase(connfd, "RMOD Teacher Darren", "OK\r\n", "RMOD-D-Teacher");
	testcase(connfd, "RGET Teacher", "Darren\r\n", "RGET-Darren-Teacher");
	testcase(connfd, "REXIST Teacher", "EXIST\r\n", "REXIST-Teacher");
	testcase(connfd, "RDEL Teacher", "OK\r\n", "RDEL-Teacher");
	testcase(connfd, "RGET Teacher", "NO EXIST\r\n", "RGET-K-Teacher");
	testcase(connfd, "RMOD Teacher KING", "NO EXIST\r\n", "RMOD-K-Teacher");
	testcase(connfd, "REXIST Teacher", "NO EXIST\r\n", "REXIST-Teacher");

}

void rbtree_testcase_1w(int connfd) {

	printf("rbtree_testcase_1w \n");
	int count = 10000;
	int i = 0;

	struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);

	for (i = 0;i < count;i ++) {

		testcase(connfd, "RSET Teacher King", "OK\r\n", "RSET-Teacher");
		testcase(connfd, "RGET Teacher", "King\r\n", "RGET-King-Teacher");
		testcase(connfd, "RMOD Teacher Darren", "OK\r\n", "RMOD-D-Teacher");
		testcase(connfd, "RGET Teacher", "Darren\r\n", "RGET-Darren-Teacher");
		testcase(connfd, "REXIST Teacher", "EXIST\r\n", "REXIST-Teacher");
		testcase(connfd, "RDEL Teacher", "OK\r\n", "RDEL-Teacher");
		testcase(connfd, "RGET Teacher", "NO EXIST\r\n", "RGET-K-Teacher");
		testcase(connfd, "RMOD Teacher KING", "NO EXIST\r\n", "RMOD-K-Teacher");
		testcase(connfd, "REXIST Teacher", "NO EXIST\r\n", "REXIST-Teacher");

	}

	struct timeval tv_end;
	gettimeofday(&tv_end, NULL);

	int time_used = TIME_SUB_MS(tv_end, tv_begin);
	int total_ops = count * 9;

	printf("rbtree testcase --> time_used: %dms, total_ops: %d, qps: %d\n",
		time_used, total_ops,
		time_used > 0 ? total_ops * 1000 / time_used : 0);

}

void rbtree_testcase_3w(int connfd) {

	printf("rbtree_testcase_3w \n");

	int count = 10000;
	int i = 0;

	struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);

	for (i = 0;i < count;i ++) {

		char cmd[128] = {0};
		snprintf(cmd, 128, "RSET Teacher%d King%d", i, i);
		testcase(connfd, cmd, "OK\r\n", "RSET-Teacher");
	}

	for (i = 0;i < count;i ++) {

		char cmd[128] = {0};
		snprintf(cmd, 128, "RGET Teacher%d", i);

		char result[128] = {0};
		snprintf(result, 128, "King%d\r\n", i);
		
		testcase(connfd, cmd, result, "RGET-King-Teacher");
	}

	for (i = 0;i < count;i ++) {

		char cmd[128] = {0};
		snprintf(cmd, 128, "RMOD Teacher%d King%d", i, i);
		testcase(connfd, cmd, "OK\r\n", "RGET-King-Teacher");
	}

	struct timeval tv_end;
	gettimeofday(&tv_end, NULL);

	int time_used = TIME_SUB_MS(tv_end, tv_begin);
	int total_ops = count * 3;

	printf("rbtree testcase --> time_used: %dms, total_ops: %d, qps: %d\n",
		time_used, total_ops,
		time_used > 0 ? total_ops * 1000 / time_used : 0);

}

void hash_testcase(int connfd) {
	printf("hash_testcase \n");

	testcase(connfd, "HSET Teacher King", "OK\r\n", "HSET-Teacher");
	testcase(connfd, "HGET Teacher", "King\r\n", "HGET-King-Teacher");
	testcase(connfd, "HMOD Teacher Darren", "OK\r\n", "HMOD-D-Teacher");
	testcase(connfd, "HGET Teacher", "Darren\r\n", "HGET-Darren-Teacher");
	testcase(connfd, "HEXIST Teacher", "EXIST\r\n", "HEXIST-Teacher");
	testcase(connfd, "HDEL Teacher", "OK\r\n", "HDEL-Teacher");
	testcase(connfd, "HGET Teacher", "NO EXIST\r\n", "HGET-K-Teacher");
	testcase(connfd, "HMOD Teacher KING", "NO EXIST\r\n", "HMOD-K-Teacher");
	testcase(connfd, "HEXIST Teacher", "NO EXIST\r\n", "HEXIST-Teacher");

}

void skiptable_testcase(int connfd){
	printf("skiptable_testcase \n");

	testcase(connfd, "STSET Teacher King", "OK\r\n", "STSET-Teacher");
	testcase(connfd, "STGET Teacher", "King\r\n", "STGET-King-Teacher");
	testcase(connfd, "STMOD Teacher Darren", "OK\r\n", "STMOD-D-Teacher");
	testcase(connfd, "STGET Teacher", "Darren\r\n", "STGET-Darren-Teacher");
	testcase(connfd, "STEXIST Teacher", "EXIST\r\n", "STEXIST-Teacher");
	testcase(connfd, "STDEL Teacher", "OK\r\n", "STDEL-Teacher");
	testcase(connfd, "STGET Teacher", "NO EXIST\r\n", "STGET-K-Teacher");
	testcase(connfd, "STMOD Teacher KING", "NO EXIST\r\n", "STMOD-K-Teacher");
	testcase(connfd, "STEXIST Teacher", "NO EXIST\r\n", "STEXIST-Teacher");

}

void skiptable_testcase_1w(int connfd) {

	printf("skiptable_testcase_1w \n");
	int count = 10000;
	int i = 0;

	struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);

	for (i = 0;i < count;i ++) {
		testcase(connfd, "STSET Teacher King", "OK\r\n", "STSET-Teacher");
		testcase(connfd, "STGET Teacher", "King\r\n", "STGET-King-Teacher");
		testcase(connfd, "STMOD Teacher Darren", "OK\r\n", "STMOD-D-Teacher");
		testcase(connfd, "STGET Teacher", "Darren\r\n", "STGET-Darren-Teacher");
		testcase(connfd, "STEXIST Teacher", "EXIST\r\n", "STEXIST-Teacher");
		testcase(connfd, "STDEL Teacher", "OK\r\n", "STDEL-Teacher");
		testcase(connfd, "STGET Teacher", "NO EXIST\r\n", "STGET-K-Teacher");
		testcase(connfd, "STMOD Teacher KING", "NO EXIST\r\n", "STMOD-K-Teacher");
		testcase(connfd, "STEXIST Teacher", "NO EXIST\r\n", "STEXIST-Teacher");

	}

	struct timeval tv_end;
	gettimeofday(&tv_end, NULL);

	int time_used = TIME_SUB_MS(tv_end, tv_begin); // ms

	int total_ops = count * 9;

	printf("skiptable testcase --> time_used: %dms, total_ops: %d, qps: %d\n",
		time_used, total_ops,
		time_used > 0 ? total_ops * 1000 / time_used : 0);
	
}


void test_wal_1(int connfd) {

	struct timeval tv_begin;
	gettimeofday(&tv_begin, NULL);

    int count = 100000;
    for (int i = 0; i < count; i++) {
        char cmd[256];
        char expected[256];
        char name[256];

        // STSET Teacher0 King0, STSET Teacher1 King1, ...
        snprintf(cmd, sizeof(cmd), "STSET Teacher%d King%d", i, i);
        snprintf(name, sizeof(name), "STSET-Teacher%d", i);
        testcase(connfd, cmd, "OK\r\n", name);

        // STGET Teacher0 → King0, STGET Teacher1 → King1, ...
        snprintf(cmd, sizeof(cmd), "STGET Teacher%d", i);
        snprintf(expected, sizeof(expected), "King%d\r\n", i);
        snprintf(name, sizeof(name), "STGET-Teacher%d", i);
        testcase(connfd, cmd, expected, name);

        // STMOD Teacher0 Darren0, ...
        snprintf(cmd, sizeof(cmd), "STMOD Teacher%d Darren%d", i, i);
        snprintf(name, sizeof(name), "STMOD-Teacher%d", i);
        testcase(connfd, cmd, "OK\r\n", name);

        // HSET Teacher0 King0, ...
        snprintf(cmd, sizeof(cmd), "HSET Teacher%d King%d", i, i);
        snprintf(name, sizeof(name), "HSET-Teacher%d", i);
        testcase(connfd, cmd, "OK\r\n", name);

        // HGET Teacher0 → King0, ...
        snprintf(cmd, sizeof(cmd), "HGET Teacher%d", i);
        snprintf(expected, sizeof(expected), "King%d\r\n", i);
        snprintf(name, sizeof(name), "HGET-Teacher%d", i);
        testcase(connfd, cmd, expected, name);

        // HMOD Teacher0 Darren0, ...
        snprintf(cmd, sizeof(cmd), "HMOD Teacher%d Darren%d", i, i);
        snprintf(name, sizeof(name), "HMOD-Teacher%d", i);
        testcase(connfd, cmd, "OK\r\n", name);
    }

	struct timeval tv_end;
	gettimeofday(&tv_end, NULL);

	int time_used = TIME_SUB_MS(tv_end, tv_begin);
	int total_ops = count * 6;

	printf("test_wal_1 --> time_used: %dms, total_ops: %d, qps: %d\n",
		time_used, total_ops,
		time_used > 0 ? total_ops * 1000 / time_used : 0);
}

void test_wal_2(int connfd) {

	testcase(connfd, "RSET Teacher King", "OK\r\n", "RSET-Teacher");
	testcase(connfd, "RGET Teacher", "King\r\n", "RGET-King-Teacher");
	testcase(connfd, "RMOD Teacher Darren", "OK\r\n", "RMOD-D-Teacher");

}

void test_wal_3(int connfd) {

	//testcase(connfd, "RGET Teacher", "Darren\r\n", "RGET-Darren-Teacher");
	testcase(connfd, "STGET Teacher1024", "Darren1024\r\n", "STGET-Darren-Teacher[1024]");
	testcase(connfd, "HGET Teacher1717", "Darren1717\r\n", "HGET-Darren-Teacher[1717]");
}





// testcase 192.168.47.128 2000 
int main(int argc, char *argv[]) {

	if (argc != 4) {
		printf("arg error\n");
		return -1;
	}

	char *ip = argv[1];
	int port = atoi(argv[2]);
	int mode = atoi(argv[3]);

	int connfd = connect_tcpserver(ip, port);


	if (mode == 0) {
		rbtree_testcase_1w(connfd);
	} else if (mode == 1) {
		rbtree_testcase_3w(connfd);
	} else if (mode == 2) {
		array_testcase_1w(connfd);
	} else if (mode == 3) {
		hash_testcase(connfd);
	} else if (mode == 4) {
		skiptable_testcase_1w(connfd);
	} else if (mode == 5) {
		skiptable_testcase(connfd);
	} else if (mode == 6) {
		hash_testcase(connfd);
	} else if (mode == 7) {
		test_wal_1(connfd);
	} else if (mode == 8) {
		test_wal_2(connfd);
	} else if (mode == 9) {
		test_wal_3(connfd);
	}

	return 0;
	
}

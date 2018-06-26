#include "echo.h"

MODULE_DESCRIPTION("echo server");
MODULE_AUTHOR("ed");
MODULE_LICENSE("GLPv2");

//set module param
static ushort port = PORT;
static ushort backlog = BACKLOG;
module_param(port, ushort, S_IRUGO);
module_param(backlog, ushort, S_IRUGO);

struct echo_server_param param;
struct socket *listen_sock;
struct task_struct *echo_server;

static int open_listen(struct socket **);
static void close_listen(struct socket *);
int echo_server_daemon(void *);

static int echo_init_module(void){

	int error;

	printk(KERN_ALERT MODULE_NAME ": module loaded!\n");

	error = open_listen(&listen_sock);
	if(error < 0){
		printk(KERN_ERR MODULE_NAME ": listen socket open error!\n");
	return error;
	}
	
	param.listen_sock = listen_sock;

	echo_server = kthread_run(echo_server_daemon, &param, MODULE_NAME);
	if(IS_ERR(echo_server)){
		printk(KERN_ERR MODULE_NAME ": cannot start server daemon\n");
	close_listen(listen_sock);
	}
	
	return 0;
}

static void echo_clean_module(void){
	
	send_sig(SIGTERM, echo_server, 1);
	kthread_stop(echo_server);
	close_listen(listen_sock);
	printk(MODULE_NAME ": module unloaded!\n");
}

static int open_listen(struct socket **result){
	struct socket *sock;
	struct sockaddr_in addr;
	int error;
	int opt = 1;

	//IPv4, TCP/IP
	error = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &sock);
	if(error < 0){
		printk(KERN_ERR MODULE_NAME ": socket create error = %d\n", error);
		return error;
	}

	//set tcp_nodelay
	error = kernel_setsockopt(sock, SOL_TCP, TCP_NODELAY, (char *)&opt, sizeof(opt));

	if(error < 0){
		printk(KERN_ERR MODULE_NAME ": setsockopt tcp_nodelay setting error = %d\n", error);
		sock_release(sock);
		return error;
	}
	printk(MODULE_NAME ": setsockopt ok\n");

	//setting sockaddr_in
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(PORT);

	//bind
	error = kernel_bind(sock, (struct sockaddr *)&addr, sizeof(addr));
	if(error < 0){
		printk(KERN_ERR MODULE_NAME ": socket bind error = %d\n", error);
		sock_release(sock);
		return error;
	}
	printk(MODULE_NAME ": socket bind ok\n");

	//listen
	error = kernel_listen(sock, BACKLOG);
	if(error < 0){
		printk(KERN_ERR MODULE_NAME ": socket listen error = %d\n", error);
		sock_release(sock);
		return error;
	}
	printk(MODULE_NAME ": socket listen ok\n");

	*result = sock;
	return 0;
}

static void close_listen(struct socket *sock){
	kernel_sock_shutdown(sock, SHUT_RDWR);
	sock_release(sock);
}

static int get_request(struct socket *sock, unsigned char *buf, size_t size){

	mm_segment_t oldfs;
	struct msghdr msg;
	struct kvec vec;
	int length;
	
	//kvec setting
	vec.iov_len = size;
	vec.iov_base = buf;

	//msghdr setting
	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	printk(MODULE_NAME ": start get response\n");
	length = kernel_recvmsg(sock, &msg, &vec, size, size, msg.msg_flags);
	set_fs(oldfs);
	
	printk(MODULE_NAME ": get request = %s\n", buf);
	
	return length;
}

static int send_request(struct socket *sock, unsigned char *buf, size_t size){

	mm_segment_t oldfs;
	int length;
	struct kvec vec;
	struct msghdr msg;
	
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	vec.iov_base = buf;
	vec.iov_len = strlen(buf);

	printk(MODULE_NAME ": start send request.\n");
	
	length = kernel_sendmsg(sock, &msg, &vec, 1, strlen(buf)-1);

	printk(MODULE_NAME ": send request = %s\n", buf);
	set_fs(oldfs);

	return length;
}

static int echo_server_worker(void *arg){
	
	struct socket *sock;
	unsigned char *buf;
 	int res;

	sock = (struct socket *)arg;
	allow_signal(SIGKILL);
	allow_signal(SIGTERM);
	
	buf = kmalloc(BUF_SIZE, GFP_KERNEL);
	if(!buf){
		printk(KERN_ERR MODULE_NAME ": kmalloc error\n");
		return -1;
	}

	while(!kthread_should_stop()){
		res = get_request(sock, buf, BUF_SIZE - 1);
		printk(MODULE_NAME ": res = %d\n", res);
		if(res <= 0){
			if(res){
				printk(KERN_ERR MODULE_NAME ": get request error = %d\n", res);
			}
			break;
		}
	
		res = send_request(sock, buf, strlen(buf));
		if(res < 0){
			printk(KERN_ERR MODULE_NAME ": send request error = %d\n", res);
		}

		if(strncmp(buf, "EXIT", 4) == 0){
			printk(MODULE_NAME ": finish\n");
			break;
		}
	}
	printk(MODULE_NAME ": connection close\n");
	kernel_sock_shutdown(sock, SHUT_RDWR);
	sock_release(sock);
	kfree(buf);

	return 0;
}

int echo_server_daemon(void *arg){

	struct echo_server_param *param;   //server
	struct socket *sock;   //client socket
	struct task_struct *thread;
	int error;

	//init
	param = (struct echo_server_param *)arg;
	allow_signal(SIGKILL);
	allow_signal(SIGTERM);

	while(!kthread_should_stop()){
		//blocking io
		error = kernel_accept(param->listen_sock, &sock, 0);
		if(error < 0){
			if(signal_pending(current))
				break;
			printk(KERN_ERR MODULE_NAME ": socket accept error = %d\n", error);
			continue;
		}
	//start server worker
		thread = kthread_run(echo_server_worker, sock, MODULE_NAME);
		if(IS_ERR(thread)){
			printk(KERN_ERR MODULE_NAME ": create worker thread error = %d\n", error);
			continue;
		}
	}
	return 0;
}

module_init(echo_init_module);
module_exit(echo_clean_module);





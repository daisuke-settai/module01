#ifndef ECHO_SERVER_H
#define ECHO_SERVER_H
#define MODULE_NAME "echo server"

#include <linux/module.h>
#include <linux/kernel.h>
// kernel thread
#include <linux/kthread.h>
#include <linux/sched.h>  //struct task
//TCP
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/in.h>
#include <linux/tcp.h>
#include <net/sock.h>

#include <linux/types.h>
#include <linux/string.h>


#define BUF_SIZE 4096
#define PORT 3003
#define BACKLOG 128

struct echo_server_param{
  struct socket *listen_sock;
};

#endif

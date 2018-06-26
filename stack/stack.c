#include <linux/module.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/string.h>

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Ed <ed@inl.ics.keio.ac.jp>");
MODULE_DESCRIPTION("stack with list");

#define STACK_SIZE	10
#define STR_MAX		10

struct mystack_entry {
	struct list_head list;
	char *str;
};

static int size = 0;

static LIST_HEAD(mystack);

static void mystack_push(char *s) {
	struct mystack_entry *e = kmalloc(sizeof(*e), GFP_KERNEL);
      	e->str = s;
       	list_add(&e->list, &mystack);
	size++;
}

static int mystack_pop(char *s) {
        struct mystack_entry *e;

        if (list_empty(&mystack))
                return -1;

        e = list_first_entry(&mystack, struct mystack_entry, list);
        if (s != NULL)
		strncpy(s, e->str, STR_MAX);
	kfree(e->str);
        list_del(&e->list);
        kfree(e);
	size--;

        return 0;
}

static void mystack_clean_out(void) {
        while (!list_empty(&mystack)) {
                mystack_pop(NULL);
        }
	size = 0;
}

static struct dentry *mylist_dir;

static struct dentry *showfile;
static struct dentry *pushfile;
static struct dentry *popfile;

static char testbuf[1024];

static ssize_t show_read(struct file *f, char __user *buf, size_t len, loff_t *ppos)
{
        char *bufp = testbuf;
        size_t remain = sizeof(testbuf);
        struct mystack_entry *e;
        size_t l;

        if (list_empty(&mystack))
                return simple_read_from_buffer(buf, len, ppos, "\n", 1);

        list_for_each_entry(e, &mystack, list) {
                int n;

                n = snprintf(bufp, remain, "%s ", e->str);
                if (n == 0)
                        break;
                bufp += n;
                remain -= n;
        }

        l = strlen(testbuf);
        testbuf[l - 1] = '\n';
        return simple_read_from_buffer(buf, len, ppos, testbuf, l);
}

static ssize_t push_write(struct file *f, const char __user *buf, size_t len, loff_t *ppos)
{
        ssize_t ret;
        char *s;

        ret = simple_write_to_buffer(testbuf, sizeof(testbuf), ppos, buf, len);
        if (ret < 0)
                return ret;
	if(size < STACK_SIZE && strlen(testbuf) <= STR_MAX ) {
		s = kmalloc(strlen(testbuf) , GFP_KERNEL);
	        sscanf(testbuf, "%s", s);
        	mystack_push(s);
	}

        return ret;
}

static ssize_t pop_read(struct file *f, char __user *buf, size_t len, loff_t *ppos)
{
        char s[STR_MAX];

        if (*ppos || mystack_pop(s) == -1)
                return 0;
        snprintf(testbuf, sizeof(testbuf), "%s\n", s);
        return simple_read_from_buffer(buf, len, ppos, testbuf, strlen(testbuf));
}

static struct file_operations show_fops = {
        .owner = THIS_MODULE,
        .read = show_read,
};

static struct file_operations push_fops = {
        .owner = THIS_MODULE,
        .write = push_write,
};

static struct file_operations pop_fops = {
        .owner = THIS_MODULE,
        .read = pop_read,
};

static int mymodule_init(void)
{
        mylist_dir = debugfs_create_dir("mystack", NULL);
        if (!mylist_dir)
                return -ENOMEM;
        showfile = debugfs_create_file("show", 0400, mylist_dir, NULL, &show_fops);
        if (!showfile)
                goto fail;
        pushfile = debugfs_create_file("push", 0200, mylist_dir, NULL, &push_fops);
        if (!pushfile)
                goto fail;
        popfile = debugfs_create_file("pop", 0400, mylist_dir, NULL, &pop_fops);
        if (!popfile)
                goto fail;

        return 0;

fail:
        debugfs_remove_recursive(mylist_dir);
        return -ENOMEM;
}

static void mymodule_exit(void)
{
        debugfs_remove_recursive(mylist_dir);
        mystack_clean_out();
}

module_init(mymodule_init);
module_exit(mymodule_exit);


#include <linux/module.h>
#include <linux/init.h>

static int __init hello_world_init(void){
    pr_info("Hello world initialization\n");
    return 0;
}

static void __exit hello_world_exit(void){
    pr_info("Hello world exit");
}

module_init(hello_world_init);
module_exit(hello_world_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gajendra");
MODULE_DESCRIPTION("Hello world linux module");
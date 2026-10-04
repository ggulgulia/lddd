#include <linux/module.h>
#include <linux/moduleparam.h> //needed for module params
#include <linux/init.h>
#include <linux/kernel.h>

static char *mystr = "hello";
static int myint = 1;
static unsigned int myunsigned = 2U;
static int myarr[3] = {0, 1, 2};

module_param(myint, int, S_IRUGO);
module_param(mystr, charp, 0644);
module_param(myunsigned, uint, S_IRUGO);
module_param_array(myarr, int,NULL, S_IWUSR|S_IRUSR);

MODULE_PARM_DESC(myint, "this is an int variable");
MODULE_PARM_DESC(mystr, "this is a char pointer variable");
MODULE_PARM_DESC(myunsigned, "this is a uint");
MODULE_PARM_DESC(myarr, "this is an array of int");
MODULE_INFO(my_field_name, "some random string");

static int __init module_param_example_init(void){
    pr_info("initialization of module param example\n");
    pr_info("int param variable is : %d\n", myint);
    pr_info("string param variable is : %s\n", mystr);
    pr_info("uint param variable is : %u\n", myunsigned);
    pr_info("array param entries are: %d, %d, %d\n", myarr[0], myarr[1], myarr[2]);
    return 0;
}

static void __exit module_param_example_exit(void){
    pr_info("exiting module parameter example\n");
}

module_init(module_param_example_init);
module_exit(module_param_example_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AWESOME_USER");
MODULE_DESCRIPTION("linux kernel module with params");

/*
* Run the module using
* ```sh
* sudo insmod module_param_example.ko myint=42 mystr="Gajendra" myunsigned=555 myarr=9,10,11
* sudo dmesg | tail -n 10
* sudo rmmod module_param_example
* sudo dmesg | tail -n 10
* ```
*/
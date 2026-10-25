# lddd
* A private repository for my personal learning journey of linux device driver development (and hence the name lddd)
* To build all the source code shipped with this repository, run `make`
* To clean up the build artifacts (as a result of `make`), run `make clean`

# Summarized notes organized in different chapters
* Chapter 1 : [Introduction](ch1/README.md)
* Chapter 2 : [Linux kernel module basic concepts](ch2/README.md)
* Chapter 3 : [Linux kernel Core Helpers](ch3/0-Index.md)

# Testing instructions
* Chapter 2 :
    - [Hello world](ch2/src/1-hello-module/TESTING_INSTRUCTIONS.md)
    - [Module param](ch2/src/2-module-param/TESTING_INSTRUCTIONS.md)
    - [Printk](ch2/src/5-printk/TESTING_INSTRUCTIONS.md)
* Chapter 3 :
    - [Spin lock](ch3/src/1-spin-lock/TESTING_INSTRUCTIONS.md)
    - [Spin lock with IRQ](ch3/src/2-spin-lock-irq/TESTING_INSTRUCTIONS.md)
    - [Spin lock IRQ vs IRQ save](ch3/src/3-spin-lock-irq-vs-irq-save/TESTING_INSTRUCTIONS.md)
    - [Wait queue demo](ch3/src/5-waitq-demo/TESTING_INSTRUCTIONS.md)
    - [Time management in ARM CPU](ch3/src/7-time-management/TESTING_INSTRUCTIONS.md)
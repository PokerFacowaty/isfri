# Why and how
While taking a course called "Advanced Linux: The Linux Kernel" on LinkedIn Learning I wanted to play around with making modules after finishing the sample module for the course. And here is what I came up with in a couple hours. I was honestly surprised I managed to get it working pretty smoothly. I used this [great book](https://sysprog21.github.io/lkmpg/), borrowed setting permissions from the TTY driver [as explained here](https://stackoverflow.com/questions/11846594) and modified it slightly when the kernel complained about NULL pointer dereference using [this resource](https://www.ccsl.carleton.ca/~falaca/comp3000/a4.html).

# Prerequisites
You need to have the Linux kernel development package installed. If you see a "build" directory under /lib/modules/\<YOUR KERNEL VERSION>/ then you're good to go. If not, consult your distro-specific instruction on how to obtain the package.

# Compilation
Clone the repo, enter it and use
```bash
make -C /lib/modules/$(uname -r)/build M=$PWD modules
```

# Usage
After succesful compilation, add the module using
```bash
sudo insmod isfri.ko
```

After the module has been added, you can use it by simply using `cat` on the new device
```bash
cat /dev/isfri
```

The module will then "respond" with a proper message according to the current day of the week:
- Friday: "IT IS!"
- Saturday: "You just missed it!"
- Sunday: "No, but it's still the weekend!"
- Any other day: "Nope."

If you want to remove the module from the kernel, simply use
```bash
sudo rmmod isfri
```
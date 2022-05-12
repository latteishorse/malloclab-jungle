# Malloc-lab (CS:APP)
> ### SW Jungle Week06 (05 ~ 12 May, 2022)

### _Dynamic memory allocator in the heap_
- **Method 1: Implicit list**
	- search all list like brute force algorithm
- **Mehtod 2: Explicit list**
	- use linked list
	- better than Implicit list
- **Method 3: Segregated free list**
	- processes are almost the same as explicit
- **Method 4: Buddy system**
	
---
## TIL (Today I Learned)
### `Thu. 12` : End of the week

- Code Review (Jungle Reviewer)

### `Wed. 11`

- Code Review (Team)
- Study Segregated free list & Buddy system

### `Tue. 10`

- make explicit allocator 
	- test score `82/100`
<!-- 	
<center><img src="https://s3.us-west-2.amazonaws.com/secure.notion-static.com/1b3c2eb4-25ef-4ace-942d-8716a0f1a2f2/Untitled.png?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Content-Sha256=UNSIGNED-PAYLOAD&X-Amz-Credential=AKIAT73L2G45EIPT3X45%2F20220511%2Fus-west-2%2Fs3%2Faws4_request&X-Amz-Date=20220511T105929Z&X-Amz-Expires=86400&X-Amz-Signature=323f89bd672da7c53c4d605d129fb99b5de6663c44b843965fe61a5115077375&X-Amz-SignedHeaders=host&response-content-disposition=filename%20%3D%22Untitled.png%22&x-id=GetObject" width="400" height="70"></center> -->

- study segregated list allocator

### `Mon. 09`

- debugging explicit code all day
	- segmentation fault

### `Sun. 08`
- implicit score `54/100` (+1 score than yesterday)

<!-- <center><img src="https://s3.us-west-2.amazonaws.com/secure.notion-static.com/c989d13d-66b9-489f-b8cd-7eac836a4cd6/Untitled.png?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Content-Sha256=UNSIGNED-PAYLOAD&X-Amz-Credential=AKIAT73L2G45EIPT3X45%2F20220510%2Fus-west-2%2Fs3%2Faws4_request&X-Amz-Date=20220510T134219Z&X-Amz-Expires=86400&X-Amz-Signature=6479cb5abec138a0cfa9e817cb0e69d814ac3764fcbc47c42bc60cdfd927a876&X-Amz-SignedHeaders=host&response-content-disposition=filename%20%3D%22Untitled.png%22&x-id=GetObject" width="400" height="70"></center> -->


- study explicit list allocator

### `Sat. 07`

- make ‘malloc’ dynamic memory allocator
	- use Method 1: Implicit list
- implicit list test score `53/100`

### `Fri. 06 `

- study dynamic memory allocator
	- Heap Allocation
	- Allocator Basics
	- Internal / External Fragmentation

### `Thu. 05 May` - Week06 Start

- clone malloc-lab to my [malloc-lab repo.](https://github.com/latteishorse/malloclab-jungle)
- setting test env.
    - aws ec2 (ubuntu 20.04)

---
<details>
<summary> Reference List </summary>
<div markdown="1">


</div>
</details>


<details>
<summary>Original Malloc Lab README</summary>
<div markdown="1">

```C

# Malloc Lab

#####################################################################
# CS:APP Malloc Lab
# Handout files for students
#
# Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.
# May not be used, modified, or copied without permission.
#
######################################################################

***********
Main Files:
***********

mm.{c,h}	
	Your solution malloc package. mm.c is the file that you
	will be handing in, and is the only file you should modify.

mdriver.c	
	The malloc driver that tests your mm.c file

short{1,2}-bal.rep
	Two tiny tracefiles to help you get started. 

Makefile	
	Builds the driver

**********************************
Other support files for the driver
**********************************

config.h	Configures the malloc lab driver
fsecs.{c,h}	Wrapper function for the different timer packages
clock.{c,h}	Routines for accessing the Pentium and Alpha cycle counters
fcyc.{c,h}	Timer functions based on cycle counters
ftimer.{c,h}	Timer functions based on interval timers and gettimeofday()
memlib.{c,h}	Models the heap and sbrk function

*******************************
Building and running the driver
*******************************
To build the driver, type "make" to the shell.

To run the driver on a tiny test trace:

	unix> mdriver -V -f short1-bal.rep

The -V option prints out helpful tracing and summary information.

To get a list of the driver flags:

	unix> mdriver -h

```
</div>
</details>

---
*This page was most recently updated on May 11th, 2022*

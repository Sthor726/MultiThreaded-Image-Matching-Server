# P3
CSCI 4061 - Fall 2024 - Project #3


Project group number
    Group 5
Group member names and x500s
    Luke Lopata Lopat018, Dan Hawker Hawke069, Samual Thorson thors648
The name of the CSELabs computer that you tested your code on
    We were running the code on our WSL. It compiles and runs fine on our end but has some unexptected behavior. The image match function always returns the same image. Other than that everything seems to work fine on our local enviromment. The only lab machine we can SSH into is login03.cselabs.umn.edu. When we run it in here the server can not find the port numbers. The server connection function is black bocked so we cannot debug it. 

Any changes you made to the Makefile or existing files that would affect grading
    Nothing
Plan outlining individual contributions for each member of your group
    Samuel Thorson all of client.py
    Dan Hawker server.py dispatch() worker() and main function, helped debug
    Luke Lopata server.py LogPrettyPrint(), readme, helped debugg
Plan on how you are going to construct the worker threads and how you will make use of mutex locks and condition variables.
    Already implemented but not fully working. 
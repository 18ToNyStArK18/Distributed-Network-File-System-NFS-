The Communication:
    The communication is different 3 different types User Client and Name server and storage server
    We should decide the communication protocal(TCP) and then create all the stucts and macros required.
    Multiple Clients run concurrently so we need to use the threads and make sure that no deadlocks exists.

Name server:
    should store about the clients and then enabel the communication b/w the clients and the storage servers
    should store about the files and the data some hashmapping to enable the communication b/w the user and the data

Storage server:
    This has all the data it should contain a buffer also every time we change we need to store the data before changing because of the operation undo
    Some locking logic aswell so that only a single person and change a file at time and the other operations should wait i think

Client:
    This part is basically take input and parse it and find what the command needs and then execute the command and communicate with the name server


We should define the flags for each like create , delete or modify and all and based on the user input send that flag using TCP


The name server should run concurrently so we use the multithreading for this part 

for the client we create a busy loop with while(1) which is waiting for the user input we can maket this optimal by using the select fucntion ig
the flow should be first the the client will take the input lets say crete this will send this packet to the name server the name server will create the thread for this user and then run this independently
the name server will recive the packet and the name server will create a thread and based on the flag we get we call different worker functions for each input
we need to check some conditions before calling these worker fucntions like create a file which already exists and delete the file which is not there some things like these
    1) Create an existent file
    2) Delete a non-existent file
    3) Writing / Reading a non-existent file
    ...
if there are no errors we will send a packet
once we send a packet we need to wait for the ack and then send the cmd success packet to the client
if there are errors we will send a specific packet for each error to the client so that the client will no what is the error
in the storage server once it recives the packet based on the flags change the files or create or delete and send an ack back if it is a successfull operation else send error flag 

Read:
    Send a req packet to the Name Server and wait for the response if we get any error we will print them and we will send a packet directly to the SS with the location  and we will get this data from the the SS to the client buffer by buffer 
    the name server will recive the packet send it to the ss and check whether there is any file with that name or not and it will find by sending a packet to the SS and then the SS will send the ack packet and based on the packet send an ack packet to the client
    the ss will get two packets first packet is just to verify the file and then second packet is the actual request for the data for the first packet just send the flag and for the second packet send the actual data 
    multiple clients can read at the same time 

Write:
    this is hard we will do it later


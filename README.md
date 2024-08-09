# chat_room
In this code like all prior codes i have created socket, binded, listen and accepted the clients and handeled each client in a new thread

here i used sockop for knowing if a timeout occured

in this when a client connects it asks for the username until it gives a valid username if there is an inactivity then the client connection is closed 
if a valid user name is entered then all the other users recieves the mssg that this client has connected and this client recieves all the clients active

if he types something it is sent to all other active clients and vice-versa
if he is idle and inactivity passes timeout then he is kicked and all other will know that he left the chat room

two commands
\list -> will print all the active clients to the requested client
\bye -> will disconnect the client who typed this and all other active clients will know that he left.

Assumptions: here i will know if a client connected not able to give a username in the server and all the clients being connected and disconnected

Note: here we can also create our own timer and run it in another thread for each thread and use socket shutdown to disable all the recieves and sends for that socket(wrote code for this also but submitted the other implementation of this)


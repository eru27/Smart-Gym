#include <stdint.h> // uint16_t and family
#include <stdio.h> // printf and family
#include <string.h> // strerror()
#include <unistd.h> // file ops
#include <fcntl.h> // open() flags
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <stdlib.h>

#define DEFAULT_BUFLEN 512
#define BROADCAST_PORT 25567
#define UDP_PORT       25565

int main(int argc, char** argv)
{
    int udpListenSocket, c, read_size;
    struct sockaddr_in udpServer, client;
    char server_message[DEFAULT_BUFLEN];
    char name[DEFAULT_BUFLEN];

    FILE *fptr;

    // Open a file in read mode
    fptr = fopen("node.data", "r");

    // Read the content and store it inside myString
    fgets(name, DEFAULT_BUFLEN, fptr);

    // Close the file
    fclose(fptr);
    
    udpListenSocket = socket(AF_INET , SOCK_DGRAM , 0);

    int broadcastEnable = 1;
    setsockopt(udpListenSocket, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

    udpServer.sin_family = AF_INET;
    udpServer.sin_addr.s_addr = INADDR_ANY;
    udpServer.sin_port = htons(BROADCAST_PORT);
    
    //Create socket
    if (udpListenSocket == -1)
    {
        puts("Could not create socket");
    }
    puts("Socket created");

    //Prepare the sockaddr_in structure
    

    //Bind
    if (bind(udpListenSocket,(struct sockaddr *)&udpServer , sizeof(udpServer)) < 0)
    {
        //print the error message
        printf("Error: UDP socket bind failed.\n");
        return 1;
    }
    printf("UDP socket bind done.\n");

    //Accept and incoming connection
    printf("Waiting for incoming connections...\n");
    c = sizeof(struct sockaddr_in);

    //Receive a message from client
    int buff_size = sizeof(struct sockaddr_in); 
    while ((read_size = recvfrom(udpListenSocket , server_message , DEFAULT_BUFLEN , 0, (struct sockaddr*)&client, &buff_size)) > -1)
    {
        printf("Bytes received: %d\n", read_size);
        server_message[read_size] = '\0';

		client.sin_port = htons(UDP_PORT);
	
	//When device gets "ping" message, it sends its username to the PC
        if (strcmp("ping", server_message) == 0) 
        {
            if (sendto(udpListenSocket, name, strlen(name), 0, (struct sockaddr*) &client, buff_size) == -1)
            {
                puts("Error: Send failed.\n");
                return 1;
            }   
        }
	//We check if the controler has received our details and ends the UDP message listening 
        else if (strcmp("Received details", server_message) == 0)
        {
            printf("Client message: %s\n\n", server_message);
            break;
        }    
        printf("Client message: %s\n\n", server_message);
    } 

    if(read_size == -1)
    {
        printf("Error: Receve failed.\n");
    }
    
    //Closing the UDP socket
    close(udpListenSocket);
    printf("UDP Socket Closed.\n");
    
	return 0;
}
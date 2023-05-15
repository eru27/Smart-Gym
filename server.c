#include <stdio.h>      //printf
#include <string.h>     //strlen
#include <sys/socket.h> //socket
#include <arpa/inet.h>  //inet_addr
#include <fcntl.h>      //for open
#include <unistd.h>     //for close
#include <pthread.h>

#define MAX_DEV_COUNT 10
#define DEFAULT_BUFF_SIZE 256

const unsigned short BROADCAST_PORT = 25567;
const unsigned short UDP_PORT = 25565;

pthread_mutex_t mutex_all = PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t cond_all = PTHREAD_COND_INITIALIZER; 
int terminate_listen = 0;
int devices_count = 0;

enum liveness {ALIVE, DEAD};

struct device
{
    char name[DEFAULT_BUFF_SIZE];
    struct in_addr adress;
    enum liveness status;
};

int ping_devs()
{
    int bcast_sock;
    int addr_len;
    struct sockaddr_in broadcast_addr;
    char *ping = "ping\0";

    int ret;

    // Make socket
    bcast_sock = socket(AF_INET , SOCK_DGRAM , 0);
    if (bcast_sock == -1)
    {
        printf("Error: Could not create UDP socket\n");

        return 1;
    }

    // Set socket broadcast options
    int broadcastEnable = 1;
    ret = setsockopt(bcast_sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
    if (ret == -1)
    {
        printf("Error: Could not set broadcast socket options. Check permitions.\n");

        close(bcast_sock);

        return 2;
    }

    // Set address
    addr_len = sizeof(struct sockaddr_in);
    memset((void*)&broadcast_addr, 0, addr_len); // Init everything to '\0'

    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(BROADCAST_PORT);
    broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;

    // Send ping signal
    ret = sendto(bcast_sock, ping, strlen(ping), 0, (struct sockaddr*) &broadcast_addr, addr_len);
    
    if (ret != strlen(ping))
    {
        printf("Error: Broadcast message not sent.\n");

        close(bcast_sock);

        return 3;
    }

    close(bcast_sock);

    return 0;
}

void print_devs(struct device devices[MAX_DEV_COUNT])
{
    printf("\nDevices available for connection:\n");

    int i;
    for (i = 0; i < devices_count; i++)
    {
        printf("%d. %s [%s]\n", i + 1, devices[i].name, inet_ntoa(devices[i].adress));
        printf("Status: ");
        if (devices[i].status == 0)
        {
            printf("ALIVE\n");
        }
        else if (devices[i].status == 1)
        {
            printf("DEAD\n");
        }
        else
        {
            printf("UNKNOWN\n");
        }
    }

    printf("\n");
}

int ping_listen()
{
    int sock;
    int addr_len;
    struct sockaddr_in address;
    char *message = "wakeup\0";

    int ret;

    sock = socket(AF_INET, SOCK_DGRAM , 0);
    if (sock == -1)
    {
        printf("Error: Could not create UDP socket2.\n");

        return 1;
    }

    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(UDP_PORT);

    int buff_size = sizeof(struct sockaddr_in); 
    if(sendto(sock, message, strlen(message), 0, (struct sockaddr*) &address, buff_size) == -1)
    {
        printf("Error: Could not ping listen. Listening socket might not close.\n");

        close(sock);

        return 1;
    }

    close(sock);

    return 0;
}

static void *listen_devices(void *dev)
{
    int sock;
    int addr_len;
    struct sockaddr_in address, device_adr;
    socklen_t addrlen;

    int i, recv;

    char *return_message = "Received details\0";
    char temp_name[DEFAULT_BUFF_SIZE];

    struct device *devices = (struct device *) dev;

    int ret;

    sock = socket(AF_INET, SOCK_DGRAM , 0);
    if (sock == -1)
    {
        pthread_mutex_lock(&mutex_all);
            printf("Error: Could not create UDP listening socket.\n");
            printf("Terminating thread...\n");
        pthread_mutex_unlock(&mutex_all);

        return;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(UDP_PORT);

    //bind socket
    if (bind(sock, (const struct sockaddr *)&address, sizeof(address)) < 0)
    {
        pthread_mutex_lock(&mutex_all);
            printf("Error: Binding listening socket failed.\n");
            printf("Terminating thread...\n");
        pthread_mutex_unlock(&mutex_all);

        close(sock);

        return;
    }

    for (int i = 0; i < MAX_DEV_COUNT; i++)
    {
        addrlen = sizeof(device_adr);
        if((recv = recvfrom(sock, temp_name, DEFAULT_BUFF_SIZE, 0, (struct sockaddr*) &device_adr, &addrlen)) < 0)
        {
            pthread_mutex_lock(&mutex_all);
            printf("Error: Failed to recieve.\n");
            

            if (terminate_listen == 1)
            {
                pthread_mutex_unlock(&mutex_all);
                break;
            }

            pthread_mutex_unlock(&mutex_all);
            continue;
        }

        pthread_mutex_lock(&mutex_all);

        if (terminate_listen == 1)
        {
            pthread_mutex_unlock(&mutex_all);
            break;
        }

        /*** Save device data
        for (i = 0; i < recv; i++)
        {
            if (temp_name[i] == ',')
            {
                devices[devices_count].name[i] = '\0';
                i++;
                devices[devices_count].status = (int) (temp_name[i] - '0');
                break;
            }
            else
            {
                devices[devices_count].name[i] = temp_name[i];
            }
        }***/

        strcpy(devices[devices_count].name, temp_name);
        devices[devices_count].status = ALIVE;

        devices[devices_count].adress = device_adr.sin_addr;
 
        if(sendto(sock , return_message , strlen(return_message), 0, (struct sockaddr*) &device_adr, addrlen) == -1)
        {
            printf("Error: Failed to send a conformation to %s at [%s].", devices[i].name, inet_ntoa(devices[i].adress));
        }

        devices_count++;

        pthread_mutex_unlock(&mutex_all);
    }
    
    close(sock);
}

int main()
{
    int ret_thr, ret_ping;
    struct device devices[MAX_DEV_COUNT];

    char input[DEFAULT_BUFF_SIZE] = {};
    int i;

    pthread_mutex_lock(&mutex_all);

    // Listen
    pthread_t listen_thr;
    ret_thr = pthread_create(&listen_thr, NULL, &listen_devices, (void*) devices);

    do
    {
        printf("\nSearching for available devices...\n");

        // Ping
        ret_ping = ping_devs();
        if (ret_ping != 0)
        {
            break;
        }

        pthread_mutex_unlock(&mutex_all);

        // Sleep so listen can execute
        sleep(5);

        pthread_mutex_lock(&mutex_all);

        print_devs(devices);

        // Check y/n
        printf("Check again? (Y/n) ");
        scanf("%s", &input);

    } while (input[0] == 'Y' || input[0] == 'y');
    
    // Mutex still locked
    terminate_listen = 1;
    ping_listen(); // Ping thread so it gets out of recvfrom() function and terminates

    pthread_mutex_unlock(&mutex_all);

    pthread_join(listen_thr, NULL);

    return 0;
}
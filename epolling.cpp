#include <unistd.h>
#include <stdio.h>
#include <cstdlib>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <signal.h>

#define MAX_PIPES 1023

// preparing variables that we needs 
int nfds = 0;  // this tells select() how far to scan (highest FD + 1)
char buffer[1024] = {0};
struct epoll_event events[MAX_PIPES];

//##################
/* Signal handling */
volatile sig_atomic_t stop = 0;

void handle_signal(int sig)
{
    stop = 1;
}
//##################

int main(int argc , char* argv[])
{
    //##################
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);   // Ctrl+C
    sigaction(SIGTERM, &sa, NULL);  // kill
    //##################

    if (argc < 2) 
    {
        printf("Usage: %s <array_size>\n", argv[0]);
        return -1;
    }

    int num_pipes = atoi(argv[1]);

    if (num_pipes <= 0)
    {
        printf("size is <= 0 \n");
        return -1;
    }

    if (num_pipes <= 0 || num_pipes > MAX_PIPES)
    {
        printf("num_pipes must be between 1 and %d\n", MAX_PIPES);
        return -1;
    }

    //allocating memory for pipes
    int (*m_pipes)[2] = (int(*)[2])malloc(num_pipes * sizeof(*m_pipes));
    if (!m_pipes)
    {
        perror("malloc");
        return -1;
    }

    for (int i = 0; i < num_pipes ; i++)
    {
        if (pipe(m_pipes[i]) == -1)
        {
            perror("pipe");
            return -1;
        }
    }

    // Priting echo's to a file///////////////////////////////////////////////
    FILE *f = fopen("epolling.sh", "w");
    if (!f)
    {
        perror("fopen");
        exit(1);
    }

    for (int i = 0; i < num_pipes; i++)
    {
        fprintf(f, "echo 'test1' > /proc/%d/fd/%d\n",getpid(), m_pipes[i][1]);
    }

    fclose(f);
    //////////////////////////////////////////////////////////////////////////
    
    // Creating epoll 
    int epoll_fd = epoll_create1(0);

    //Register pipe n
    for (int i =0; i < num_pipes ; i++)
    {
        struct epoll_event ev1;
        ev1.events = EPOLLIN; // inputs 
        ev1.data.fd = m_pipes[i][0];
        ev1.data.u32 = i;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, m_pipes[i][0], &ev1);
    }

    while(!stop)
    {
        printf("Waiting for activity...\n");
        nfds = epoll_wait(epoll_fd, events, num_pipes, -1);
        
        if (nfds < 0) 
        { 
            if (errno == EINTR)
                continue;   // interrupted by signal, loop will exit because stop=1
            perror("epoll_wait");
            exit(1); 
        }

        //printf("n = %d \n", nfds);
        for (int i = 0; i < nfds; i++) 
        {
            int index = events[i].data.u32;
            read(m_pipes[index][0], buffer, sizeof(buffer));
            printf("fd %d reading data : %s \n", index, buffer);
            
        }
        
    }

    printf("Cleaning up...\n");
    //closing pipes
    for (int i = 0; i < num_pipes ; i++)
    {
        close(m_pipes[i][0]);
    }

    close(epoll_fd);
    free(m_pipes);
    printf("Exited cleanly.\n");
    
    return 0;
}
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <cstdlib>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define MAX_PIPES 1023

// preparing variables that we needs 
fd_set readfds; // this var will carry the masks for read

int nfds = 0;  // this tells select() how far to scan (highest FD + 1)
char buffer[1024] = {0};

//##################
/* Signal handling */
volatile sig_atomic_t stop = 0;

void handle_signal(int sig)
{
    stop = 1;
}
//##################

int main(int argc, char *argv[])
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
    FILE *f = fopen("selecting.sh", "w");
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

    for (int i = 0; i < num_pipes ; i++)
    {
        if ((m_pipes[i][0]) > nfds)
        {
            nfds = m_pipes[i][0];
        }
    }

    while(!stop)
    {   
        FD_ZERO(&readfds); // here we clear masks
        for (int i = 0; i < num_pipes ; i++)
        {
            FD_SET(m_pipes[i][0], &readfds);
        }
           
        // select() blocks until at least one FD is ready
        printf("Waiting for activity...\n");
        int ready = select(nfds + 1, &readfds, NULL, NULL, NULL);
        if (ready < 0) 
        {
            if (errno == EINTR)
                continue;   // interrupted by signal, loop will exit because stop=1
            perror("select");
            exit(1);
        }

        // check which fd is ready after the select returns ...
        // the returned bitmask has only bits that represents the ready fd 
        for (int i =0; i < num_pipes; i++)
        {
            if (FD_ISSET(m_pipes[i][0], &readfds))
            {
                read(m_pipes[i][0], buffer, sizeof(buffer));
                printf("fd %d reading data : %s \n", i, buffer);
            }
        }
    }

    printf("Cleaning up...\n");
    //closing pipes
    for (int i = 0; i < num_pipes ; i++)
    {
        close(m_pipes[i][0]);
    }

    free(m_pipes);
    printf("Exited cleanly.\n");
    return 0;
}
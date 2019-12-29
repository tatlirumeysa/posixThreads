#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "assist.h"

#define	PROBE	0
#define	ACK	    1
#define	REJECT	2
#define DATA    3
#define N       4
#define ROOT    0

pool_t pool;
fifo_t fifos[N];
pthread_t threads[N];
pthread_t threads2[N];
pthread_t threads3[N];
int controller[N] = {0};

typedef struct {
    int sender;
    int receiver;
    int type; // probe, ack, reject
    int data;
} msg_t;

int a[N][N] = {
    {0,1,1,1},
    {1,0,1,0},
    {1,1,0,1},
    {1,0,1,0},
};


// int a[N][N] = {
//     {0,1,0,0,0,0,1},
//     {1,0,1,0,0,1,1},
//     {0,1,0,1,1,1,0},
//     {0,0,1,0,1,0,0},
//     {0,0,1,1,0,1,0},
//     {0,1,1,0,1,0,1},
//     {1,1,0,0,0,1,0}
// };

int childs[N][N] = {0};
int others[N][N] = {0};
int parents[N] = {0};
int totalParents = 0;

// functions
void *T_ST(void *val);
void *Tbcast(void *val);
void *Tccast(void *val);

int main(void) {

    for(int i=0; i<N; i++) {
        init_fifo(&(fifos[i]));
    }
        
    init_pool(&pool,50);

    int ints[N];
    for (int i = 0; i < N; i++)
    {
        ints[i] = i;
        pthread_create(&threads[i], NULL, T_ST, &ints[i]);
    }

    for (int i = 0; i < N; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // // bcast
    // for (int i = 0; i < N; i++)
    // {
    //     ints[i] = i;
    //     pthread_create(&threads2[i], NULL, Tbcast, &ints[i]);
    // }

    // for (int i = 0; i < N; i++)
    // {
    //     pthread_join(threads[i], NULL);
    // }

    // // ccast
    // for (int i = 0; i < N; i++)
    // {
    //     ints[i] = i;
    //     pthread_create(&threads3[i], NULL, Tccast, &ints[i]);
    // }

    // for (int i = 0; i < N; i++)
    // {
    //     pthread_join(threads[i], NULL);
    // }
    
    return 0;
}

void *T_ST(void *val) {

    bufptr bp;
    int parent = -1;
    int me = *((int *) val);

    int acount = 0; // childs
    int rcount = 0; // others
    int neigh_count = 0;

    printf("\n-------------------------------------- me = %d\n", me);

    for (int j = 0; j < N; j++)
    {
        if(a[me][j] == 1) {
            neigh_count++;
        }
    }

    if (me == ROOT)
    {
        bp = get_buf(&pool);
        bp->type    = PROBE;
        bp->sender  = me;
        bp->data    = 99;

        for (int j = 0; j < N; j++)
        {
            if(a[me][j] == 1) { // root
                bp->receiver = j;
                printf("ROOT RECEIVER: %d\n", bp->receiver);
                write_fifo(&fifos[j], bp);
            }
        }
        parent = me;
    }

    while(acount + rcount < neigh_count)
    {
        printf("acount: %d, rcount: %d, neight_count: %d\n\n", acount, rcount, neigh_count);
        
        bp = read_fifo(&fifos[me]);
        int recvr = bp->receiver;
        printf("ME: %d, SENDER: %d, RECEIVER: %d, type: %d\n", me, bp->sender, bp->receiver, bp->type);
        switch (bp->type)
        {
            case PROBE:
                if(parent == -1) { // if the message comes for the first time
                    parent = bp->sender;
                    parents[me] = parent;
                    printf("i'm: %d - my parent is %d\n", me, parent);
                    bp->type = ACK;
                    bp->receiver = parent;
                    bp->sender = me;
                    printf("me: %d, sender: %d, receiver: %d, type: %d\n", me, bp->sender, bp->receiver, bp->type);
                    write_fifo(&(fifos[parent]), bp);
                    for (int j = 0; j < N; j++)
                    {
                        if (j != parent && a[me][j] == 1)
                        {
                            bp = get_buf(&pool);
                            bp->type = PROBE;
                            bp->receiver = j;
                            bp->sender = me;
                            printf("me: %d, sender: %d, receiver: %d, type: %d\n", me, bp->sender, bp->receiver, bp->type);
                            write_fifo(&(fifos[j]), bp);
                        }
                    }
                } else {
                    bp->receiver = bp->sender;
                    bp->sender = me;
                    bp->type = REJECT;
                    printf("me: %d, sender: %d, receiver: %d, type: %d\n", me, bp->sender, bp->receiver, bp->type);
                    write_fifo(&fifos[recvr], bp);
                }
                break;
            
            case ACK:
                childs[me][acount++] = bp->sender;
                put_buf(&pool, bp);
                break;

            case REJECT:
                others[me][rcount++] = bp->sender;
                put_buf(&pool, bp);
                break;
        }
    }

    pthread_exit(NULL);
}

void *Tbcast(void *val) {
    bufptr bp;
    int me = *((int *) val);
    if(me == ROOT) {
        for (int i = 1; i <= childs[me][0]; i++)
        {
            bp = get_buf(&pool);
            bp->sender = me;
            bp->receiver = childs[me][i];
            bp->data = 99;
            write_fifo(&fifos[childs[me][i]], bp);
        }
    } else {
        bp = read_fifo(&fifos[me]);
        printf("%d %d\n", me, bp->data);
        put_buf(&pool, bp);
        for (int i = 1; i <= childs[me][0]; i++)
        {
            bp = get_buf(&pool);
            bp->sender = me;
            bp->receiver = childs[me][i];
            bp->data = 99;
            write_fifo(&fifos[childs[me][i]], bp);
        }
    }
    pthread_exit(NULL);
}

void *Tccast(void *val) {
    bufptr bp;
    int me = *((int *) val);
    if(childs[me][0] == 0) { // leaf
        bp = get_buf(&pool);
        bp->data = 99;
        bp->sender = me;
        bp->receiver = parents[me];
        write_fifo(&fifos[parents[me]], bp);
    } else {
        for (int i = 1; i <= childs[me][0]; i++)
        {
            bp = read_fifo(&fifos[me]);
            bp->data = 99;
            bp->sender = me;
            bp->receiver = parents[me];
            write_fifo(&fifos[parents[me]], bp);
        }
    }
    pthread_exit(NULL);
}

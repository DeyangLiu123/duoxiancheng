#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

#define BUFFER_SIZE 16

/* Circular buffer of integers. */
struct prodcons {
    int buffer[BUFFER_SIZE];      /* the actual data */
    pthread_mutex_t lock;         /* mutex ensuring exclusive access to buffer */
    int readpos, writepos;        /* positions for reading and writing */
    pthread_cond_t notempty;      /* signaled when buffer is not empty */
    pthread_cond_t notfull;       /* signaled when buffer is not full */
};

#define OVER (-1)
struct prodcons buffer;
volatile int running = 1; // Global flag to indicate if the threads should run

/*--------------------------------------------------------*/
/* Initialize a buffer */
void init(struct prodcons * b) {
    pthread_mutex_init(&b->lock, NULL);
    pthread_cond_init(&b->notempty, NULL);
    pthread_cond_init(&b->notfull, NULL);
    b->readpos = 0;
    b->writepos = 0;
}

/*--------------------------------------------------------*/
/* Store an integer in the buffer */
void put(struct prodcons * b, int data) {
    pthread_mutex_lock(&b->lock);

    /* Wait until buffer is not full */
    while ((b->writepos + 1) % BUFFER_SIZE == b->readpos) {
        pthread_cond_wait(&b->notfull, &b->lock);
    }
    /* Write the data and advance write pointer */
    b->buffer[b->writepos] = data;
    b->writepos++;
    if (b->writepos >= BUFFER_SIZE) b->writepos = 0;
    /* Signal that the buffer is now not empty */
    pthread_cond_signal(&b->notempty);

    pthread_mutex_unlock(&b->lock);
}

/*--------------------------------------------------------*/
/* Read and remove an integer from the buffer */
int get(struct prodcons * b) {
    int data;
    pthread_mutex_lock(&b->lock);

    /* Wait until buffer is not empty */
    while (b->writepos == b->readpos) {
        pthread_cond_wait(&b->notempty, &b->lock);
    }
    /* Read the data and advance read pointer */
    data = b->buffer[b->readpos];
    b->readpos++;
    if (b->readpos >= BUFFER_SIZE) b->readpos = 0;
    /* Signal that the buffer is now not full */
    pthread_cond_signal(&b->notfull);

    pthread_mutex_unlock(&b->lock);
    return data;
}

/*--------------------------------------------------------*/
void *producer(void *data) {
    int n;
    for (n = 0; n < 1000; n++) {
        if (!running) break; // Check if the running flag is set
        put(&buffer, n);
    }
    put(&buffer, OVER);
    return NULL;
}

/*--------------------------------------------------------*/
void *consumer(void *data) {
    int d;
    while (1) {
        d = get(&buffer);
        if (d == OVER) break;
        printf("              %d-->get\n", d);
    }
    return NULL;
}

/*--------------------------------------------------------*/
/* Function to configure terminal for non-blocking input */
void configureTerminal() {
    struct termios newt;
    tcgetattr(STDIN_FILENO, &newt);
    newt.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
}

/*--------------------------------------------------------*/
/* Function to check for keyboard input */
int kbhit(void) {
    struct termios oldt, newt;
    int oldf;
    int ch；
    
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if(ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

/*--------------------------------------------------------*/
/* Thread to handle keyboard input */
void *inputHandler(void *data) {
    while (running) {
        if (kbhit()) {
            int ch = getchar();
            if (ch == 27) { // ASCII code for ESC key
                running = 0; // Set running flag to 0 to stop threads
                break;
            }
        }
        usleep(100000); // Sleep for a while to avoid busy waiting
    }
    return NULL;
}

/*--------------------------------------------------------*/
int main(void) {
    pthread_t th_a, th_b, th_input;
    void *retval;

    init(&buffer);
    pthread_create(&th_a, NULL, producer, 0);
    pthread_create(&th_b, NULL, consumer, 0);
    pthread_create(&th_input, NULL, inputHandler, 0); // Create input handler thread

    /* Wait until producer and consumer finish. */
    pthread_join(th_a, &retval);
    pthread_join(th_b, &retval);
    pthread_join(th_input, &retval); // Join input handler thread

    printf("All threads stopped!\n");
    return 0;
}

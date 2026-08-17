/*
 * IPC Prime vs Abundant Number Race
 *
 * Demonstrates multi-directional inter-process communication (IPC) built on
 * fork() and pipe(). The parent process (P1) spawns two child processes:
 *   - P2 scans File1 and counts how many of its N integers are prime.
 *   - P3 scans File2 and counts how many of its N integers are abundant.
 * The parent relays each child's count to the other child over the pipes,
 * and both children independently determine and announce the winner
 * (the process with the higher count).
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

/* Last 3 digits of the author's student ID; defines the random number range. */
#define ID_LAST_THREE_DIGITS 14
#define RANDOM_RANGE_MAX (ID_LAST_THREE_DIGITS + 100)

#define FILE1_NAME "File1"
#define FILE2_NAME "File2"

#define CHILD_P2_ID 2
#define CHILD_P3_ID 3

/* One full-duplex channel between the parent and a single child process. */
typedef struct {
    int to_child[2];  /* parent writes, child reads  */
    int to_parent[2]; /* child writes, parent reads  */
} PipePair;

/* Returns 1 if n is prime, 0 otherwise. */
static int is_prime(int n) {
    if (n <= 1) return 0;
    for (int i = 2; (long)i * i <= n; i++) {
        if (n % i == 0) return 0;
    }
    return 1;
}

/* Returns 1 if n is abundant (the sum of its proper divisors exceeds n). */
static int is_abundant(int n) {
    if (n <= 0) return 0;
    int divisor_sum = 0;
    for (int i = 1; i <= n / 2; i++) {
        if (n % i == 0) divisor_sum += i;
    }
    return divisor_sum > n;
}

/* Writes `count` random integers in [0, RANDOM_RANGE_MAX] to `filename`. */
static int write_random_numbers(const char *filename, int count) {
    FILE *file = fopen(filename, "w");
    if (!file) return 0;
    for (int i = 0; i < count; i++) {
        fprintf(file, "%d\n", rand() % (RANDOM_RANGE_MAX + 1));
    }
    fclose(file);
    return 1;
}

/* Creates File1 and File2, each holding `n` random integers. */
static int generate_input_files(int n) {
    return write_random_numbers(FILE1_NAME, n) &&
           write_random_numbers(FILE2_NAME, n);
}

/* Allocates both pipes of a PipePair. Returns 0 on failure. */
static int open_pipe_pair(PipePair *pair) {
    return pipe(pair->to_child) != -1 && pipe(pair->to_parent) != -1;
}

/* Counts how many of the first `n` integers in `filename` satisfy `predicate`. */
static int count_matching(const char *filename, int n, int (*predicate)(int)) {
    FILE *file = fopen(filename, "r");
    int count = 0, value;
    if (!file) return 0;
    for (int i = 0; i < n; i++) {
        if (fscanf(file, "%d", &value) != 1) break;
        if (predicate(value)) count++;
    }
    fclose(file);
    return count;
}

/* Prints the standard "I am Child process Px: ..." winner announcement. */
static void announce_winner(int self_id, int self_count, int other_id, int other_count) {
    if (self_count > other_count) {
        printf("I am Child process P%d: The winner is child process P%d\n", self_id, self_id);
    } else if (other_count > self_count) {
        printf("I am Child process P%d: The winner is child process P%d\n", self_id, other_id);
    } else {
        printf("I am Child process P%d: It is a tie!\n", self_id);
    }
}

/*
 * Runs a child process: closes the pipe ends it does not own, receives N
 * from the parent, counts matches in its input file, reports the count to
 * the parent, receives the opponent's count back, and announces the winner.
 * Never returns; terminates the child with exit(0).
 */
static void run_child(PipePair *mine, PipePair *other, int self_id, int other_id,
                       const char *filename, int (*predicate)(int)) {
    /* Keep only the ends this child actually uses. */
    close(mine->to_child[1]);
    close(mine->to_parent[0]);
    close(other->to_child[0]);
    close(other->to_child[1]);
    close(other->to_parent[0]);
    close(other->to_parent[1]);

    int n, my_count, opponent_count;
    read(mine->to_child[0], &n, sizeof(int));

    my_count = count_matching(filename, n, predicate);

    write(mine->to_parent[1], &my_count, sizeof(int));
    read(mine->to_child[0], &opponent_count, sizeof(int));

    announce_winner(self_id, my_count, other_id, opponent_count);

    close(mine->to_child[0]);
    close(mine->to_parent[1]);
    exit(0);
}

/*
 * Runs the parent process: sends N to both children, collects their counts,
 * cross-relays the counts so each child can determine the winner, waits for
 * both children to finish, then prints the final summary.
 */
static void run_parent(PipePair *p2_pipes, PipePair *p3_pipes, int n) {
    /* Keep only the ends the parent actually uses. */
    close(p2_pipes->to_child[0]);
    close(p2_pipes->to_parent[1]);
    close(p3_pipes->to_child[0]);
    close(p3_pipes->to_parent[1]);

    write(p2_pipes->to_child[1], &n, sizeof(int));
    write(p3_pipes->to_child[1], &n, sizeof(int));

    int prime_count, abundant_count;
    read(p2_pipes->to_parent[0], &prime_count, sizeof(int));
    read(p3_pipes->to_parent[0], &abundant_count, sizeof(int));

    /* Cross-share each child's result so the other child can pick a winner. */
    write(p2_pipes->to_child[1], &abundant_count, sizeof(int));
    write(p3_pipes->to_child[1], &prime_count, sizeof(int));

    wait(NULL);
    wait(NULL);

    printf("The number of positive integers in each file: %d\n", n);
    printf("The number of prime numbers in File1: %d\n", prime_count);
    printf("The number of abundant numbers in File2: %d\n", abundant_count);

    close(p2_pipes->to_child[1]);
    close(p2_pipes->to_parent[0]);
    close(p3_pipes->to_child[1]);
    close(p3_pipes->to_parent[0]);
}

int main(void) {
    int n;

    printf("Enter the number of integers (N): ");
    /* Flush before fork(): otherwise this buffered prompt is duplicated by
     * every child process when stdout is not line-buffered (e.g. piped). */
    fflush(stdout);

    if (scanf("%d", &n) != 1 || n < 0) {
        fprintf(stderr, "Invalid input: N must be a non-negative integer.\n");
        return 1;
    }

    srand((unsigned int)time(NULL));
    if (!generate_input_files(n)) {
        fprintf(stderr, "Failed to create %s/%s.\n", FILE1_NAME, FILE2_NAME);
        return 1;
    }

    PipePair p2_pipes, p3_pipes;
    if (!open_pipe_pair(&p2_pipes) || !open_pipe_pair(&p3_pipes)) {
        fprintf(stderr, "Failed to create pipes.\n");
        return 1;
    }

    pid_t p2 = fork();
    if (p2 == -1) {
        fprintf(stderr, "Failed to fork child process P2.\n");
        return 1;
    }
    if (p2 == 0) {
        run_child(&p2_pipes, &p3_pipes, CHILD_P2_ID, CHILD_P3_ID, FILE1_NAME, is_prime);
    }

    pid_t p3 = fork();
    if (p3 == -1) {
        fprintf(stderr, "Failed to fork child process P3.\n");
        return 1;
    }
    if (p3 == 0) {
        run_child(&p3_pipes, &p2_pipes, CHILD_P3_ID, CHILD_P2_ID, FILE2_NAME, is_abundant);
    }

    run_parent(&p2_pipes, &p3_pipes, n);

    return 0;
}

/**
 * CDP Asynchronous Workflow Engine
 * Implements the paradigm:
 * 1. Check for new messages (non-blocking)
 * 2. Execute subtasks concurrently
 * 3. Dynamically adjust task tree
 * 4. Loop until all tasks complete
 */

#include <pthread.h>
#include <unistd.h>
#include <sys/select.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef enum {
    TASK_PENDING,
    TASK_RUNNING, 
    TASK_BLOCKED,
    TASK_COMPLETED,
    TASK_FAILED
} TaskStatus;

typedef enum {
    PRIORITY_CRITICAL = 0,
    PRIORITY_HIGH = 1,
    PRIORITY_NORMAL = 2,
    PRIORITY_LOW = 3
} TaskPriority;

typedef struct Task {
    char id[64];
    char description[256];
    TaskStatus status;
    TaskPriority priority;
    
    struct Task **dependencies;
    int dep_count;
    
    struct Task **subtasks;
    int subtask_count;
    
    int (*execute)(struct Task *self, void *context);
    void *context;
    
    int (*on_message)(struct Task *self, const char *message);
    
    struct Task *next;
} Task;

typedef struct {
    Task *message_queue[4];
    pthread_mutex_t queue_lock;
    pthread_cond_t queue_cond;
    
    Task *task_tree;
    pthread_rwlock_t tree_lock;
    
    pthread_t *workers;
    int worker_count;
    volatile int running;
    
    int user_input_fd;
    void (*on_user_input)(const char *input);
    
} WorkflowEngine;

static WorkflowEngine *g_engine = NULL;

/* Initialize workflow engine */
WorkflowEngine* workflow_init(int worker_count) {
    WorkflowEngine *engine = calloc(1, sizeof(WorkflowEngine));
    
    pthread_mutex_init(&engine->queue_lock, NULL);
    pthread_cond_init(&engine->queue_cond, NULL);
    pthread_rwlock_init(&engine->tree_lock, NULL);
    
    engine->worker_count = worker_count;
    engine->workers = calloc(worker_count, sizeof(pthread_t));
    engine->running = 1;
    engine->user_input_fd = STDIN_FILENO;
    
    g_engine = engine;
    return engine;
}

/* Step 1: Check for new messages (non-blocking) */
int workflow_check_messages(WorkflowEngine *engine) {
    fd_set readfds;
    struct timeval tv = {0, 0};  // Non-blocking
    
    FD_ZERO(&readfds);
    FD_SET(engine->user_input_fd, &readfds);
    
    if (select(engine->user_input_fd + 1, &readfds, NULL, NULL, &tv) > 0) {
        char buffer[1024];
        int n = read(engine->user_input_fd, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            if (engine->on_user_input) {
                engine->on_user_input(buffer);
            }
            return 1;  // Got new message
        }
    }
    return 0;  // No new message
}

/* Step 2: Execute subtasks asynchronously */
void* worker_thread(void *arg) {
    WorkflowEngine *engine = (WorkflowEngine*)arg;
    
    while (engine->running) {
        Task *task = NULL;
        
        pthread_mutex_lock(&engine->queue_lock);
        
        // Find highest priority ready task
        for (int pri = 0; pri < 4; pri++) {
            if (engine->message_queue[pri]) {
                task = engine->message_queue[pri];
                engine->message_queue[pri] = task->next;
                break;
            }
        }
        
        if (!task) {
            // No task, wait for signal
            pthread_cond_wait(&engine->queue_cond, &engine->queue_lock);
            pthread_mutex_unlock(&engine->queue_lock);
            continue;
        }
        
        task->status = TASK_RUNNING;
        pthread_mutex_unlock(&engine->queue_lock);
        
        // Execute task
        if (task->execute) {
            int result = task->execute(task, task->context);
            task->status = (result == 0) ? TASK_COMPLETED : TASK_FAILED;
        }
        
        // Schedule subtasks if any
        if (task->status == TASK_COMPLETED && task->subtask_count > 0) {
            pthread_mutex_lock(&engine->queue_lock);
            for (int i = 0; i < task->subtask_count; i++) {
                workflow_schedule_task(engine, task->subtasks[i]);
            }
            pthread_cond_broadcast(&engine->queue_cond);
            pthread_mutex_unlock(&engine->queue_lock);
        }
    }
    
    return NULL;
}

/* Schedule a task based on priority */
void workflow_schedule_task(WorkflowEngine *engine, Task *task) {
    if (!task || task->status != TASK_PENDING) return;
    
    // Check dependencies
    for (int i = 0; i < task->dep_count; i++) {
        if (task->dependencies[i]->status != TASK_COMPLETED) {
            return;  // Dependencies not met
        }
    }
    
    // Add to priority queue
    task->next = engine->message_queue[task->priority];
    engine->message_queue[task->priority] = task;
}

/* Step 3: Dynamically adjust task tree */
void workflow_adjust_tree(WorkflowEngine *engine, const char *message) {
    pthread_rwlock_wrlock(&engine->tree_lock);
    
    Task *current = engine->task_tree;
    while (current) {
        // Let each task handle the message
        if (current->on_message) {
            current->on_message(current, message);
        }
        
        // Check if we need to reprioritize
        if (strstr(message, current->id)) {
            current->priority = PRIORITY_CRITICAL;
        }
        
        current = current->next;
    }
    
    pthread_rwlock_unlock(&engine->tree_lock);
    
    // Wake up workers in case priorities changed
    pthread_cond_broadcast(&engine->queue_cond);
}

/* Step 4: Main workflow loop */
void workflow_run(WorkflowEngine *engine) {
    // Start worker threads
    for (int i = 0; i < engine->worker_count; i++) {
        pthread_create(&engine->workers[i], NULL, worker_thread, engine);
    }
    
    while (engine->running) {
        // Step 1: Check for new messages
        int has_message = workflow_check_messages(engine);
        
        if (has_message) {
            // Step 3: Adjust task tree based on message
            // (This happens via callback)
        }
        
        // Step 2: Tasks are executing in background
        
        // Check if all tasks completed
        pthread_rwlock_rdlock(&engine->tree_lock);
        int all_done = 1;
        Task *current = engine->task_tree;
        while (current) {
            if (current->status != TASK_COMPLETED && 
                current->status != TASK_FAILED) {
                all_done = 0;
                break;
            }
            current = current->next;
        }
        pthread_rwlock_unlock(&engine->tree_lock);
        
        if (all_done) {
            break;  // All tasks completed
        }
        
        // Small sleep to prevent CPU spinning
        usleep(10000);  // 10ms
    }
    
    // Cleanup
    engine->running = 0;
    pthread_cond_broadcast(&engine->queue_cond);
    
    for (int i = 0; i < engine->worker_count; i++) {
        pthread_join(engine->workers[i], NULL);
    }
}

/* Create a new task */
Task* workflow_create_task(const char *id, const char *desc, 
                           int (*execute)(Task*, void*), void *context) {
    Task *task = calloc(1, sizeof(Task));
    strncpy(task->id, id, sizeof(task->id) - 1);
    strncpy(task->description, desc, sizeof(task->description) - 1);
    task->execute = execute;
    task->context = context;
    task->status = TASK_PENDING;
    task->priority = PRIORITY_NORMAL;
    return task;
}

/* Add dependency between tasks */
void workflow_add_dependency(Task *task, Task *dependency) {
    task->dependencies = realloc(task->dependencies, 
                                 (task->dep_count + 1) * sizeof(Task*));
    task->dependencies[task->dep_count++] = dependency;
}

/* Add subtask */
void workflow_add_subtask(Task *parent, Task *subtask) {
    parent->subtasks = realloc(parent->subtasks,
                               (parent->subtask_count + 1) * sizeof(Task*));
    parent->subtasks[parent->subtask_count++] = subtask;
}

/* Example task execution function */
int example_task_execute(Task *self, void *context) {
    printf("[Worker] Executing task: %s\n", self->description);
    
    // Simulate work
    usleep(100000);  // 100ms
    
    // Could interact with CDP here
    // char *result = execute_javascript(context);
    
    return 0;  // Success
}

/* Example message handler */
int example_task_on_message(Task *self, const char *message) {
    if (strstr(message, "urgent")) {
        self->priority = PRIORITY_CRITICAL;
        printf("[Adjust] Task %s elevated to CRITICAL\n", self->id);
    }
    
    if (strstr(message, "cancel") && strstr(message, self->id)) {
        self->status = TASK_FAILED;
        printf("[Adjust] Task %s cancelled\n", self->id);
    }
    
    return 0;
}

/* Example usage */
void example_workflow() {
    WorkflowEngine *engine = workflow_init(4);  // 4 worker threads
    
    // Create task tree
    Task *task1 = workflow_create_task("compile", "Compile CDP modules", 
                                       example_task_execute, NULL);
    Task *task2 = workflow_create_task("test", "Run tests",
                                       example_task_execute, NULL);
    Task *task3 = workflow_create_task("deploy", "Deploy to production",
                                       example_task_execute, NULL);
    
    // Set dependencies
    workflow_add_dependency(task2, task1);  // test depends on compile
    workflow_add_dependency(task3, task2);  // deploy depends on test
    
    // Set message handlers
    task1->on_message = example_task_on_message;
    task2->on_message = example_task_on_message;
    task3->on_message = example_task_on_message;
    
    // Add to task tree
    task1->next = task2;
    task2->next = task3;
    engine->task_tree = task1;
    
    // Schedule initial tasks
    pthread_mutex_lock(&engine->queue_lock);
    workflow_schedule_task(engine, task1);
    pthread_cond_broadcast(&engine->queue_cond);
    pthread_mutex_unlock(&engine->queue_lock);
    
    // Set user input handler
    engine->on_user_input = (void*)workflow_adjust_tree;
    
    // Run workflow
    printf("Starting workflow engine...\n");
    printf("Type 'urgent <task_id>' to prioritize, 'cancel <task_id>' to cancel\n");
    workflow_run(engine);
    
    printf("Workflow completed!\n");
}

#ifdef WORKFLOW_STANDALONE
int main() {
    example_workflow();
    return 0;
}
#endif
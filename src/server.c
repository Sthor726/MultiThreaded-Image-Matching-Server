#include "../include/server.h"

FILE *logfile;  // log filestream
pthread_mutex_t log_mtx = PTHREAD_MUTEX_INITIALIZER; // log lock

database_entry_t database[100]; // database of images
u_int32_t database_size = 0; // number of images in the database

request_t queue[MAX_QUEUE_LEN]; // queue
int queue_head, queue_tail, queue_len, curr_queue_size = 0; // queue indices
pthread_cond_t space_available = PTHREAD_COND_INITIALIZER; // queue condition var
pthread_cond_t entry_available = PTHREAD_COND_INITIALIZER; // queue condition var
pthread_mutex_t queue_mtx = PTHREAD_MUTEX_INITIALIZER; // queue lock

pthread_t dispatcher_thread[MAX_THREADS]; // array to hold id of dispatcher threads
pthread_t worker_thread[MAX_THREADS]; // array to hold id of worker threads
int num_dispatcher = 0; // number of dispatchers
int num_worker = 0; // number of workers

database_entry_t image_match(char *input_image, int size)
{
  const char *closest_file     = NULL;
  int         closest_distance = 10;
  int closest_index = 0;
  for (int i = 0; i < database_size; i++) {
  	const char *current_file = database[i].buffer;
  	int result = memcmp(input_image, current_file, size);
    if(result == 0) {
  		return database[i];
  	}
  	else if (result < closest_distance) {
  		closest_distance = result;
  		closest_file     = current_file;
      closest_index = i;
  	}
  }

  if (closest_file != NULL) {
    return database[closest_index];
  } else {
    printf("No closest file found.\n");
    return database[0];
  }
}

//TODO: Implement this function
/**********************************************
 * LogPrettyPrint
   - parameters:
      - to_write is expected to be an open file pointer, or it 
        can be NULL which means that the output is printed to the terminal
      - All other inputs are self explanatory or specified in the writeup
   - returns:
       - no return value
************************************************/

void LogPrettyPrint(FILE* to_write, int threadId, int requestNumber, char * file_name, int file_size){
  // make string to print
  char log[1200]; // allocate enough space for the name 1028 chars and some extra for ints 
  memset(log, '\0', sizeof(log));
  sprintf(log, "[%d][%d][%s][%d]\n", threadId, requestNumber, file_name, file_size);


  if (pthread_mutex_lock(&log_mtx) != 0) {
    perror("Error acquiring log lock");
    exit(EXIT_FAILURE);
  }
  // flush before to clean buffer
  if (fflush(to_write) == -1){
      perror("Error flushing buffer to file");
      exit(EXIT_FAILURE);
  }


  if(to_write == NULL){
    printf("%s", log);
  } else {
    // acquire the log lock
    if (fprintf(to_write, "%s", log) == -1){
      perror("Error writing to file");
      exit(EXIT_FAILURE);
    }

    if (fflush(to_write) == -1){
      perror("Error flushing buffer to file");
      exit(EXIT_FAILURE);
    }
  }

  // release the log lock
  if (pthread_mutex_unlock(&log_mtx) != 0) {
    perror("Error releasing log lock");
    exit(EXIT_FAILURE);
  }
  return;
}

void loadDatabase(char *path) {
  struct dirent *entry; 
  DIR *dir = opendir(path);
  if (dir == NULL)
  {
    perror("Opendir ERROR");
    exit(0);
  }
  while ((entry = readdir(dir)) != NULL)
  {
    if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 && strcmp(entry->d_name, ".DS_Store") != 0)
    {
      sprintf(database[database_size].file_name, "%s/%s", path, entry->d_name);
      FILE *fp1;
      unsigned char *buffer1 = NULL;
      long fileLength1;

      // Open the image files
      fp1 = fopen(database[database_size].file_name, "rb");
      if (fp1 == NULL) {
          perror("Error: Unable to open image files.\n");
          exit(1);
      }

      // Get the length of file 1
      fseek(fp1, 0, SEEK_END);
      fileLength1 = ftell(fp1);
      rewind(fp1);

      // Allocate memory to store file contents
      buffer1 = (unsigned char*)malloc(fileLength1 * sizeof(unsigned char));
    
      if (buffer1 == NULL) 
      {
        printf("Error: Memory allocation failed.\n");
        fclose(fp1);
        if (buffer1 != NULL) free(buffer1);
      }

      // Read file contents into memory buffers
      fread(buffer1, sizeof(unsigned char), fileLength1, fp1);
      database[database_size].buffer = (char*) buffer1;
      database[database_size].file_size = fileLength1;
      database_size++;
    }
  }

  closedir(dir);
}

void* dispatch(void *thread_id) {   
  while (true) {
    request_t request;

    // accept client connection
    request.file_descriptor = accept_connection();
    if (request.file_descriptor < 0) {
      continue;
    }

    // get client request
    request.buffer = get_request_server(request.file_descriptor, &request.file_size);
    if (request.buffer == NULL) {
      continue;
    }

    // acquire the queue lock
    if (pthread_mutex_lock(&queue_mtx) != 0) {
      perror("Error acquiring lock");
      exit(EXIT_FAILURE);
    }

    // wait for space in the queue
    while(curr_queue_size == queue_len) {
      if (pthread_cond_wait(&space_available, &queue_mtx) != 0) {
        perror("Error waiting for condition var");
        exit(EXIT_FAILURE);
      }
    }

    // add the request into the queue and update indices
    queue[queue_tail] = request;
    queue_tail = (queue_tail + 1) % queue_len;
    curr_queue_size++;

    // release the queue lock
    if (pthread_mutex_unlock(&queue_mtx) != 0) {
      perror("Error releasing lock");
      exit(EXIT_FAILURE);
    }

    // signal that the queue is not empty
    if (pthread_cond_broadcast(&entry_available) != 0) {
      perror("Error signaling condition var");
      exit(EXIT_FAILURE);
    }

  }
    return NULL;
}

void * worker(void *thread_id) {
  int num_request = 0;                                    //Integer for tracking each request for printing into the log file
  //int fileSize    = 0;                                    //Integer to hold the size of the file being requested
  //void *memory    = NULL;                                 //memory pointer where contents being requested are read and stored
  //int fd          = INVALID;                              //Integer to hold the file descriptor of incoming request
  //char *mybuf;                                            //String to hold the contents of the file being requested
    
  while (true) {
    // acquire the queue lock
    if (pthread_mutex_lock(&queue_mtx) != 0) {
      perror("Error acquiring lock");
      exit(EXIT_FAILURE);
    }

    // wait for an item in the queue
    while(curr_queue_size == 0) {
      if (pthread_cond_wait(&entry_available, &queue_mtx) != 0) {
        perror("Error waiting for condition var");
        exit(EXIT_FAILURE);
      }
    }

    // extract the request
    request_t request = queue[queue_head];
    queue_head = (queue_head + 1) % queue_len;
    curr_queue_size--;

    // release the queue lock
    if (pthread_mutex_unlock(&queue_mtx) != 0) {
      perror("Error releasing lock");
      exit(EXIT_FAILURE);
    }

    // signal that the queue has space available
    if (pthread_cond_signal(&space_available) != 0) {
      perror("Error signaling condition var");
      exit(EXIT_FAILURE);
    }
        
    // call image_match()
    database_entry_t result = image_match(request.buffer, request.file_size);

    // send file to the client
    send_file_to_client(request.file_descriptor, result.buffer, result.file_size);

    // log
    LogPrettyPrint(logfile, *((int*)thread_id), ++num_request, result.file_name, result.file_size);

  }
}

int main(int argc , char *argv[]) {
  if(argc != 6){
    printf("usage: %s port path num_dispatcher num_workers queue_length \n", argv[0]);
    return -1;
  }

  // retrive input arguments
  int port = atoi(argv[1]);
  if ((port > MAX_PORT) || (port < MIN_PORT)) {
    printf("Port must be between %i and %i\n", MIN_PORT, MAX_PORT);
    return -1;
  }

  char path[BUFF_SIZE];
  strncpy(path, argv[2], BUFF_SIZE);

  num_dispatcher = atoi(argv[3]);
  num_worker = atoi(argv[4]);
  if ((num_worker > MAX_THREADS) || (num_dispatcher > MAX_THREADS)) {
    printf("Maximum of %i dispatchers/ workers\n", MAX_QUEUE_LEN);
    return -1;
  }

  queue_len = atoi(argv[5]);
  if (queue_len > MAX_QUEUE_LEN) {
    printf("Maximum of %i queue length\n", MAX_QUEUE_LEN);
    return -1;
  }
  

  // open log file
  logfile = fopen("server_log", "w");
  if (logfile == NULL) {
      perror("Failed to open log file");
      exit(EXIT_FAILURE);
  }

  // start the server
  init(port);

  // load database into memory
  loadDatabase(path);

  // create threads
  for (int i = 0; i < num_worker; i++) {
      int *thread_id = malloc(sizeof(int));
      if (thread_id == NULL) {
          perror("Failed to allocate memory for thread ID");
          exit(EXIT_FAILURE);
      }
      *thread_id = i;
      if (pthread_create(&worker_thread[i], NULL, worker, (void *)thread_id) != 0) {
          printf("ERROR: Failed to create worker thread %d.\n", i);
          exit(EXIT_FAILURE);
      }
  }
  for (int i = 0; i < num_dispatcher; i++) {
      int *thread_id = malloc(sizeof(int));
      if (thread_id == NULL) {
          perror("Failed to allocate memory for thread ID");
          exit(EXIT_FAILURE);
      }
      *thread_id = i;
      if (pthread_create(&dispatcher_thread[i], NULL, dispatch, (void *)thread_id) != 0) {
          printf("ERROR: Failed to create worker thread %d.\n", i);
          exit(EXIT_FAILURE);
      }
  }

  // Wait for each of the threads to complete their work
  // Threads (if created) will not exit (see while loop), but this keeps main from exiting
  for(int i = 0; i < num_dispatcher; i++){
    if((pthread_join(dispatcher_thread[i], NULL)) != 0){
      printf("ERROR : Fail to join dispatcher thread %d.\n", i);
    }
  }

  for(int i = 0; i < num_worker; i++){
   // fprintf(stderr, "JOINING WORKER %d \n",i);
    if((pthread_join(worker_thread[i], NULL)) != 0){
      printf("ERROR : Fail to join worker thread %d.\n", i);
    }
  }
  fprintf(stderr, "SERVER DONE \n");  // will never be reached in SOLUTION
  fclose(logfile);//closing the log files

}
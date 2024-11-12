#include "../include/server.h"

int num_dispatcher = 0; //Global integer to indicate the number of dispatcher threads   
int num_worker = 0;  //Global integer to indicate the number of worker threads
FILE *logfile;  //Global file pointer to the log file

database_entry_t database[100]; // database of images
u_int32_t database_size = 0; // number of images in the database

request_t queue[MAX_QUEUE_LEN]; // queue
ssize_t queue_head, queue_tail, queue_len = 0; // queue indices
pthread_cond_t space_available = PTHREAD_COND_INITIALIZER; // queue condition var
pthread_cond_t entry_available = PTHREAD_COND_INITIALIZER; // queue condition var
pthread_mutex_t queue_mtx = PTHREAD_MUTEX_INITIALIZER; // queue lock

int dispatcher_thread[MAX_THREADS]; // array to hold id of dispatcher threads
int worker_thread[MAX_THREADS]; // array to hold id of worker threads

//TODO: Implement this function
/**********************************************
 * image_match
   - parameters:
      - input_image is the image data to compare
      - size is the size of the image data
   - returns:
       - database_entry_t that is the closest match to the input_image
************************************************/
//just uncomment out when you are ready to implement this function
database_entry_t image_match(char *input_image, int size)
{
  // const char *closest_file     = NULL;
	// int         closest_distance = 10;
  // int closest_index = 0;
  // for(int i = 0; i < 0 /* replace with your database size*/; i++)
	// {
	// 	const char *current_file; /* TODO: assign to the buffer from the database struct*/
	// 	int result = memcmp(input_image, current_file, size);
	// 	if(result == 0)
	// 	{
	// 		return database[i];
	// 	}

	// 	else if(result < closest_distance)
	// 	{
	// 		closest_distance = result;
	// 		closest_file     = current_file;
  //     closest_index = i;
	// 	}
	// }

	// if(closest_file != NULL)
	// {
  //   return database[closest_index];
	// }
  // else
  // {
  //   printf("No closest file found.\n");
  // }
  
  
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
  
}

void loadDatabase(char *path) {
  struct dirent* entry;

  // open the directory
  DIR* dir = opendir(path);
  if (dir == NULL) {
    perror("Failed to open database");
    exit(EXIT_FAILURE);
  }

  // iterate through the directory
  while ((entry = readdir(dir)) != NULL) {
    // skip '.' and '..' entries
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
        continue;
    }

    // construct the file path
    char fullPath[BUFF_SIZE];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);

    // open the file
    int fd = open(fullPath, O_RDONLY);
    if (fd == -1) {
      perror("Failed to open database file");
      exit(EXIT_FAILURE);
    }

    // initialize database entry struct
    database_entry_t image;
    image.file_name = entry->d_name;
    image.buffer = malloc(BUFFER_SIZE);
    if (!image.buffer) {
      perror("Failed to allocate memory for image");
      exit(EXIT_FAILURE);
    }

    // read file contents into buffer
    ssize_t bytesRead;
    ssize_t totalSize = 0;

    while ((bytesRead = read(fd, image.buffer + totalSize, BUFFER_SIZE)) > 0) {
      totalSize += bytesRead;
      // may need to reallocate here? are the images under a certain size?
    }
    if (bytesRead == -1) {
      perror("Error reading file");
      exit(EXIT_FAILURE);
    }

    if (close(fd) == -1) {
      perror("Error closing file");
      exit(EXIT_FAILURE);
    }

    // save file size
    image.file_size = totalSize;

    // add the database entry to the in-memory array
    database[database_size++] = image;
  }

  // close the directory
  if (closedir(dir) == -1) {
    perror("Error closing directory");
    exit(EXIT_FAILURE);
  }
}

void * dispatch(void *thread_id) {   
  while (1) {
    request_t request;

    // accept client connection-
    request.file_descriptor = accept_connection();
    if (request.file_descriptor < 0) {
      continue;
    }

    // get client request
    char* req = get_request_server(request.file_descriptor, &request.file_size);
    if (req == NULL) {
      continue;
    }

    // allocate and copy the request string into the struct
    request.buffer = malloc(request.file_size);
    if (request.buffer == NULL) {
      perror("Failed to allocate memory for buffer");
      exit(EXIT_FAILURE);
    }
    strncpy(request.buffer, req, request.file_size);

    // free
    free(req);

    // acquire the queue lock
    if (pthread_mutex_lock(&queue_mtx) != 0) {
      perror("Error acquiring lock");
      exit(EXIT_FAILURE);
    }

    // wait for space in the queue
    while(queue_len == MAX_QUEUE_LEN) {
      if (pthread_cond_wait(&space_available, &queue_mtx) != 0) {
        perror("Error waiting for condition var");
        exit(EXIT_FAILURE);
      }
    }

    // add the request into the queue and update indices
    queue[queue_tail] = request;
    queue_tail = (queue_tail + 1) % MAX_QUEUE_LEN;
    queue_len++;

    // release the queue lock
    if (pthread_mutex_unlock(&queue_mtx) != 0) {
      perror("Error releasing lock");
      exit(EXIT_FAILURE);
    }

    // signal that the queue is not empty
    if (pthread_cond_signal(&entry_available) != 0) {
      perror("Error signaling condition var");
      exit(EXIT_FAILURE);
    }

  }
    return NULL;
}

void * worker(void *thread_id) {

  // You may use them or not, depends on how you implement the function
  int num_request = 0;                                    //Integer for tracking each request for printing into the log file
  int fileSize    = 0;                                    //Integer to hold the size of the file being requested
  void *memory    = NULL;                                 //memory pointer where contents being requested are read and stored
  int fd          = INVALID;                              //Integer to hold the file descriptor of incoming request
  char *mybuf;                                  //String to hold the contents of the file being requested


  /* TODO : Intermediate Submission 
  *    Description:      Get the id as an input argument from arg, set it to ID
  */
    
  while (1) {
    /* TODO
    *    Description:      Get the request from the queue and do as follows
      //(1) Request thread safe access to the request queue by getting the req_queue_mutex lock
      //(2) While the request queue is empty conditionally wait for the request queue lock once the not empty signal is raised

      //(3) Now that you have the lock AND the queue is not empty, read from the request queue

      //(4) Update the request queue remove index in a circular fashion

      //(5) Fire the request queue not full signal to indicate the queue has a slot opened up and release the request queue lock  
      */
        
      
    /* TODO
    *    Description:       Call image_match with the request buffer and file size
    *    store the result into a typeof database_entry_t
    *    send the file to the client using send_file_to_client(int fd, char * buffer, int size)              
    */

    /* TODO
    *    Description:       Call LogPrettyPrint() to print server log
    *    update the # of request (include the current one) this thread has already done, you may want to have a global array to store the number for each thread
    *    parameters passed in: refer to write up
    */
  }
}

int main(int argc , char *argv[]) {
  if(argc != 6){
    printf("usage: %s port path num_dispatcher num_workers queue_length \n", argv[0]);
    return -1;
  }

  // retrive input arguments
  int port = atoi(argv[1]);
  char path[BUFF_SIZE];
  strncpy(path, argv[2], BUFF_SIZE);
  num_dispatcher = atoi(argv[3]);
  num_worker = atoi(argv[4]);
  queue_len = atoi(argv[5]);
  

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
      if (pthread_create(&dispatcher_thread[i], NULL, worker, (void *)thread_id) != 0) {
          printf("ERROR: Failed to create worker thread %d.\n", i);
          exit(EXIT_FAILURE);
      }
  }

  // Wait for each of the threads to complete their work
  // Threads (if created) will not exit (see while loop), but this keeps main from exiting
  for(int i = 0; i < num_dispatcher; i++){
    fprintf(stderr, "JOINING DISPATCHER %d \n",i);
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
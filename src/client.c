#include "../include/client.h"

int port = 0;

pthread_t worker_thread[100];
int worker_thread_id = 0;
char output_path[1028];

processing_args_t req_entries[100];

pthread_mutex_t img_mtx = PTHREAD_MUTEX_INITIALIZER;

int img = 1;

void * request_handle(void * img_file_path) {
    // open the file
    FILE* file = fopen(img_file_path, "rb");
    if (file == NULL) {
        perror("fopen");
        return NULL;
    }

    // get file size
    if (fseek(file, 0, SEEK_END) != 0){
        perror("fseek");
        fclose(file);
        return NULL;
    }
    long len = ftell(file);
    if(len == -1){
        perror("ftell");
        fclose(file);
        return NULL;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        perror("fseek");
        fclose(file);
        return NULL;
    }

    // set up connection
    int fd = setup_connection(port);

    // send file to server
    send_file_to_server(fd, file, len);

    // contruct output destination
    char constructed_path[1600];

    // Construct output destination
    // acquire the img lock
    if (pthread_mutex_lock(&img_mtx) != 0) {
      perror("Error acquiring lock");
      exit(EXIT_FAILURE);
    }

    snprintf(constructed_path, sizeof(constructed_path), "%s/img_%d.png", output_path, img++);

    if (pthread_mutex_unlock(&img_mtx) != 0) {
      perror("Error releasing lock");
      exit(EXIT_FAILURE);
    }

    // receive the file
    if (receive_file_from_server(fd, constructed_path) == -1){
        printf("Error: receive_file_from_server\n");
        fclose(file);
        return NULL;
    }

    // close file
    if (fclose(file) == EOF) {
        perror("fclose");
        return NULL;
    }
    return NULL;
}

/* Directory traversal function is provided to you. */
void directory_trav(char * img_directory_path)
{
    char dir_path[BUFF_SIZE]; 
    strcpy(dir_path, img_directory_path);
    struct dirent *entry; 
    DIR *dir = opendir(dir_path);
    if (dir == NULL)
    {
        perror("Opendir ERROR");
        exit(0);
    }
    while ((entry = readdir(dir)) != NULL)
    {
        if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 && strcmp(entry->d_name, ".DS_Store") != 0)
        {
            sprintf(req_entries[worker_thread_id].file_name, "%s/%s", dir_path, entry->d_name);
            printf("New path: %s\n", req_entries[worker_thread_id].file_name);
            pthread_create(&worker_thread[worker_thread_id], NULL, request_handle, (void *) req_entries[worker_thread_id].file_name);
            worker_thread_id++;
        }
    }
    closedir(dir);
    for(int i = 0; i < worker_thread_id; i++)
    {
        pthread_join(worker_thread[i], NULL);
    }
}


int main(int argc, char *argv[]) {
    // verify argument count
    if(argc != 4) {
        fprintf(stderr, "Usage: %s <directory path> <Server Port> <output path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // parse arguments
    char* dir_path = argv[1];
    port = atoi(argv[2]);
    memset(output_path, '\0', sizeof(output_path));
    strcpy(output_path, argv[3]);

    // traverse the input directory
    directory_trav(dir_path);

    return EXIT_SUCCESS;  
}
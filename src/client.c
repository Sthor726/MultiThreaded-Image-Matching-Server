#include "../include/client.h"



int port = 0;

pthread_t worker_thread[100];
int worker_thread_id = 0;
char output_path[1028];

processing_args_t req_entries[100];

/* TODO: implement the request_handle function to send the image to the server and recieve the processed image
* 1. Open the file in the read-binary mode (i.e. "rb" mode) - Intermediate Submission, DONE
* 2. Get the file length using the fseek and ftell functions - Intermediate Submission, DONE
* 3. set up the connection with the server using the setup_connection(int port) function - Intermediate Submission, DONE
* 4. Send the file to the server using the send_file_to_server(int fd, FILE *file, int size) function - Intermediate Submission, DONE
* 5. Receive the processed image from the server using the receive_file_from_server(int socket, char *file_path) function, DONE?
* 6. receive_file_from_server saves the processed image in the output directory, so pass in the right directory path, DONE?
* 7. Close the file, DONE
*/
void * request_handle(void * img_file_path)
{
    FILE* file = fopen(img_file_path, "rb");
    if (fseek(file, 0, SEEK_END) != 0){
        perror("fseek");
    }
    long len = ftell(file);
    if(len == -1){
        perror("ftell");
    }
    fseek(file, 0, SEEK_SET);

    int fd = setup_connection(port);
    if(send_file_to_server(fd, file, len) == -1){
        perror("send_file_to_server");
    }
    if(receive_file_from_server(fd, "./output/img") == -1){
        perror("receive_file_from_server");
    }

    fclose(file);
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


int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        fprintf(stderr, "Usage: ./client <directory path> <Server Port> <output path>\n");
        exit(-1);
    }
    /*TODO:  Intermediate Submission
    * 1. Get the input args --> (1) directory path (2) Server Port (3) output path, DONE
    */
    char* dir_path = argv[1];
    port = atoi(argv[2]);
    strcpy(output_path, argv[3]);
    /*TODO: Intermediate Submission
    * Call the directory_trav function to traverse the directory and send the images to the server, DONE
    */
    directory_trav(dir_path);
    return 0;  
}
#include <dirent.h> 
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <time.h>
#include <pthread.h>

#define BUFFER_SIZE 1048576 // 1MB
#define NUMBER_OF_THREADS 4

//Struct Definitions for Buffer and Thread Data
typedef struct{
	unsigned char input[BUFFER_SIZE];
	int input_size;
	unsigned char output[BUFFER_SIZE];
	int output_size;
} Buffer;
typedef struct{
	int start;
	int end;	
} ThreadData;

//Function Declerations
int cmp(const void *a, const void *b);
void* processPPM(void* arg);

//Global Variables
char **files;
Buffer** buffers;
ThreadData** thread_data;
int nfiles = 0;
char *direc;

// Main Function
int main(int argc, char **argv) {
	// time computation header
	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);
	// end of time computation header
	// do not modify the main function before this point!

	direc = argv[1];

	DIR *d;
	struct dirent *dir;
	
	d = opendir(argv[1]);
	if(d == NULL) {
		printf("An error has occurred\n");
		return 0;
	}

	// create sorted list of PPM files
	while ((dir = readdir(d)) != NULL) {
		files = realloc(files, (nfiles+1)*sizeof(char *));

		int len = strlen(dir->d_name);
		if(dir->d_name[len-4] == '.' && dir->d_name[len-3] == 'p' && dir->d_name[len-2] == 'p' && dir->d_name[len-1] == 'm') {
			files[nfiles] = strdup(dir->d_name);

			nfiles++;
		}
	}
	closedir(d);
	qsort(files, nfiles, sizeof(char *), cmp);

    //allocate space for structs on heap
    thread_data = malloc(NUMBER_OF_THREADS * sizeof(ThreadData*));
    buffers = malloc( nfiles * sizeof(Buffer*));


    // for each thread calculate the start and end file and save it in thread data struct
	int extra_files = nfiles % NUMBER_OF_THREADS;
	int files_per_thread = nfiles / NUMBER_OF_THREADS;
	for (int i = 0; i < NUMBER_OF_THREADS; i++) {
		thread_data[i] = malloc(sizeof(ThreadData));
		thread_data[i]->start = i;
		thread_data[i]->end = i + ((files_per_thread - 1) * NUMBER_OF_THREADS) + (i < extra_files ? NUMBER_OF_THREADS : 0);
	}


	// create n threads and assign each thread an offset
	pthread_t threads[NUMBER_OF_THREADS];
	for (int i = 0, start = 0; i < NUMBER_OF_THREADS; i++) {
		pthread_create(&threads[i], NULL, processPPM, thread_data[i]);
	}
	// wait for each thread to join back to main thread
	for (int i = 0; i < NUMBER_OF_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}
	// read the buffers and write the output in video.vzip
	int total_in = 0, total_out = 0;
	FILE *f_out = fopen("video.vzip", "w");
	for(int i = 0; i < nfiles; i++) {
		total_in += buffers[i]->input_size;
		total_out += buffers[i]->output_size;
		fwrite(&buffers[i]->output_size, sizeof(int), 1, f_out);
		fwrite(buffers[i]->output, sizeof(unsigned char), buffers[i]->output_size, f_out);
	}
	fclose(f_out);

	printf("Compression rate: %.2lf%%\n", 100.0*(total_in-total_out)/total_in);

	// time computation footer
	clock_gettime(CLOCK_MONOTONIC, &end);
	printf("Time: %.2f seconds\n", ((double)end.tv_sec+1.0e-9*end.tv_nsec)-((double)start.tv_sec+1.0e-9*start.tv_nsec));
	// end of time computation footer

	return 0;
}

// Comparison Function for Qsort
int cmp(const void *a, const void *b) {
	return strcmp(*(char **) a, *(char **) b);
}

// Function to pass in each thread to decompress the images and store it in the Buffer Array
void* processPPM(void* arg) {
	ThreadData* data = (ThreadData*) arg;

    //iterates through the index of files passed in the thread
	for(int i = data->start; i <= data->end; i+=NUMBER_OF_THREADS) {
        
        
        buffers[i] = malloc(sizeof(Buffer));
		int len = strlen(direc) + strlen(files[i]) + 1;

        //derive path for each file
		char *full_path = malloc(len*sizeof(char));
		strcpy(full_path, direc);
		strcat(full_path, "/");
		strcat(full_path, files[i]);
		FILE *f_in = fopen(full_path, "r");

		// read the PPM file
		buffers[i]->input_size = fread(buffers[i]->input, 1, BUFFER_SIZE, f_in);

		// compress the PPM file
		z_stream stream;
		int ret = deflateInit(&stream, 9);
		stream.avail_in = buffers[i]->input_size;
		stream.next_in = buffers[i]->input;
		stream.avail_out = BUFFER_SIZE;
		stream.next_out = buffers[i]->output;

		ret = deflate(&stream, Z_FINISH);

		buffers[i]->output_size = stream.total_out;

	}
}

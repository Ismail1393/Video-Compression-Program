#include <dirent.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <time.h>
#include <pthread.h>

#define BUFFER_SIZE 1048576 // 1MB
#define THREAD_COUNT 1

typedef struct
{
	const char *directory;
	const char *filename;
	FILE *f_out;
	int *total_in;
	int *total_out;
} ProcessAndZipArgs;

void sortedListOfFiles(DIR *d, char ***files, int *nfiles);
char *buildPath(const char *directory, const char *filename);
int loadFile(const char *filepath, unsigned char *buffer);
int zipFile(unsigned char *buffer_in, int input_size, unsigned char *buffer_out, int buffer_out_size);
void *processAndZipFile(void *arg);
int cmp(const void *a, const void *b);

void *func(void *vargp)
{
	long thread_num = (long)vargp;
	sleep(1);
	printf("Printing from Thread %ld\n", thread_num);
	return NULL;
}

int main(int argc, char **argv)
{
	// time computation header
	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);
	// end of time computation header

	// do not modify the main function before this point!

	assert(argc == 2);

	DIR *d = opendir(argv[1]);
	if (d == NULL)
	{
		printf("An error has occurred\n");
		return 0;
	}

	// create sorted list of PPM files
	struct dirent *dir;
	char **files = NULL;
	int nfiles = 0;

	sortedListOfFiles(d, &files, &nfiles);

	pthread_t threads[nfiles];

	// create a single zipped package with all PPM files in lexicographical order
	int total_in = 0, total_out = 0;
	FILE *f_out = fopen("video.vzip", "w");
	assert(f_out != NULL);

	ProcessAndZipArgs args[nfiles]; // Create an array of argument structures.

	for (int i = 0; i < nfiles; i++)
	{
		args[i].directory = argv[1];
		args[i].filename = files[i];
		args[i].f_out = f_out;
		args[i].total_in = &total_in;
		args[i].total_out = &total_out;

		printf("Creating thread %d\n", i);
		pthread_create(&threads[i], NULL, processAndZipFile, &args[i]);
	}

	// Wait for all threads to finish
	for (int i = 0; i < nfiles; i++)
	{
		pthread_join(threads[i], NULL);
	}
	fclose(f_out);

	printf("Compression rate: %.2lf%%\n", 100.0 * (total_in - total_out) / total_in);

	// release list of files
	for (int i = 0; i < nfiles; i++)
		free(files[i]);
	free(files);

	// do not modify the main function after this point!

	// time computation footer
	clock_gettime(CLOCK_MONOTONIC, &end);
	printf("Time: %.2f seconds\n", ((double)end.tv_sec + 1.0e-9 * end.tv_nsec) - ((double)start.tv_sec + 1.0e-9 * start.tv_nsec));
	// end of time computation footer

	return 0;
}

void *processAndZipFile(void *arg)
{
	ProcessAndZipArgs *args = (ProcessAndZipArgs *)arg;

	char *full_path = buildPath(args->directory, args->filename);

	unsigned char buffer_in[BUFFER_SIZE];
	unsigned char buffer_out[BUFFER_SIZE];

	int nbytes = loadFile(full_path, buffer_in);
	*(args->total_in) += nbytes;

	int nbytes_zipped = zipFile(buffer_in, nbytes, buffer_out, BUFFER_SIZE);
	assert(nbytes_zipped > 0);

	fwrite(&nbytes_zipped, sizeof(int), 1, args->f_out);
	fwrite(buffer_out, sizeof(unsigned char), nbytes_zipped, args->f_out);
	*(args->total_out) += nbytes_zipped;

	free(full_path);

	return NULL;
}

char *buildPath(const char *directory, const char *filename)
{
	int len = strlen(directory) + strlen(filename) + 2;
	char *full_path = malloc(len * sizeof(char));
	assert(full_path != NULL);

	strcpy(full_path, directory);
	strcat(full_path, "/");
	strcat(full_path, filename);

	return full_path;
}

int loadFile(const char *filepath, unsigned char *buffer)
{
	FILE *f_in = fopen(filepath, "rb");
	assert(f_in != NULL);
	int nbytes = fread(buffer, sizeof(unsigned char), BUFFER_SIZE, f_in);
	fclose(f_in);
	return nbytes;
}

void sortedListOfFiles(DIR *d, char ***files, int *nfiles)
{
	struct dirent *dir;

	while ((dir = readdir(d)) != NULL)
	{
		int len = strlen(dir->d_name);
		if (len > 4 && dir->d_name[len - 4] == '.' && dir->d_name[len - 3] == 'p' && dir->d_name[len - 2] == 'p' && dir->d_name[len - 1] == 'm')
		{
			*files = realloc(*files, (*nfiles + 1) * sizeof(char *));
			assert(*files != NULL);

			(*files)[*nfiles] = strdup(dir->d_name);
			assert((*files)[*nfiles] != NULL);

			(*nfiles)++;
		}
	}
	closedir(d);

	qsort(*files, *nfiles, sizeof(char *), cmp);
}

int zipFile(unsigned char *buffer_in, int input_size, unsigned char *buffer_out, int buffer_out_size)
{
	z_stream strm;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;

	int ret = deflateInit(&strm, Z_BEST_COMPRESSION);
	assert(ret == Z_OK);

	strm.avail_in = input_size;
	strm.next_in = buffer_in;
	strm.avail_out = buffer_out_size;
	strm.next_out = buffer_out;

	ret = deflate(&strm, Z_FINISH);
	assert(ret != Z_STREAM_ERROR); // Ensure there was no stream error

	int compressed_size = buffer_out_size - strm.avail_out;

	deflateEnd(&strm);

	return compressed_size; // Returns the size of the compressed data
}

int cmp(const void *a, const void *b)
{
	return strcmp(*(char **)a, *(char **)b);
}

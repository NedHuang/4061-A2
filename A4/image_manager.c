/*
   CSci4061 Spring 2017
   Assignment 4
   Jiatong Ruan
   Mingzhe Huang
 */


#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <libgen.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>

// global variables that count the threads / files
int dir_count = 0;
int thread_count = 0;
int jpg_count = 0;
int png_count = 0;
int bmp_count = 0;
int gif_count = 0;

// global variables, the file that we need to write,
FILE *catalog;
FILE *html;
FILE *output;
// time of modification
// https://www.tutorialspoint.com/c_standard_library/c_function_localtime.htm
struct tm* modification_time;
// represent the time in YY MM DD HH MM SS format
char time_output[512];
int running = 0;
int time_counter = 0;

// mutex for catalog.html
pthread_mutex_t html_lock;
// mutex for catalog.log
pthread_mutex_t catalog_lock;
// mutex for output.log
pthread_mutex_t outlog_lock;

// get the extension of a file.
// source: http://stackoverflow.com/questions/5309471/getting-file-extension-in-c
const char *get_extension(const char *filename) {
  const char *dot = strrchr(filename, '.');
  if(!dot || dot == filename) return "";
  return dot + 1;
}

// remove the extension of the file
// The C library function void *malloc(size_t size) allocates the requested memory and returns a pointer to it.
// memcopy: copies n characters from memory area str2 to memory area str1.
char *remove_ext(const char *filename)
{
  size_t len = strlen(filename);
  char *newfilename = malloc(len-3);
  memcpy(newfilename, filename, len-4);
  newfilename[len - 4] = 0;
  return newfilename;
}

// function that create the output_dir
void create_output_directory(const char *dirName) {
  DIR* odir = opendir(dirName);
  struct stat info;
  if(odir){
    opendir(odir);
  } else if(stat(dirName,&info) == -1){
    if (ENOENT == errno) {
      mkdir(dirName, 0700);
      odir = opendir(dirName);
    } else {
      perror("stat");
      exit(1);
    }
  } else {
    printf("Error, failed to open output_dir\n");
    exit(1);
  }
  closedir(odir);
}


// handle the signal
static void signal_handle(int s) {
	if (running)
	{
		time_counter += 1;
		pthread_mutex_lock(&catalog_lock);
		fprintf(catalog, "Time \t%d ms\t#dirs\t%d\t#files\t%d\n", time_counter, dir_count, jpg_count + bmp_count + png_count + gif_count);
		pthread_mutex_unlock(&catalog_lock);
	}
}

//  SIGALRM
static int setupinterrupt(void) {
	struct sigaction action;
	action.sa_handler = signal_handle;
	action.sa_flags = 0;
	return (sigemptyset(&action.sa_mask) || sigaction(SIGALRM, &action, NULL));
}
//set up TIMER_REAL ( 1 ms interval )
static int setupitimer(void) {
	struct itimerval value;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = 1000;
	value.it_value = value.it_interval;
	return (setitimer(ITIMER_REAL, &value, NULL));
}


// function for v1
void *v1_threads(const char* dir_path) {
  struct stat file_stat;
  struct dirent *dir_path_dirent;
  DIR *current_dir;
  current_dir = opendir(dir_path);
  pthread_mutex_lock(&outlog_lock);
  fprintf(output, "Start processing directory: %s\n", dir_path);
  pthread_mutex_unlock(&outlog_lock);
  // check if the directory exists and readable
  // dir_path_dirent = readdir(current_dir);
  while( NULL != current_dir && NULL != (dir_path_dirent = readdir(current_dir))){
    // exclude the case that . and .. is the name of entry
    // http://stackoverflow.com/questions/18428767/how-can-i-get-all-file-name-in-a-directory-without-getting-and-in-c
    if( strcmp(dir_path_dirent->d_name, ".." ) == 0 || strcmp(dir_path_dirent->d_name, "." ) == 0 ){
      continue;
    } else {
      // deal with the file name
      // int dir_length = strlen(dir_path);
      // int entry_name_length = strlen(dir_path_dirent->d_name);
      // int filename_length = dir_length + entry_name_length + 2;
      char filename[512];
      strcpy(filename, dir_path);
      strcat(filename, "/");
      strcat(filename, dir_path_dirent->d_name);
      if (0 != stat(filename, &file_stat)) {
        continue;
      } else {
        // test if this is a directory http://pubs.opengroup.org/onlinepubs/7908799/xsh/sysstat.h.html
        pthread_t new_thread;
        if (S_ISDIR(file_stat.st_mode)) {
          //  suspend execution of the calling thread (this one) until the target thread terminates
          pthread_join(new_thread, NULL);
          pthread_mutex_lock(&outlog_lock);
          fprintf(output, "Directory %s found \n\t in %s\n", filename, dir_path);
          pthread_mutex_unlock(&outlog_lock);
          thread_count++;
          // start a new thread
          // int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);
          pthread_create(&new_thread, NULL, (void *) & v1_threads, (void *) filename);
          dir_count++;
          pthread_join(new_thread, NULL);
        } else {
          // test if the file is a regular file, check file types, write log and html files and update jpg_count;
          if(S_ISREG(file_stat.st_mode)) {
            char ext = get_extension(filename);
            if (strcmp(ext, "jpg") == 0){
              pthread_mutex_lock(&outlog_lock);
              fprintf(output, "Filename:  %s, Type : jpg, Location \
                  %s\n", filename, dir_path);
              pthread_mutex_unlock(&outlog_lock);
              jpg_count ++;
              pthread_mutex_lock(&html_lock);
              fprintf(html, "<a href=\"../%s\">\n<img src=\"%s\" width=100 height =100></img></a><p align= \"left\">", filename, filename);
              pthread_mutex_unlock(&html_lock);
              // get the thread id
              pthread_t thread_id = pthread_self();
              // get the time of data modification, http://pubs.opengroup.org/onlinepubs/7908799/xsh/sysstat.h.html
              modification_time = localtime(&file_stat.st_mtime);
              // format the time  https://www.tutorialspoint.com/c_standard_library/time_h.htm
              // char time_output[512];
              strftime (time_output, sizeof (time_output), "%Y %b %d %H:%M:%S", modification_time);
              // write infos about the img into the html.
              fprintf(html, " FileID: %llu FileName: %s FileType: jpg Size: %lld TimeofModification: \
                  %s ThreadID: %lu </p>\n", file_stat.st_ino, dir_path_dirent->d_name, file_stat.st_size, time_output, pthread_self());
              pthread_mutex_unlock(&html_lock);
            }
            else if(strcmp(ext, "gif") == 0) {
              pthread_mutex_lock(&outlog_lock);
              fprintf(output, "Filename:  %s, Type : gif, Location %s\n", filename, dir_path);
              pthread_mutex_unlock(&outlog_lock);
              gif_count ++;
              pthread_mutex_lock(&html_lock);
              fprintf(html, "<a href=\"../%s\">\n<img src=\"%s\" width=100 \
                  height =100></img></a><p align= \"left\">", filename, filename);
              pthread_mutex_unlock(&html_lock);
              // get the thread id
              pthread_t thread_id = pthread_self();
              // get the time of data modification, http://pubs.opengroup.org/onlinepubs/7908799/xsh/sysstat.h.html
              modification_time = localtime(&file_stat.st_mtime);
              // format the time  https://www.tutorialspoint.com/c_standard_library/time_h.htm
              // char time_output[512];
              strftime (time_output, sizeof (time_output), "%Y %b %d %H:%M:%S", modification_time);
              // write infos about the img into the html.
              fprintf(html, " FileID: %llu FileName: %s FileType: gif Size: %lld TimeofModification: \
                  %s ThreadID: %lu </p>\n", file_stat.st_ino, dir_path_dirent->d_name, file_stat.st_size, time_output, pthread_self());
              pthread_mutex_unlock(&html_lock);
            }
            else if (strcmp(ext, "bmp") == 0) {
              pthread_mutex_lock(&outlog_lock);
              fprintf(output, "Filename:  %s, Type : jpg, Location %s\n", filename, dir_path);
              pthread_mutex_unlock(&outlog_lock);
              bmp_count ++;
              pthread_mutex_lock(&html_lock);
              fprintf(html, "<a href=\"../%s\">\n<img src=\"%s\" width=100 \
                  height =100></img></a><p align= \"left\">", filename, filename);
              pthread_mutex_unlock(&html_lock);
              // get the thread id
              pthread_t thread_id = pthread_self();
              // get the time of data modification, http://pubs.opengroup.org/onlinepubs/7908799/xsh/sysstat.h.html
              modification_time = localtime(&file_stat.st_mtime);
              // format the time  https://www.tutorialspoint.com/c_standard_library/time_h.htm
              // char time_output[512];
              strftime (time_output, sizeof (time_output), "%Y %b %d %H:%M:%S", modification_time);
              // write infos about the img into the html.
              fprintf(html, " FileID: %llu FileName: %s FileType: bmp Size: %lld TimeofModification: \
                  %s ThreadID: %lu </p>\n", file_stat.st_ino, dir_path_dirent->d_name, file_stat.st_size, time_output, pthread_self());
              pthread_mutex_unlock(&html_lock);
            }
            else if (strcmp(ext, "png") == 0) {
              pthread_mutex_lock(&outlog_lock);
              fprintf(output, "Filename:  %s, Type : jpg, Location %s\n", filename, dir_path);
              pthread_mutex_unlock(&outlog_lock);
              png_count ++;
              pthread_mutex_lock(&html_lock);
              fprintf(html, "<a href=\"../%s\">\n<img src=\"%s\" width=100 \
                  height =100></img></a><p align= \"left\">", filename, filename);
              pthread_mutex_unlock(&html_lock);
              // get the thread id
              pthread_t thread_id = pthread_self();
              // get the time of data modification, http://pubs.opengroup.org/onlinepubs/7908799/xsh/sysstat.h.html
              modification_time = localtime(&file_stat.st_mtime);
              // format the time  https://www.tutorialspoint.com/c_standard_library/time_h.htm
              // char time_output[512];
              strftime (time_output, sizeof (time_output), "%Y %b %d %H:%M:%S", modification_time);
              // write infos about the img into the html.
              fprintf(html, " FileID: %llu FileName: %s FileType: png Size: %lld TimeofModification: \
                  %s ThreadID: %lu </p>\n", file_stat.st_ino, dir_path_dirent->d_name, file_stat.st_size, time_output, pthread_self());
              pthread_mutex_unlock(&html_lock);
            }
            else{
              continue;
            }
          } else {
            continue;
          }
        }
      }
    }
  }
}

void *v3_threads(const char* dir_path){
  // create a list that accomodate the sub directories and a counter to count # of files
  // http://stackoverflow.com/questions/1121383/counting-the-number-of-files-in-a-directory-using-c
  int sub_dir_count = 0;
  DIR *d;
	struct dirent * entry;
  d = opendir(dir_path);
  while ((entry = readdir(d)) != NULL) {
    if (entry->d_type == DT_DIR) { /* If the entry is a directory */
      sub_dir_count ++;
    }
  }
  closedir(d);

  // create a list for all sub directories;
  const char *sub_dir_list[sub_dir_count][512];
  struct stat file_stat;
  struct dirent *dir_path_dirent;
  DIR *current_dir;
  current_dir = opendir(dir_path);
  pthread_mutex_lock(&outlog_lock);
  fprintf(output, "Start processing in  directory: %s\n", dir_path);
  pthread_mutex_unlock(&outlog_lock);
  // check how many files are there in this directory
  // read from the directory, if the entry is a directory, store the name in the list
  int count = 0;
  while( NULL != current_dir && NULL != (dir_path_dirent = readdir(current_dir))) {
    if( strcmp(dir_path_dirent->d_name, ".." ) == 0 || strcmp(dir_path_dirent->d_name, "." ) == 0 ){
      continue;
    } else {
      if (S_ISDIR(file_stat.st_mode)) {
        // sub_dir_list[count] = (char *) malloc(1024);
        strcpy(sub_dir_list[count], dir_path);
        strcat(sub_dir_list[count], "/");
        strcat(sub_dir_list[count], dir_path_dirent->d_name);
        count ++;
      }
    }
  }

  while( NULL != current_dir && NULL != (dir_path_dirent = readdir(current_dir))) {
    if( strcmp(dir_path_dirent->d_name, ".." ) == 0 || strcmp(dir_path_dirent->d_name, "." ) == 0 ){
      continue;
    } else {
			char filename[512];
      strcpy(filename, dir_path);
      strcat(filename, "/");
      strcat(filename, dir_path_dirent->d_name);
      if (0 != stat(filename, &file_stat)) {
        continue;
      } else {
        pthread_t new_thread;
        // if the file is regular file:
        if(S_ISREG(file_stat.st_mode)) {
          char ext = get_extension(filename);
          if (strcmp(ext, "jpg") == 0){
            pthread_mutex_lock(&outlog_lock);
            fprintf(output, "Filename:  %s, Type : jpg, Location %s\n", filename, dir_path);
            pthread_mutex_unlock(&outlog_lock);
            jpg_count ++;
            pthread_mutex_lock(&html_lock);
            fprintf(html, "<a href=\"../%s\">\n<img src=\"%s\" width=100 height =100></img></a><p align= \"left\">", filename, filename);
              pthread_mutex_unlock(&html_lock);
            // get the thread id
            pthread_t thread_id = pthread_self();
            // get the time of data modification, http://pubs.opengroup.org/onlinepubs/7908799/xsh/sysstat.h.html
            modification_time = localtime(&file_stat.st_mtime);
            // format the time  https://www.tutorialspoint.com/c_standard_library/time_h.htm
            // char time_output[512];
            strftime (time_output, sizeof (time_output), "%Y %b %d %H:%M:%S", modification_time);
            // write infos about the img into the html.
            fprintf(html, " FileID: %llu FileName: %s FileType: jpg Size: %lld TimeofModification: %s ThreadID: %lu </p>\n", file_stat.st_ino, dir_path_dirent->d_name, file_stat.st_size, time_output, thread_id);
            pthread_mutex_unlock(&html_lock);
          } else if(strcmp(ext, "gif") == 0) {
            pthread_mutex_lock(&outlog_lock);
            fprintf(output, "Filename:  %s, Type : gif, Location %s\n", filename, dir_path);
            pthread_mutex_unlock(&outlog_lock);
            gif_count ++;
            pthread_mutex_lock(&html_lock);
            fprintf("html, <a href=\"../%s\">\n<img src=\"%s\" width=100 height =100></img></a><p align= \"left\">", filename, filename);
              pthread_mutex_unlock(&html_lock);
            // get the thread id
            pthread_t thread_id = pthread_self();
            // get the time of data modification, http://pubs.opengroup.org/onlinepubs/7908799/xsh/sysstat.h.html
            modification_time = localtime(&file_stat.st_mtime);
            // format the time  https://www.tutorialspoint.com/c_standard_library/time_h.htm
            // char time_output[512];
            strftime (time_output, sizeof (time_output), "%Y %b %d %H:%M:%S", modification_time);
            // write infos about the img into the html.
            fprintf(html, " FileID: %llu FileName: %s FileType: gif Size: %lld TimeofModification: %s ThreadID: %lu </p>\n", file_stat.st_ino, dir_path_dirent->d_name, file_stat.st_size, time_output, thread_id);
            pthread_mutex_unlock(&html_lock);
          }
          else if (strcmp(ext, "bmp")==0) {
              pthread_mutex_lock(&outlog_lock);
              fprintf(output, "Filename:  %s, Type : jpg, Location %s\n", filename, dir_path);
              pthread_mutex_unlock(&outlog_lock);
              bmp_count ++;
              pthread_mutex_lock(&html_lock);
              fprintf(html, "<a href=\"../%s\">\n<img src=\"%s\" width=100 height =100></img></a><p align= \"left\">", filename, filename);
              pthread_mutex_unlock(&html_lock);
              // get the thread id
              pthread_t thread_id = pthread_self();
              // get the time of data modification, http://pubs.opengroup.org/onlinepubs/7908799/xsh/sysstat.h.html
              modification_time = localtime(&file_stat.st_mtime);
              // format the time  https://www.tutorialspoint.com/c_standard_library/time_h.htm
              // char time_output[512];
              strftime (time_output, sizeof (time_output), "%Y %b %d %H:%M:%S", modification_time);
              // write infos about the img into the html.
              fprintf(html, " FileID: %llu FileName: %s FileType: bmp Size: %lld TimeofModification: %s ThreadID: %lu </p>\n", file_stat.st_ino, dir_path_dirent->d_name, file_stat.st_size, time_output, thread_id);
              pthread_mutex_unlock(&html_lock);
						} else if (strcmp(ext, "png") == 0) {
                pthread_mutex_lock(&outlog_lock);
                fprintf(output, "Filename:  %s, Type : jpg, Location %s\n", filename, dir_path);
                pthread_mutex_unlock(&outlog_lock);
                png_count ++;
                pthread_mutex_lock(&html_lock);
                fprintf(html, "<a href=\"../%s\">\n<img src=\"%s\" width=100 height =100></img></a><p align= \"left\">", filename, filename);
                pthread_mutex_unlock(&html_lock);
                // get the thread id
                pthread_t thread_id = pthread_self();
                // get the time of data modification, http://pubs.opengroup.org/onlinepubs/7908799/xsh/sysstat.h.html
                modification_time = localtime(&file_stat.st_mtime);
                // format the time  https://www.tutorialspoint.com/c_standard_library/time_h.htm
                // char time_output[512];
                strftime (time_output, sizeof (time_output), "%Y %b %d %H:%M:%S", modification_time);
                // write infos about the img into the html.
                fprintf(html, " FileID: %llu FileName: %s FileType: png Size: %lld TimeofModification: %s ThreadID: %lu </p>\n", file_stat.st_ino, dir_path_dirent->d_name, file_stat.st_size, time_output, thread_id);
                pthread_mutex_unlock(&html_lock);
              }
            }
        }
    }
}

    for (int i = 0; i < sub_dir_count; i++){
      pthread_t new_thread;
      //  suspend execution of the calling thread (this one) until the target thread terminates
      pthread_create(&new_thread, NULL, (void *) & v3_threads, (void *) sub_dir_list[i]);

      pthread_join(new_thread, NULL);
      pthread_mutex_lock(&outlog_lock);
      fprintf(output, "Directory %s found \n\t in %s\n", sub_dir_list[i], dir_path);
      pthread_mutex_unlock(&outlog_lock);
      thread_count++;
      // start a new thread
      // int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);
      dir_count++;
      pthread_join(new_thread, NULL);
    }
  }



  // function to check images, for jpg, gif, png, bmp and directory
void *jpg_file_check(const char* dir_path) {
	  struct stat file_stat;
	  struct dirent *dir_path_dirent;
	  DIR *current_dir;
	  current_dir = opendir(dir_path);
	  pthread_mutex_lock(&outlog_lock);
	  fprintf(output, "checking jpg files in %s\n", dir_path);
	  pthread_mutex_unlock(&outlog_lock);
	  while (NULL != current_dir && NULL != (dir_path_dirent = readdir(current_dir))) {
	    if (0 == strcmp(dir_path_dirent->d_name, ".") || 0 == strcmp(dir_path_dirent->d_name, ".."))
	    {
	      continue;
	    }
	    char filename[512];
	    strcpy(filename, dir_path);
	    strcat(filename, "/");
	    strcat(filename, dir_path_dirent->d_name);
	    if (0 == stat(filename, &file_stat))
	    {
	      // check if the file is a file
	      if (S_ISREG(file_stat.st_mode))
	      {
	        // check if the file is of type jpg
	        if(0 == strcmp(get_extension(filename), "jpg"))
	        {
	          pthread_mutex_lock(&outlog_lock);
	          fprintf(output, "jpg file %s is found in %s\n", filename, dir_path);
	          pthread_mutex_unlock(&outlog_lock);
	          pthread_mutex_lock(&html_lock);
	          // append to html file
	          fprintf(html, "<a href=\"%s\">\n<img src=\"%s\" width=100 height =100></img></a><p align= \"left\">", filename, filename);
	          pthread_t thread_id;
	          thread_id = pthread_self();
	          modification_time = localtime (&file_stat.st_mtime);
	          strftime (time_output, sizeof (time_output), "%Y %b %d %H:%M:%S", modification_time);
	          fprintf(html, " FileID: %llu FileName: %s FileType: jpg Size: %lld TimeofModification: %s ThreadID: %lu </p>\n", file_stat.st_ino, dir_path_dirent->d_name, file_stat.st_size, time_output, thread_id);
	          pthread_mutex_unlock(&html_lock);
	          jpg_count++;
	        }
	      } else {continue;}
	    } else {continue;}
	  }
    pthread_mutex_lock(&outlog_lock);
    fprintf(output, "Finished checking jpg files in %s\n", dir_path);
    pthread_mutex_unlock(&outlog_lock);
    pthread_exit(0);
}


void *gif_file_check(const char* dir_path) {
		  struct stat file_stat;
		  struct dirent *dir_path_dirent;
		  DIR *current_dir;
		  current_dir = opendir(dir_path);
		  pthread_mutex_lock(&outlog_lock);
		  fprintf(output, "checking gif files in %s\n", dir_path);
		  pthread_mutex_unlock(&outlog_lock);
		  while (NULL != current_dir && NULL != (dir_path_dirent = readdir(current_dir))) {
		    if (0 == strcmp(dir_path_dirent->d_name, ".") || 0 == strcmp(dir_path_dirent->d_name, ".."))
		    {
		      continue;
		    }
		    char filename[512];
		    strcpy(filename, dir_path);
		    strcat(filename, "/");
		    strcat(filename, dir_path_dirent->d_name);
		    if (0 == stat(filename, &file_stat))
		    {
		      // check if the file is a file
		      if (S_ISREG(file_stat.st_mode))
		      {
		        // check if the file is of type   gif
		        if(0 == strcmp(get_extension(filename), "  gif"))
		        {
		          pthread_mutex_lock(&outlog_lock);
		          fprintf(output, "  gif file %s is found in %s\n", filename, dir_path);
		          pthread_mutex_unlock(&outlog_lock);
		          pthread_mutex_lock(&html_lock);
		          // append to html file
		          fprintf(html, "<a href=\"%s\">\n<img src=\"%s\" width=100 height =100></img></a><p align= \"left\">", filename, filename);
		          pthread_t thread_id;
		          thread_id= pthread_self();
		          modification_time = localtime (&file_stat.st_mtime);
		          strftime (time_output, sizeof (time_output), "%Y %b %d %H:%M:%S", modification_time);
		          fprintf(html, " FileID: %llu FileName: %s FileType:   gif Size: %lld TimeofModification: %s ThreadID: %lu </p>\n", file_stat.st_ino, dir_path_dirent->d_name, file_stat.st_size, time_output, thread_id);

		          pthread_mutex_unlock(&html_lock);
		            gif_count++;
		        }
		      } else {continue;}
		    } else {continue;}
		  }
	    pthread_mutex_lock(&outlog_lock);
	    fprintf(output, "Finished checking gif files in %s\n", dir_path);
	    pthread_mutex_unlock(&outlog_lock);
	    pthread_exit(0);
}

void *png_file_check(const char* dir_path) {
	struct stat file_stat;
	struct dirent *dir_path_dirent;
	DIR *current_dir;
	current_dir = opendir(dir_path);
	pthread_mutex_lock(&outlog_lock);
	fprintf(output, "checking gif files in %s\n", dir_path);
	pthread_mutex_unlock(&outlog_lock);
	while (NULL != current_dir && NULL != (dir_path_dirent = readdir(current_dir))) {
		if (0 == strcmp(dir_path_dirent->d_name, ".") || 0 == strcmp(dir_path_dirent->d_name, "..")) { continue; }
		char filename[512];
		strcpy(filename, dir_path);
		strcat(filename, "/");
		strcat(filename, dir_path_dirent->d_name);
		if (0 == stat(filename, &file_stat))
		{
		  // check if the file is a file
		  if (S_ISREG(file_stat.st_mode))
		  {
		    // check if the file is of type   png
		    if(0 == strcmp(get_extension(filename), "  png"))
		    {
		      pthread_mutex_lock(&outlog_lock);
		      fprintf(output, "  png file %s is found in %s\n", filename, dir_path);
		      pthread_mutex_unlock(&outlog_lock);
		      pthread_mutex_lock(&html_lock);
		      // append to html file
		      fprintf(html, "<a href=\"%s\">\n<img src=\"%s\" width=100 height =100></img></a><p align= \"left\">", filename, filename);
		      pthread_t thread_id;
		      thread_id= pthread_self();
		      modification_time = localtime (&file_stat.st_mtime);
		      strftime (time_output, sizeof (time_output), "%Y %b %d %H:%M:%S", modification_time);
		      fprintf(html, " FileID: %llu FileName: %s FileType:   png Size: %lld TimeofModification: %s ThreadID: %lu </p>\n", file_stat.st_ino, dir_path_dirent->d_name, file_stat.st_size, time_output, thread_id);

		      pthread_mutex_unlock(&html_lock);
		        png_count++;
		    }
		  } else {continue;}
		} else {continue;}
			  }
		pthread_mutex_lock(&outlog_lock);
	  fprintf(output, "Finished checking   png files in %s\n", dir_path);
		pthread_mutex_unlock(&outlog_lock);
		pthread_exit(0);
}

void *bmp_file_check(const char* dir_path) {
	struct stat file_stat;
	struct dirent *dir_path_dirent;
	DIR *current_dir;
	current_dir = opendir(dir_path);
	pthread_mutex_lock(&outlog_lock);
	fprintf(output, "checking gif files in %s\n", dir_path);
	pthread_mutex_unlock(&outlog_lock);
	while (NULL != current_dir && NULL != (dir_path_dirent = readdir(current_dir))) {
		if (0 == strcmp(dir_path_dirent->d_name, ".") || 0 == strcmp(dir_path_dirent->d_name, "..")) { continue; }
		char filename[512];
		strcpy(filename, dir_path);
		strcat(filename, "/");
		strcat(filename, dir_path_dirent->d_name);
		if (0 == stat(filename, &file_stat))
		{
		  // check if the file is a file
		  if (S_ISREG(file_stat.st_mode))
		  {
		    // check if the file is of type   bmp
		    if(0 == strcmp(get_extension(filename), "  bmp"))
		    {
		      pthread_mutex_lock(&outlog_lock);
		      fprintf(output, "  bmp file %s is found in %s\n", filename, dir_path);
		      pthread_mutex_unlock(&outlog_lock);
		      pthread_mutex_lock(&html_lock);
		      // append to html file
		      fprintf(html, "<a href=\"%s\">\n<img src=\"%s\" width=100 height =100></img></a><p align= \"left\">", filename, filename);
		      pthread_t thread_id;
		      thread_id = pthread_self();
		      modification_time = localtime (&file_stat.st_mtime);
		      strftime (time_output, sizeof (time_output), "%Y %b %d %H:%M:%S", modification_time);
		      fprintf(html, " FileID: %llu FileName: %s FileType:   bmp Size: %lld TimeofModification: %s ThreadID: %lu </p>\n", file_stat.st_ino, dir_path_dirent->d_name, file_stat.st_size, time_output, thread_id);

		      pthread_mutex_unlock(&html_lock);
		        bmp_count++;
		    }
		  } else {continue;}
		} else {continue;}
			  }
		    pthread_mutex_lock(&outlog_lock);
		    fprintf(output, "Finished checking   bmp files in %s\n", dir_path);
		    pthread_mutex_unlock(&outlog_lock);
		    pthread_exit(0);
		}

		void *dir_check(const char* dir_path)
		{
			struct stat file_stat;
			struct dirent *dir_path_dirent;
			//struct stat s;
			//struct dirent *dir_path_dirent;
			DIR *current_dir;
			current_dir = opendir(dir_path);
			pthread_mutex_lock(&outlog_lock);
			fprintf(output, "Start checking directories in %s\n", dir_path);
			pthread_mutex_unlock(&outlog_lock);
			while (NULL != current_dir && NULL != (dir_path_dirent = readdir(current_dir)))
			{
				// ignore the these 2 directories
				if (0 == strcmp(dir_path_dirent->d_name, ".") || 0 == strcmp(dir_path_dirent->d_name, ".."))
				{
					continue;
				}
				char filename[512];
				strcpy(filename, dir_path);
				strcat(filename, "/");
				strcat(filename, dir_path_dirent->d_name);
				if (0 == stat(filename, &file_stat))
				{
					// check if the file is a directory
					if (S_ISDIR(file_stat.st_mode))
					{
						// create 5 threads
						dir_count ++; // increase the count of files.
						pthread_mutex_lock(&outlog_lock);
						fprintf(output, "Directory: %s found in\n\t %s, creating threads\n", filename, dir_path);
						pthread_mutex_unlock(&outlog_lock);
						pthread_t dir_thread;
						pthread_t jpg_thread;
						pthread_t png_thread;
						pthread_t bmp_thread;
						pthread_t gif_thread;

						thread_count += 5; // increase the thread count

						pthread_create(&dir_thread, NULL, (void *) &dir_check, (void *) filename);
						pthread_create(&jpg_thread, NULL, (void *) &jpg_file_check, (void *) filename);
						pthread_create(&png_thread, NULL, (void *) &png_file_check, (void *) filename);
						pthread_create(&bmp_thread, NULL, (void *) &bmp_file_check, (void *) filename);
						pthread_create(&gif_thread, NULL, (void *) &gif_file_check, (void *) filename);

						pthread_join(dir_thread, NULL);
						pthread_join(jpg_thread, NULL);
						pthread_join(png_thread, NULL);
						pthread_join(bmp_thread, NULL);
						pthread_join(gif_thread, NULL);
					}
					else
					{
						continue;
					}
				}
				else
				{
					continue;
				}
			}
			pthread_mutex_lock(&outlog_lock);
			fprintf(output, "Checking directories in %s\n finished", dir_path);
			pthread_mutex_unlock(&outlog_lock);
			pthread_exit(0);
		}
// split threads to 5 for a directory
void *splitthreads(const char* path) {
			// create 5 threads
			pthread_t dir_thread;
			pthread_t jpg_thread;
			pthread_t png_thread;
			pthread_t bmp_thread;
			pthread_t gif_thread;

			thread_count += 5;

			pthread_create(&dir_thread, NULL, (void *) &dir_check, (void *) path);
			pthread_create(&jpg_thread, NULL, (void *) &jpg_file_check, (void *) path);
			pthread_create(&png_thread, NULL, (void *) &png_file_check, (void *) path);
			pthread_create(&bmp_thread, NULL, (void *) &bmp_file_check, (void *) path);
			pthread_create(&gif_thread, NULL, (void *) &gif_file_check, (void *) path);

			pthread_join(dir_thread, NULL);
			pthread_join(jpg_thread, NULL);
			pthread_join(png_thread, NULL);
			pthread_join(bmp_thread, NULL);
			pthread_join(gif_thread, NULL);

			pthread_exit(0);
		}


/*******************************************************************************
    main function
*******************************************************************************/

  int main(int argc, char *argv[]) {
    // Check validility of user input
    if (argc != 4) {
      printf("usage: ./image_manager [varient_options] ~/[input_path] /[output_dir]");
      exit(1);
    }
    if(strcmp(argv[1], "v1") != 0 && strcmp(argv[1], "v2") == 0 && strcmp(argv[1], "v3") == 0) {
      printf("varient_options should be \"v1\" \"v2\" or \"v3\" ");
      exit(0);
    }

    // Try to open input directory
    DIR *input_dir = opendir(argv[2]);
    if (NULL == input_dir)
    {
      printf("Error: Input directory does not exist/ not readable\n");
      exit(1);
    }

		if (setupinterrupt() == -1) {
		  perror("Failed to set up handler for SIGALRM");
		  return 1;
	  }

		if (setupitimer() == -1) {
			perror("Failed to set up the ITIMER_REAL interval timer");
			return 1;
		}
/*华丽丽的分割线华丽丽的分割线华丽丽的分割线华丽丽的分割线华丽丽的分割线华丽丽的分割线华丽丽的分割线
                              create files and directory 
华丽丽的分割线华丽丽的分割线华丽丽的分割线华丽丽的分割线华丽丽的分割线华丽丽的分割线华丽丽的分割线*/
    // Create output directory
    char *cur_dir = getcwd(NULL, 0);
    // printf("%s\n", cur_dir);
    // out_dir is the output directory
    char out_dir[512];
    strcpy(out_dir, cur_dir);
    strcat(out_dir, "/");
    strcat(out_dir, argv[3]);
    while (out_dir[strlen(out_dir)-1] == '/') {
      out_dir[strlen(out_dir)-1] = '\0';
    }
    // printf("%s\n",out_dir );

    create_output_directory(argv[3]);

    // Create output log which is used for debuging
    char output_log[512];
    strcpy(output_log, out_dir);
    strcat(output_log, "/output.log");
    output = fopen(output_log, "w");
    if (NULL == output)
    {
      printf("Faile to create output.log\n");
      exit(1);
    }
    fprintf(output, "-------------------Varient_options: %s, Input_dir: %s, Output_dir: %s -------------------\n", argv[1], argv[2], argv[3]);
    fprintf(output, "Output.log is created.\n");
    // printf( "Output.log is created.\n");

    // Create catalog.html
    char catalog_log_pwd[512];
    strcpy(catalog_log_pwd, out_dir);
    strcat(catalog_log_pwd, "/catalog.log");
    catalog = fopen(catalog_log_pwd, "a");
    if (NULL == catalog)
    {
      fprintf(output, "Failed to open catalog.log\n");
      exit(1);
    }
    fprintf(output, "Catalog.log is created\n");
    printf("Catalog.log is created\n");

    //
    // Start to write on html file

    char html_pwd[512];
    strcpy(html_pwd, out_dir);
    strcat(html_pwd, "/catalog.html");
    html = fopen(catalog_log_pwd, "w");
    if (NULL == html)
    {
      fprintf(output, "Failed to open html file\n");
      exit(1);
    }
    fprintf(output, "html is created\n");

    //html = fopen(html_pwd, "w");
    printf("html is created\n");
    fprintf(html, "<html><head><title>Image Manager BMP</title></head><body>\n");


  }
/*******************************************************************************/

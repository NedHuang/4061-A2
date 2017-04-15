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
struct tm modification_time;
// represent the time in YY MM DD HH MM SS format
char time_output[1024];

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


// function for v1

void *v1_threads(const char* dir_path) {
  struct stat file_stat;
  struct dirent *dir_path_dirent;
  DIR *current_dir;
  current_dir = opendir(dir_path);
  pthread_mutex_lock(&outlog_lock);
  fprintf(o_log, "Start processing directory: %s\n", dir_path);
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
      char filename[1024];
      strcpy(filename, dir_path);
      strcat(filename, "/");
      strcat(filename, dir_path_dirent->d_name);
      if (0 != stat(filename, &file_stat)) {
        continue;
      } else {
        // test if this is a directory http://pubs.opengroup.org/onlinepubs/7908799/xsh/sysstat.h.html
        pthread_t new_thread;
        if (S_ISDIR(file_stat.s_mode)) {
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
            if(S_ISREG(file_stat.s_mode)) {
              char ext = get_extension(filename);
              if (strcmp(ext, "jpg")){
                pthread_mutex_lock(&outlog_lock);
  					    fprintf(output, "Filename:  %s, Type : jpg, Location %s\n", filename, dir_path);
  				      pthread_mutex_unlock(&outlog_lock);
                jpg_count ++;
                pthread_mutex_lock(&html_lock);
                fprintf(html, <a href=\"../%s\">\n<img src=\"%s\" width=100 height =100></img></a><p align= \"left\">", filename, filename)
                pthread_mutex_unlock(&html_lock);
                // get the thread id
                pthread_t thread_id = pthread_self();
                // get the time of data modification, http://pubs.opengroup.org/onlinepubs/7908799/xsh/sysstat.h.html
                modification_time = localtime(&s.st_mtime);
                // format the time  https://www.tutorialspoint.com/c_standard_library/time_h.htm
                // char time_output[1024];
                strftime (time_output, sizeof (time_output), "%Y %b %d %H:%M:%S", modification_time);
                // write infos about the img into the html.
                fprintf(html, " FileID: %llu FileName: %s FileType: jpg Size: %lld TimeofModification: %s ThreadID: %lu </p>\n", s.st_ino, pDirent->d_name, s.st_size, time_output, id);
                pthread_mutex_unlock(&html_lock);
              }

            }
          }
        }

      }
    }

  }
}





// function to check images
void *image_check(const char* path) {

}












/*******************************************************************************
main function
*******************************************************************************/




int main(int argc, char *argv[]) {
  if (argc != 4) {
    printf("usage: ./image_manager [varient_options]
    ~/[input_path] /[output_dir]");
    exit(1);
  }
  if(strcmp(argv[1], "v1") != 0 && strcmp(argv[1], "v2") == 0 && strcmp(argv[1], "v3") == 0) {
    printf("varient_options should be \"v1\" \"v2\" or \"v3\" ");
    exit(0);
  }


}

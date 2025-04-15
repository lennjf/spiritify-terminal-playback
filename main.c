#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <sys/stat.h>
#include <mytool.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include "SDL_mixer.h"


int current_playing = 0;
filelist fl;
Mix_Music* music = NULL;
char* addr = NULL;
int stop = 0;


void config(int *argc, char*** argv);
void play_mp3();
void play();
void list();
void mp3_pause();
void mp3_pre();
void mp3_post();
void mp3_stop();
void set_vol(int vol_num);
void mp3_jump(int num);

enum {PLAY, JUMP, PAUSE, LIST, PRE, POST, VOL, STOP};

#define SHM_SIZE 50000
#define FILE_LIST_MEM_LENS (100 * 100)
#define FILE_LIST_ROWS 100
#define FILE_LIST_COLUMN 100

/**
* 0 ==== pid
* 10 ==== file_nums
* 20 ==== current_playing
* 10000 ==== mp3 file paths
* 50000(old) => 30000 bytes ==== action
* 60000(old) => 40000 ==== data send to service
**/
#define SHM_OFFSET_FILE_LENS 10
#define SHM_OFFSET_CURR_PLAYING 20
#define SHM_OFFSET_MP3_PATHS 10000
#define SHM_OFFSET_ACTION 30000
#define SHM_OFFSET_DATA_SEND 40000


int main(int argc, char **argv){
    if(argc < 2){
        printf("Usage: spiritify <config | play | jump | pause | list | pre | post | vol | stop | kill>\n");
        return 0;
    }
    if(strcmp(argv[1], "config") == 0){
        config(&argc, &argv);
        return 0;
    }

    if(strcmp(argv[1], "play") == 0){
        play_mp3();
        return 0;
    }

    if(strcmp(argv[1], "pause") == 0){
        mp3_pause();
        return 0;
    }


    if(strcmp(argv[1], "jump") == 0){
        if(argc < 3){
            printf("Usage : spiritfy jump <index num>\n");
        }
        mp3_jump(atoi(argv[2]));
        return 0;
    }
    
    if(strcmp(argv[1], "pre") == 0){
        mp3_pre();
        return 0;
    }

    if(strcmp(argv[1], "post") == 0){
        mp3_post();
        return 0;
    }
    
    if(strcmp(argv[1], "stop") == 0){
        mp3_stop();
        return 0;
    }

    if(strcmp(argv[1], "vol") == 0){
        if(argc < 3){
            printf("Usage : spiritify vol <volume-number(0-128)>\n");
            return 0;
        }
        set_vol(atoi(argv[2]));
        return 0;
    }

    if(strcmp(argv[1], "kill") == 0){
        system("ps aux|grep 'spiritify play'  |grep -v grep | awk '{print $2}'|xargs kill -2");
    } 
    
    if(strcmp(argv[1], "list") == 0){
        list();
    } 

    return 0;
}

void config(int *argc, char*** argv){
    printf("config\n");
    gtk_init(argc, argv);
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
    dialog = gtk_file_chooser_dialog_new("choose a mp3 directory", NULL, 
                                         GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, 
                                         "cancel", GTK_RESPONSE_CANCEL, 
                                         "confirm", GTK_RESPONSE_ACCEPT, 
                                         NULL);
    int res = gtk_dialog_run(GTK_DIALOG(dialog));
    if(res == GTK_RESPONSE_ACCEPT){
        char *mp3dir_path;
        mp3dir_path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        printf("mp3dir: %s\n", mp3dir_path);
        char *config_home_path;
        config_home_path = getenv("XDG_CONFIG_HOME");
        if(strlen(config_home_path) == 0){
            printf("pls set config_home_path by 'export XDG_CONFIG_HOME=your_config_home_path'\n");
            return;
        }
        char *spiritify_path_end = "/spiritify";
        char *spiritify_config_abs_path;
        spiritify_config_abs_path = strcat(config_home_path, spiritify_path_end);
        if(check_file_or_dir_exists(spiritify_config_abs_path) != 1){
            if(mkdir(spiritify_config_abs_path, 0755) == -1){
                perror("create config dir path");
                return;
            }        
        }
        char *spiritify_config_file_end = "/spiritify.txt";
        char *spiritify_config_file_path = strcat(spiritify_config_abs_path, spiritify_path_end);
        FILE *file = fopen(spiritify_config_file_path, "w");
        if(file == NULL){
            perror("fopen config file");
            return;
        }
        int items_write = fwrite(mp3dir_path, strlen(mp3dir_path), 1, file);
        printf("write mp3dir into config path [%lu bytes]\n", strlen(mp3dir_path) * items_write);
        
        fclose(file);
    }else {
        printf("canceled\n");
    }
    gtk_widget_destroy(dialog);
}

void list(){
    int shm_fd = shm_open("spiritify", O_RDWR, 0700); 
    if(shm_fd == -1){
        printf("service not launched\n");
    }else{
        addr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        int *fn = (int *)(addr + SHM_OFFSET_FILE_LENS);
        char list[FILE_LIST_ROWS][FILE_LIST_COLUMN];
        memcpy(list, addr + SHM_OFFSET_MP3_PATHS, FILE_LIST_MEM_LENS);
        int *curr = (int *)(addr + SHM_OFFSET_CURR_PLAYING);
        for(int i = 0; i < *fn; i++){
            if(i == *curr){
                printf("\e[33m%-4d\e[1;32;44m%s\e[0m\n", i, list[i]);
            }else{
                printf("\e[33m%-4d\e[32m%s\e[0m\n", i, list[i]);
            }
        }
        if (close(shm_fd) == -1) {
            perror("close fd");
        }
    }
    return;

}

void mp3_pause(){
    int shm_fd = shm_open("spiritify", O_RDWR, 0700); 
    if(shm_fd == -1){
        printf("service not launched\n");
    }else{
        addr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        int *pid = (int *)addr;
        if(*pid > 1000){
            printf("pause acition : service pid is %d \n", *pid);
            if(kill(*pid, 0) != 0){
                printf("yes, it's not running, will unlink shm\n");
                shm_unlink("spiritify");
                if (close(shm_fd) == -1) {
                    perror("close fd");
                } 
                return;
            }

            int *action = (int *)(addr + SHM_OFFSET_ACTION);
            *action = PAUSE;
            char buf[20];
            sprintf(buf, "kill -10 %d",*pid);
            system(buf);
        }else{
            printf("pid < 1000? \n");
        }

        if (close(shm_fd) == -1) {
            perror("close fd");
        }
    }
    return;

}

void mp3_pre(){
    int shm_fd = shm_open("spiritify", O_RDWR, 0700); 
    if(shm_fd == -1){
        printf("service not launched\n");
    }else{
        addr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        int *pid = (int *)addr;
        if(*pid > 1000){
            printf("pre acition : service pid is %d \n", *pid);
            if(kill(*pid, 0) != 0){
                printf("yes, it's not running, will unlink shm\n");
                if (close(shm_fd) == -1) {
                    perror("close fd");
                } 
                shm_unlink("spiritify");
                return;
            }

            int *action = (int *)(addr + SHM_OFFSET_ACTION);
            *action = PRE;
            char buf[20];
            sprintf(buf, "kill -10 %d",*pid);
            system(buf);
        }else{
            printf("pid < 1000? \n");
        }

        if (close(shm_fd) == -1) {
            perror("close fd");
        }
    }
    return;
}

void mp3_post(){
    int shm_fd = shm_open("spiritify", O_RDWR, 0700); 
    if(shm_fd == -1){
        printf("service not launched\n");
    }else{
        addr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        int *pid = (int *)addr;
        if(*pid > 1000){
            printf("post acition : service pid is %d \n", *pid);
            if(kill(*pid, 0) != 0){
                printf("yes, it's not running, will unlink shm\n");
                if (close(shm_fd) == -1) {
                    perror("close fd");
                } 
                shm_unlink("spiritify");
                return;
            }

            int *action = (int *)(addr + SHM_OFFSET_ACTION);
            *action = POST;
            char buf[20];
            sprintf(buf, "kill -10 %d",*pid);
            system(buf);
        }else{
            printf("pid < 1000? \n");
        }

        if (close(shm_fd) == -1) {
            perror("close fd");
        }
    }
    return;
}

void mp3_stop(){
    int shm_fd = shm_open("spiritify", O_RDWR, 0700); 
    if(shm_fd == -1){
        printf("service not launched\n");
    }else{
        addr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        int *pid = (int *)addr;
        if(*pid > 1000){
            printf("stop acition : service pid is %d \n", *pid);
            if(kill(*pid, 0) != 0){
                printf("yes, it's not running, will unlink shm\n");
                if (close(shm_fd) == -1) {
                    perror("close fd");
                } 
                shm_unlink("spiritify");
                return;
            }

            int *action = (int *)(addr + SHM_OFFSET_ACTION);
            *action = STOP;
            char buf[20];
            sprintf(buf, "kill -10 %d",*pid);
            system(buf);
        }else{
            printf("pid < 1000? \n");
        }

        if (close(shm_fd) == -1) {
            perror("close fd");
        }
    }
    return;

}

void set_vol(int vol_num){
    int shm_fd = shm_open("spiritify", O_RDWR, 0700); 
    if(shm_fd == -1){
        printf("service not launched\n");
    }else{
        addr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        int *pid = (int *)addr;
        if(*pid > 1000){
            printf("set volume acition : service pid is %d \n", *pid);
            if(kill(*pid, 0) != 0){
                printf("yes, it's not running, will unlink shm\n");
                if (close(shm_fd) == -1) {
                    perror("close fd");
                } 
                shm_unlink("spiritify");
                return;
            }

            int *action = (int *)(addr + SHM_OFFSET_ACTION);
            *action = VOL;
            int *set_num = (int *)(addr + SHM_OFFSET_DATA_SEND);
            *set_num = vol_num;
            char buf[20];
            sprintf(buf, "kill -10 %d",*pid);
            system(buf);
        }else{
            printf("pid < 1000? \n");
        }

        if (close(shm_fd) == -1) {
            perror("close fd");
        }
    }
    return;
}

void mp3_jump(int num){
    int shm_fd = shm_open("spiritify", O_RDWR, 0700); 
    if(shm_fd == -1){
        printf("service not launched\n");
    }else{
        addr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        int *pid = (int *)addr;
        if(*pid > 1000){
            printf("jump acition : service pid is %d \n", *pid);
            if(kill(*pid, 0) != 0){
                printf("yes, it's not running, will unlink shm\n");
                if (close(shm_fd) == -1) {
                    perror("close fd");
                } 
                shm_unlink("spiritify");
                return;
            }

            int *action = (int *)(addr + SHM_OFFSET_ACTION);
            *action = JUMP;
            int *set_num = (int *)(addr + SHM_OFFSET_DATA_SEND);
            *set_num = num;
            char buf[20];
            sprintf(buf, "kill -10 %d",*pid);
            system(buf);
        }else{
            printf("pid < 1000? \n");
        }

        if (close(shm_fd) == -1) {
            perror("close fd");
        }
    }
    return;

}

void cleanup(){
    if(addr != NULL){
        munmap(addr, SHM_SIZE);
    }
    if(shm_unlink("spiritify") == -1){
        printf("\e[33m clean shm file failed \e[0m\n");
    } 
    printf("shm file cleaned up\n");
}

void quit_mix(){
    if(music != NULL){
        Mix_FreeMusic(music);
        music = NULL;
    }
    Mix_CloseAudio();
    SDL_Quit(); 
    printf("exit %d\n", getpid());
    cleanup();
    exit(0);
}

void signal_handler(int sig) {
    printf("Signal received: %d\n", sig);
    if(sig == SIGINT){
        stop = 1;
    }
    if(sig == SIGUSR1){
        int *action = (int *)(addr + SHM_OFFSET_ACTION);
        switch (*action) {
            case JUMP:
                printf("jump to specific index\n");
                if(music != NULL){
                    Mix_FreeMusic(music);
                    music = NULL;
                }
                current_playing = *((int *)(addr + SHM_OFFSET_DATA_SEND));
                play();
                break;
            case PAUSE:
                printf("service will be paused\n");
                if(Mix_PausedMusic()){
                    Mix_ResumeMusic();
                }else {
                    Mix_PauseMusic();
                }
                break;
            case PRE:
                printf("service will play pre music\n");
                if(music != NULL){
                    Mix_FreeMusic(music);
                    music = NULL;
                }
                current_playing--;
                play();
                break;
            case POST:
                printf("service will play post music\n");
                if(music != NULL){
                    Mix_FreeMusic(music);
                    music = NULL;
                }
                current_playing++;
                play();
                break;
            case VOL:
                printf("service will change volume\n");
                Mix_VolumeMusic(*((int *)(addr + SHM_OFFSET_DATA_SEND)));
                break;
            case STOP:
                printf("service will stop\n");
                if(music != NULL){
                    Mix_FreeMusic(music);
                    music = NULL;
                }
                stop = 1;
                break;
            default:
                printf("what action?\n");
        }
    }
}

void play(){
    printf("current playing index: %d\n", current_playing);
    if((current_playing >= 0) && (current_playing < fl.file_nums)){
        music = Mix_LoadMUS(fl.list[current_playing]);
        if(music == NULL){
            fprintf(stderr, "LoadMUS err, %s\n", SDL_GetError());
            quit_mix();
        }
        const char *title;
        title= Mix_GetMusicTitle(music);
        if(strlen(title) < 1){
            title = fl.list[current_playing];
        }
        double duration = Mix_MusicDuration(music);
        printf("title: \e[1;37;42m %s \e[0m\nduration: %d mins %d secs\n", title, (int)duration / 60, (int)duration % 60);
        //Mix_PlayMusic(music, 0);   
        Mix_FadeInMusic(music, 0, 1000);
        if(addr != NULL){
            int *curr_playing_ptr = (int *)(addr + SHM_OFFSET_CURR_PLAYING);    
            *curr_playing_ptr = current_playing;
        }
    }
}

void music_finish(){
    printf("a music finished\n");
    if(music != NULL){
        Mix_FreeMusic(music);
        music = NULL;
    }
    current_playing++;
    play();
}

int load_and_list_files(filelist *fl){
    char *config_home_path;
    config_home_path = getenv("XDG_CONFIG_HOME");
    if(strlen(config_home_path) == 0){
        printf("pls set config_home_path by 'export XDG_CONFIG_HOME=your_config_home_path'\n");
        return -1;
    }
    char *spiritify_config_file_end = "/spiritify/spiritify";
    char *spiritify_config_file;
    spiritify_config_file = strcat(config_home_path, spiritify_config_file_end);
    if(check_file_or_dir_exists(spiritify_config_file) != 1){
        printf("check config exists: pls run spiritify to choose a directory for datasource\n");
        return -1;
    }
    FILE * file = fopen(spiritify_config_file, "r");
    char pathbuf[100];
    memset(pathbuf, 0, sizeof(pathbuf));
    printf("config file path: %s\n", pathbuf);
    size_t bytes_read = fread(pathbuf, 1, sizeof(pathbuf) - 1, file);
    if(bytes_read < 1){
        printf("read config: pls run spiritify to choose a directory for datasource\n");
        return -1;
    }
    memset(fl->list, 0, FILE_LIST_MEM_LENS);

    char *ext_buf[100] = {
        ".mp3",
        ".flac"
    };
    search_file_by_multi_ext(pathbuf, ext_buf, 2, fl); 
    for(int i = 0; i < fl->file_nums; i++){
        printf("\e[33m%-4d\e[32m%s\e[0m\n", i, fl->list[i]);
    }

    printf("before memory opt\n");
    int *file_nums_ptr = (int *)(addr + SHM_OFFSET_FILE_LENS);
    *file_nums_ptr = fl->file_nums;  
    memcpy(addr + SHM_OFFSET_MP3_PATHS, fl->list, FILE_LIST_MEM_LENS);
    printf("after memory opt\n");
    return 0; 
}

int shm_operation(){
    int shm_fd = shm_open("spiritify", O_RDWR, 0700); 
    if(shm_fd == -1){
        printf("there is no shm already exists\n");
        shm_fd = shm_open("spiritify", O_RDWR | O_CREAT, 0700);
        if(shm_fd == -1){
            perror("open/create shm");
            exit(1);
        }
        if(ftruncate(shm_fd, SHM_SIZE) != 0){
            perror("truncate shm");
            exit(1);
        }
    }
    addr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    int *pid = (int *)addr;
    if(*pid != 0){
        printf("is there a instance already run?\n");
        if(kill(*pid, 0) == 0){
            printf("yes, it's already running\n");
            if (close(shm_fd) == -1) {
                perror("close fd");
            } 
            return 1;
        }else{
            printf("yes, it's not running, will unlink shm\n");
            if (close(shm_fd) == -1) {
                perror("close fd");
            } 
            shm_unlink("spiritify");
            return 1;

        }
    }
    *pid = getpid();
    if (close(shm_fd) == -1) {
        perror("close fd");
    }
    return 0;
}


void play_mp3(){
    pid_t pid = fork();
    if(pid != 0){
        printf("spiritify service's pid is \e[034m %d \e[0m\n", pid);    
        return;
    }
    if(shm_operation() == 1){
        return;
    }
    if(!SDL_Init(SDL_INIT_AUDIO)){
        quit_mix();
        return;
    }

    if(!Mix_OpenAudio(0, NULL)){
        fprintf(stderr, "open %s\n", SDL_GetError());
        quit_mix();
        return;
    }
    int load_ret;
    load_ret = load_and_list_files(&fl); 
    if(load_ret != 0 || fl.file_nums < 1){
        quit_mix();
        return;
    }
    play();  
    Mix_HookMusicFinished(music_finish);
    Mix_VolumeMusic(20);
    printf("decoder: %s\n", Mix_GetMusicDecoder(0));
    signal(SIGUSR1, signal_handler);
    signal(SIGINT, signal_handler);
    while (stop == 0) {
        pause();
    }
    printf("final quit\n");
    quit_mix();
}

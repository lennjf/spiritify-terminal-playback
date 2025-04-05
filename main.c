#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <sys/stat.h>
#include <mytool.h>
#include <unistd.h>
#include "SDL_mixer.h"


void config(int *argc, char*** argv);
void play_mp3(int argc, char** argv);

int main(int argc, char **argv){
    if(argc < 2){
        printf("Usage: spiritify <config | play | list | vol | stop>\n");
        return 0;
    }
    if(strcmp(argv[1], "config") == 0){
        config(&argc, &argv);
        return 0;
    }

    if(strcmp(argv[1], "play") == 0){
        play_mp3(argc, argv);
        return 0;
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

void quit_mix(Mix_Music *music){
    if(music != NULL){
        Mix_FreeMusic(music);
    }
    Mix_CloseAudio();
    SDL_Quit(); 
}

void signal_handler(int sig) {
    printf("Signal received: %d\n", sig);
}

void play_mp3(int argc, char** argv){
    
    pid_t pid = fork();
    if(pid != 0){
        printf("spiritify service's pid is %d\n", pid);    
        return;
    }
    Mix_Music* music = NULL;
    if(!SDL_Init(SDL_INIT_AUDIO)){
        quit_mix(NULL);
        return;
    }

    if(!Mix_OpenAudio(0, NULL)){
        fprintf(stderr, "open %s\n", SDL_GetError());
        quit_mix(NULL);
        return;
    }
   
    music = Mix_LoadMUS(argv[2]);
    if(music == NULL){
        fprintf(stderr, "INIT %s\n", SDL_GetError());
        quit_mix(NULL);
    }
    const char *title;
    title= Mix_GetMusicTitle(music);
    printf("title: %s\n duration: %f\n", title, Mix_MusicDuration(music));
    //Mix_PlayMusic(music, 0);   
    Mix_FadeInMusic(music, 0, 1000);
    Mix_VolumeMusic(80);
    printf("decoder: %s\n", Mix_GetMusicDecoder(0));
    signal(SIGUSR1, signal_handler);
    pause();
    FILE *af = fopen("/home/carpenter/tmp/kkk", "w");
    fwrite("after pause", 11, 1, af);
    fclose(af);
    //getchar();
}

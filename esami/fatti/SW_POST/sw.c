/*
Esame di Reti di Calcolatori - 24 Giugno 2021

Si modifichi il programma web server esame.c in modo tale che supporti una tipologia di web application gateway (cgi) differente da quella finora supportata.

Il client dovrà accedere tramite il programma web server alla risorsa http://88.80.187.84:9088/post.html contenente il sorgente html dato.

La pagina consentirà all’utente, per tramite dell’User Agent (browser),
di inserire il nome di un comando di shell UNIX da eseguire (per esempio ls) e uno o due parametri (per esempio -l).

Alla pressione del bottone “Invia”, il browser invierà al Web server una HTTP-request della risorsa /cgi-bin/command con il metodo POST,
contenente nel suo Entity Body il nome del comando e i due parametri secondo il medesimo formato (detto urlencoded) utilizzato nelle query string degli URL (v. RFC 1866 Cap. 8.2).

Il Web Server ricevendo la richiesta alla risorsa /cgi-bin/command dovrà eseguire il comando specificato con i parametri e riportare l’output di quel comando come Entity Body della HTTP-response.

Pseudocodice:

SW cgi:
Cambiato nulla

cgi-bin/command.c:
legge in input una stringa fatta da:
    commando=value&param1=value&param2=value
param1 e param2 possono vuoti, il programma funziona comunque

viene scritto l'intero commando, unendo in un unica stringa command, param1, param2

viene eseguito il comando lanciando system();

viene inserito l'output del comando come entity body della http-response
*/

#include<stdio.h>
#include<stdlib.h>
#include <string.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


struct sockaddr_in local, remote;
char request[100000];
char response[1000];

struct header {
  char * n;
  char * v;
} h[100];

unsigned char  envbuf[1000];
int pid;
int env_i, env_c;
char * env[100];
int new_stdin, new_stdout;
char * myargv[10];

void add_env(char * env_key, char* env_value){
    sprintf(envbuf + env_c, "%s=%s", env_key, env_value);
    env[env_i++] = envbuf+env_c;
    env_c += (strlen(env_value) + strlen(env_key)+2);
    env[env_i] = NULL;
}


int main() {
    char hbuffer[10000];
    char * reqline;
    char * method, *url, *ver;
    char * filename,*content_type;
    char fullname[200];
    FILE * fin;
    int c;
    int n;
    int i,j,t, s,s2;
    int yes = 1;
    int len;
    int length;
    char boundary[100];
    if (-1 == ( s = socket(AF_INET, SOCK_STREAM, 0 ))) {
        printf("errno = %d\n",errno); perror("Socket Fallita"); return -1; 
    }
    
    local.sin_family = AF_INET;
    local.sin_port = htons(9088);
    local.sin_addr.s_addr = 0;

    t = setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int));
    if (t == -1){ perror("setsockopt fallita"); return 1; }

    if (-1 == bind(s, (struct sockaddr *)&local,sizeof(struct sockaddr_in))) { 
        perror("Bind Fallita"); return -1; }

    if (-1 == listen(s,10)) { perror("Listen Fallita"); return -1; }
    
    remote.sin_family = AF_INET;
    remote.sin_port = htons(0);
    remote.sin_addr.s_addr = 0;
    len = sizeof(struct sockaddr_in);
    while (1){
        printf("\n\nAccept\n");
        s2=accept(s,(struct sockaddr *)&remote,&len);
        bzero(hbuffer,10000);
        bzero(h,sizeof(struct header)*100);
        
        reqline = h[0].n = hbuffer;

        for (i=0,j=0; read(s2,hbuffer+i,1); i++) {
            if(hbuffer[i]=='\n' && hbuffer[i-1]=='\r'){
                hbuffer[i-1]=0; // Termino il token attuale
                if (!h[j].n[0])
                    break;
                h[++j].n=hbuffer+i+1;
            }
            if (hbuffer[i]==':' && !h[j].v){
                hbuffer[i]=0;
                h[j].v = hbuffer + i + 2;
            }
        }
        length=0;
        for(i=0; i<j; i++){
            printf("Headers %s : %s\n", h[i].n, h[i].v);
            if (!strcmp(h[i].n, "Content-Length"))
                length = atoi(h[i].v);
        }
        
        len = 1000;
        if(len == -1) {
            perror("Read Fallita"); return -1;
        }
        
        method = reqline;
        add_env("METHOD",method);
        
        for(i=0;i<len && reqline[i]!=' ';i++);
        reqline[i++]=0; 
        url=reqline+i;
        for(;i<len && reqline[i]!=' ';i++);
        reqline[i++]=0; 
        ver=reqline+i;
        for(;i<len && reqline[i]!='\r';i++);
        reqline[i++]=0; 
        
        filename = url+1;

        //CGI
        if (!strncmp(url,"/cgi-bin/",5)) {
            if (!strcmp(method, "GET")) {
                for(i=0;filename[i] && (filename[i]!='?');i++);
                if (filename[i] == '?') { 
                    filename[i]=0;
                    add_env("QUERY_STRING", filename+i+1);
                }
                add_env("CONTENT_LENGTH","0");
            }
            else if(!strcmp(method, "POST")) {
                char tmp[10];
                sprintf(tmp,"%d", length);
                add_env("CONTENT_LENGTH", tmp);
            } else {
                sprintf(response,"HTTP/1.1 501 Not Implemented\r\n\r\n");
                write(s2,response,strlen(response));
                close(s2);
                continue;
            }
            fin=fopen(filename,"rt");
            if (fin == NULL){
                sprintf(response,"HTTP/1.1 404 Not Found\r\n\r\n");
                write(s2,response,strlen(response));
            }
            else{
                sprintf(response,"HTTP/1.1 200 OK\r\n\r\n");
                write(s2,response,strlen(response));
                fclose(fin);
                for(i=0;env[i];i++)
                    printf("environment: %s\n", env[i]);
                sprintf(fullname,"./%s",filename);
                myargv[0]=fullname;
                myargv[1]=NULL;
                
                printf("Executing %s\n",fullname);                
                if(!(pid=fork())){ 
                    dup2(s2,1);
                    dup2(s2,0);
                    if(-1==execve(fullname,myargv,env)) {
                        perror("execve"); exit(1);
                    }
                }
                waitpid(pid,NULL,0);
                printf("Il figlio e' morto...\n");
            }
        }
        //NOT CGI
        else if (!strcmp(method,"GET")){ 
            filename = url+1;
            printf("filename: %s\n",filename);
            fin=fopen(filename,"rt");
            if (fin == NULL){
                sprintf(response,"HTTP/1.1 404 Not Found\r\n\r\n");
                write(s2,response,strlen(response));
            } else {
                sprintf(response,"HTTP/1.1 200 OK\r\n\r\n");
                write(s2,response,strlen(response));
                while ( (c = fgetc(fin))!=EOF)
                    write(s2,&c,1);
                fclose(fin);
                }
        } else {
            sprintf(response,"HTTP/1.1 501 Not Implemented\r\n\r\n");
            write(s2,response,strlen(response));
        }
        close(s2);
        env_c=env_i=0;
    }
    close(s);
}
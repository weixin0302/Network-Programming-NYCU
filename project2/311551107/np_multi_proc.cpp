#include "part3_function.h"

int main(int argc, char* argv[])
{
    int sockfd, childpid;
    int flag=1;
    struct sockaddr_in serv_addr, cli_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[1]));
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));

    bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
    listen(sockfd, LISTENQ);
    // signal(SIGCHLD, reaper);
    
    setbuf(stdout, NULL);

    
    // msg_sem = sem_open(MSGSEM, O_CREAT | O_EXCL, PERMS, 1);
    // sem_init(msg_sem , 0, 1);
    // sem_init(&msg_sem, 0, 1);

    create_shm_tables();

    attach_shm_tables();

    for(int i=1; i<31; i++){
        sem_init(&shmclientTable[i].msg_sem, 1, 1);
    }
    
    while(1)
    {
        
        signal(SIGUSR1, sig_usr);
        signal(SIGUSR2, sig_usr);
        signal(SIGINT, sig_usr);
        signal(SIGCHLD, SIG_IGN); 
        socklen_t clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);
        
        childpid = fork();
        if(childpid==0)
        {
           
            // msg_sem = sem_open(MSGSEM, O_RDWR);
            // cout << newsockfd;

            
            close(sockfd);
            
            int i;
            for(i=1; i<31; i++){
                if(shmidTable[i] == 0)
                {
                    shmidTable[i] = 1;
                    break;
                }
            }
            my_id = i;
            char addr[20];
            inet_ntop(AF_INET, &cli_addr.sin_addr.s_addr, addr, sizeof(cli_addr));
            string default_name = "(no name)";

            sem_wait(&shmclientTable[i].msg_sem);
            strcpy(shmclientTable[i].name, default_name.c_str());
            shmclientTable[i].socket_fd = newsockfd;
            shmclientTable[i].pid = getpid();
            shmclientTable[i].port = ntohs(cli_addr.sin_port);
            strcpy(shmclientTable[i].ip, addr);
            shmclientTable[i].id = i;
            sem_post(&shmclientTable[i].msg_sem);

            
            int val;
            sem_getvalue(&shmclientTable[i].msg_sem, &val);
            cout << "i am " << my_id << ", my sem is " << val << endl;
            
            string name = shmclientTable[i].name;
            string ip = shmclientTable[i].ip;
            string port = to_string(shmclientTable[i].port);
            string msg = "*** User '" + name + "' entered from " + ip + ":" + port + ". ***\n";
            for(int j=1; j<31; j++){      //broadcast login message
                if(j==i){
                    string newmsg = welcome_message + msg + "% ";
                    write(shmclientTable[j].socket_fd, newmsg.c_str(), newmsg.length());
                }
                else
                {
                    if(shmidTable[j]>0)
                    {
                        sem_wait(&shmclientTable[j].msg_sem);
                        strcpy(shmclientTable[j].msgbuffer, msg.c_str());
                        kill(shmclientTable[j].pid, SIGUSR1);
                        // sem_post(&shmclientTable[j].msg_sem);
                    }
                }
            }
            cout << "*** User '" << name  << "' entered from " << ip << ":" << port << ". ***" << endl;

            vector<string> parse;
            setenv("PATH", "bin:.", 1);
            char input[MAXLEN] = "";
            signal(SIGCHLD, SIG_IGN);
            
            while(read(newsockfd, input, sizeof(input)))
            {
                string line = string(input);
                for(int j=0; j<line.length(); j++){
                    if(line[j] == '\r' || line[j] == '\n'){
                        line.erase(line.begin()+j);
                        j --;
                    }
                }
                cout << i <<  " " << line << endl;
                parse = string_split(line, WHITE_SPACE);
                if(parse[0]=="exit")
                {
                    string name = shmclientTable[i].name;
                    string msg = "*** User '" + name  + "' left. ***\n";
                    
                    for(int j=1; j<31; j++)
                    {
                        if(shmidTable[j]>0 && j !=i)
                        {
                            sem_wait(&shmclientTable[j].msg_sem);
                            strcpy(shmclientTable[j].msgbuffer, msg.c_str());
                            kill(shmclientTable[j].pid, SIGUSR1);
                            // sem_post(&shmclientTable[j].msg_sem);
                        }
                    }
                    sem_wait(&shmclientTable[i].msg_sem);
                    close(shmclientTable[i].socket_fd);
                    shmidTable[i] = 0;
                    sem_post(&shmclientTable[i].msg_sem);

                    for(int j = 1; j < 31; j++){
                        if(shmclientTable[i].recv_from[j] == 1)
                        {
                            sem_wait(&shmclientTable[i].msg_sem);
                            shmclientTable[i].recv_from[j] = 0;
                            sem_post(&shmclientTable[i].msg_sem);
                        }
                    }
                    exit(0);
                }
                if(decided_built_in_function(parse, shmclientTable[i].socket_fd, i, line))
                {
                    parse_preprocess(parse, shmclientTable[i].socket_fd, i, line);
                }
                memset(input, 0, sizeof(input));
                write(shmclientTable[i].socket_fd, "% ", strlen("% "));
            }
        }
        else close(newsockfd);
    }
    close(sockfd);
    return 0;
}
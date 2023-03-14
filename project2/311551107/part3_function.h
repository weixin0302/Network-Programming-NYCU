#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <grp.h>
#include <iostream>
#include <map>
#include <pwd.h>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <unordered_map>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <semaphore.h>

using namespace std;

#define MAXLEN 15001
#define MAXPIPE 1001
#define LISTENQ 31

#define PERMS 0666

#define welcome_message "****************************************\n** Welcome to the information server. **\n****************************************\n"

#define CLIENTTABLE_SHMKEY ((key_t) 7671)
#define IDTABLE_SHMKEY ((key_t) 7672) 

#define MSGSEM "/msgsemmm"

int now = 0;
int wait_childid;
int my_id;

vector<int *> allpipe;
unordered_map<int, int *> allnumpipe;

struct client_shm{
    int id;
    int pid;
    char name[50];
    char ip[50];
    int port;
    int socket_fd;
    sem_t msg_sem;
    char msgbuffer[1025];
    int recv_from[31];
    int sent_to_me;
};

int *shmidTable;
int shmidTableid;

client_shm *shmclientTable;
int shmclientTableid;

int newsockfd;

const string WHITE_SPACE = " \t\r\n";

bool is_white_space(char ch) { return WHITE_SPACE.find(ch) != -1; }

vector<string> string_split(const string &s, const string &delims)
{
    vector<string> vec;
    int p = 0, q;
    while ((q = s.find_first_of(delims, p)) != string::npos)
    {
        if (q > p)
            vec.push_back(s.substr(p, q - p));
        p = q + 1;
    }
    if (p < s.length())
        vec.push_back(s.substr(p));
    return vec;
}

void reaper(int signal_number)
{
    union wait status;
    while(wait3(&status, WNOHANG, (struct rusage *)0)>=0) {}
}

void create_shm_tables(){
    shmidTableid = shmget(IDTABLE_SHMKEY, sizeof(int)*31, PERMS|IPC_CREAT);
    shmidTable = (int*)shmat(shmidTableid, (char *)0, 0);
    for(int i = 0; i < 31; i++){
        shmidTable[i] = 0;
    }
    shmdt(shmidTable);

    shmclientTableid = shmget(CLIENTTABLE_SHMKEY, sizeof(client_shm)*31, PERMS|IPC_CREAT);

}

void attach_shm_tables(){
    shmclientTable = (client_shm*)shmat(shmclientTableid, (char *)0, 0);
    
    shmidTable = (int*)shmat(shmidTableid, (char *)0, 0);
}

void detach_shm_tables(){
    shmdt(shmidTable);
    shmdt(shmclientTable);
}

void sig_usr(int signo)
{
    if(signo == SIGUSR1){
        cout << my_id << " " << shmclientTable[my_id].msgbuffer << endl;
        write(shmclientTable[my_id].socket_fd, shmclientTable[my_id].msgbuffer, strlen(shmclientTable[my_id].msgbuffer));
        sem_post(&shmclientTable[my_id].msg_sem);
        int val;
        sem_getvalue(&shmclientTable[my_id].msg_sem, &val);
        cout << "i am " << my_id << ", after kill, my sem is " << val<< endl;
    }
    else if(signo == SIGUSR2){
        string filename = "user_pipe/";
        filename = filename + to_string(shmclientTable[my_id].sent_to_me) + "_" + to_string(my_id) +  ".txt";
        mkfifo(filename.c_str(), 0666);
        sem_post(&shmclientTable[my_id].msg_sem);
    }
    else if(signo == SIGINT)
    {
        detach_shm_tables();
        for(int i=1; i<31; i++){
            sem_close(&shmclientTable[i].msg_sem);
        }
        shmctl(shmclientTableid, IPC_RMID, NULL);
        shmctl(shmidTableid, IPC_RMID, NULL);
        exit(0);
    }
}

int decide_function(vector<string> parse, int redirect, int isnumpipe, int nextpipe, int isuserpipein, int isuserpipeout, int prepipe, int *pipfd, int clientsock, int id)
{
    // cout << parse[0] << " " << isuserpipeout;
    int parse_num = parse.size();
    int wait_status;
    char *argv[parse_num + 1];
    int readfd, writefd;

    if (nextpipe)
    {
        allpipe.push_back(pipfd);
    }
    if (isnumpipe && allnumpipe.find((now + abs(isnumpipe)) % MAXPIPE) == allnumpipe.end())
    {
        allnumpipe[(now + abs(isnumpipe)) % MAXPIPE] = pipfd;
    }

    for (int i = 0; i < parse_num; i++)
    {
        char tmp[MAXLEN];
        strcpy(tmp, parse[i].c_str());
        argv[i] = strdup(tmp);
    }
    argv[parse_num] = NULL;

    pid_t pid = fork();
    while (pid < 0)
    {
        wait(&wait_status);
        pid = fork();
    }

    if (pid == 0)
    {
        dup2(clientsock, 1);
        dup2(clientsock, 2);
        if (prepipe)
        {
            close(allpipe[0][1]);
            dup2(allpipe[0][0], 0);
            close(allpipe[0][0]);
            allpipe.erase(allpipe.begin());
        }
        if (nextpipe)
        {
            close(allpipe[0][0]);
            dup2(allpipe[0][1], 1);
            close(allpipe[0][1]);
        }

        int target = (now + abs(isnumpipe)) % MAXPIPE;
        if (allnumpipe.find(now) != allnumpipe.end())   //numpipe receive
        {

            close(allnumpipe[now][1]);
            dup2(allnumpipe[now][0], 0);
            close(allnumpipe[now][0]);
            allnumpipe.erase(now);
        }
        if (isnumpipe) //numpipe send
        {
            if (isnumpipe < 0)
                dup2(allnumpipe[target][1], 2);
            close(allnumpipe[target][0]);
            dup2(allnumpipe[target][1], 1);
            close(allnumpipe[target][1]);
        }

        if(isuserpipein == -1)
        {
            FILE *fp = fopen("/dev/null","r");
            int readfd = fileno(fp);
            dup2(readfd, 0);
            close(readfd);
        }

        else if(isuserpipein > 0)
        {
            string filename = "user_pipe/";
            filename = filename + to_string(isuserpipein) + "_" + to_string(id) +  ".txt";

            sem_wait(&shmclientTable[id].msg_sem);
            shmclientTable[id].recv_from[isuserpipein] = 0;
            sem_post(&shmclientTable[id].msg_sem);
            FILE *fp = fopen(filename.c_str(),"r");
            int readfd = fileno(fp);
            dup2(readfd, 0);
            close(readfd);
        }
        
        if(isuserpipeout == -1)
        {
            FILE *fp = fopen("/dev/null","w");
            int writefd = fileno(fp);
            dup2(writefd, 1);
            close(writefd);
        }

        else if(isuserpipeout > 0)
        {
            string filename = "user_pipe/";
            filename = filename + to_string(id) + "_" + to_string(isuserpipeout) +  ".txt";
            FILE *fp = fopen(filename.c_str(),"w");
            int writefd = fileno(fp);
            dup2(writefd, 1);
            close(writefd);
        }

        if (redirect)
            dup2(redirect, 1);
        int res = execvp(argv[0], argv);
        if (res != 0)
            cerr << "Unknown command: [" << argv[0] << "]." << endl;
        exit(0);
    }

    else
    {
        if (prepipe)
        {
            close(allpipe[0][0]);
            close(allpipe[0][1]);
            allpipe.erase(allpipe.begin());
        }

        if (allnumpipe.find(now) != allnumpipe.end())
        {
            close(allnumpipe[now][0]);
            close(allnumpipe[now][1]);
            allnumpipe.erase(now);
        }

        if (!nextpipe && !isnumpipe && !(isuserpipeout >0))
            waitpid(pid, &wait_status, 0);
    }
    return 0;
}

int decided_built_in_function(vector<string> parse, int clientsock, int id, string whole_command)
{
    if (parse[0] == "printenv")
    {
        char env[MAXLEN];
        strcpy(env, parse[1].c_str());
        if (!getenv(env))
            return 0;
        string msg = getenv(env);
        msg += "\n";
        write(clientsock, msg.c_str(), msg.length());        
        return 0;
    }
    else if (parse[0] == "setenv")
    {
        char env[MAXLEN], envarg[MAXLEN];
        strcpy(env, parse[1].c_str());
        strcpy(envarg, parse[2].c_str());
        setenv(env, envarg, 1);
        return 0;
    }

    else if(parse[0] == "who")
    {
        string msg = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
        for(int i=1; i<31; i++)
        {
            if(shmidTable[i] > 0)
            {
                string name = shmclientTable[i].name;
                string ip = shmclientTable[i].ip;
                string port = to_string(shmclientTable[i].port);
                msg = msg + to_string(i) + "\t" + name + "\t" + ip + ":" + port;
                if(i == id) msg = msg + "\t<-me";
                msg += "\n";
            }
        }
        write(clientsock, msg.c_str(), msg.length());
        return 0;
    }

    else if(parse[0] == "tell")
    {
        int tell_id = stoi(parse[1]);
        if(shmidTable[tell_id] == 0)
        {
            string msg = "*** Error: user #" + to_string(tell_id) + " does not exist yet. ***\n";
            write(clientsock, msg.c_str(), msg.length());
        }
        else
        {
            string name = shmclientTable[id].name;
            string tell_msg = whole_command.substr(7, whole_command.size()-7);

            string msg = "*** " + name + " told you ***: " + tell_msg + "\n";
            sem_wait(&shmclientTable[tell_id].msg_sem);
            strcpy(shmclientTable[tell_id].msgbuffer, msg.c_str());
            kill(shmclientTable[tell_id].pid, SIGUSR1);
            // sem_post(&shmclientTable[tell_id].msg_sem);
        }
        return 0;
    }

    else if(parse[0] == "yell")
    {
        string yell_msg = whole_command.substr(5, whole_command.size()-5);
        string name = shmclientTable[id].name;
        string msg = "*** " + name + " yelled ***: " + yell_msg + "\n";
        for(int i=1; i<31; i++)
        {
            if(shmidTable[i] > 0)
            {
                sem_wait(&shmclientTable[i].msg_sem);
                strcpy(shmclientTable[i].msgbuffer, msg.c_str());
                kill(shmclientTable[i].pid, SIGUSR1);
                // sem_post(&shmclientTable[i].msg_sem);
            }
        }
        
        return 0;
    }

    else if(parse[0] == "name")
    {
        for(int i=1; i<31; i++)
        {
            if(shmidTable[i] > 0 && shmclientTable[i].name == parse[1])
            {
                string msg = "*** User '" + parse[1] + "' already exists. ***\n";
                write(clientsock, msg.c_str(), msg.length());
                return 0;
            }
        }
        sem_wait(&shmclientTable[id].msg_sem);
        strcpy(shmclientTable[id].name, parse[1].c_str());
        sem_post(&shmclientTable[id].msg_sem);

        string ip = shmclientTable[id].ip;
        string port = to_string(shmclientTable[id].port);
        string msg = "*** User from " + ip + ":" + port + " is named '" + parse[1] + "'. ***\n";
        for(int i=1; i<31; i++)
        {
            if(shmidTable[i] > 0)
            {
                sem_wait(&shmclientTable[i].msg_sem);
                strcpy(shmclientTable[i].msgbuffer, msg.c_str());
                kill(shmclientTable[i].pid, SIGUSR1);
                // sem_post(&shmclientTable[i].msg_sem);
            }
        }
        return 0;
    }
    return 1;
}


void parse_preprocess(vector<string> parse, int clientsock, int id, string whole_command)
{
    signal(SIGCHLD, SIG_IGN);
    char redir[MAXLEN];
    vector<pair<int, int>> parse_userpipe;
    parse_userpipe.push_back(make_pair(0,0));
    int parse_num = parse.size();
    int redirect = 0;  // store redirect fd
    int isnumpipe = 0; // store target line
    int nextpipe = 0;  // has next pipe or not
    int prepipe = 0;   // has pre pipe or not
    int isuserpipein = 0; // has userpipe in 
    int isuserpipeout = 0; // has userpipe out
    int pipfd[2];
    int wait_status;
    int exit = 0;
    FILE *file;
    int start = 0;
    int command_now = 0;
    int ininin = 0;
    for(int i = 0; i < parse_num; i++)
    {
        if(parse[i] == ">" || parse[i] == "|" || ((parse[i][0] == '|' || parse[i][0] == '!') && parse[i].size() > 1))
        {
            command_now += 1;
            parse_userpipe.push_back(make_pair(0,0));
        }
        if(parse[i][0] == '<' && parse[i].size() > 1)
        {
            int find = 0;
            int isuserpipe = stoi(parse[i].substr(1, parse[i].size() - 1));
            if(shmidTable[isuserpipe] <= 0)
            {
                string msg = "*** Error: user #" + to_string(isuserpipe) + " does not exist yet. ***\n";
                write(clientsock, msg.c_str(), msg.length());
                parse_userpipe[command_now].first = -1;
            }
            
            else
            {
                if(shmclientTable[id].recv_from[isuserpipe] != 1)
                {
                    string msg = "*** Error: the pipe #" + to_string(isuserpipe) + "->#" + to_string(id) + " does not exist yet. ***\n";
                    write(clientsock, msg.c_str(), msg.length());
                    parse_userpipe[command_now].first = -1;
                }
                else
                {
                    string sender_name = shmclientTable[isuserpipe].name;
                    string recv_name = shmclientTable[id].name;
                    parse_userpipe[command_now].first = isuserpipe;
                    string msg = "*** " + recv_name  + " (#" + to_string(id) + ") just received from " + sender_name + " (#" + to_string(isuserpipe) + ") by '" + whole_command + "' ***\n";
                    for(int j=1; j<31; j++)
                    {
                        if(shmidTable[j] > 0)
                        {
                            int val;
                            sem_getvalue(&shmclientTable[j].msg_sem, &val);
                            cout << "i am " << j << ", before in, my sem is " << val << endl;
                            sem_wait(&shmclientTable[j].msg_sem);
                            sem_getvalue(&shmclientTable[j].msg_sem, &val);
                            cout << "i am " << j << ", after in, my sem is " << val << endl;
                            // cout << "ininininin" <<endl;
                            ininin = 1;
                            strcpy(shmclientTable[j].msgbuffer, msg.c_str());
                            cout << "i am " << j << ", my msgbuffer is " << shmclientTable[j].msgbuffer << endl;
                            kill(shmclientTable[j].pid, SIGUSR1);
                            
                        }
                    }
                }
            }
            parse.erase(parse.begin()+i);
            parse_num --;
            i --;
        }
    }
    command_now = 0;
    for(int i = 0; i < parse_num; i++)
    {
        if(parse[i] == ">" || parse[i] == "|" || ((parse[i][0] == '|' || parse[i][0] == '!') && parse[i].size() > 1))
        {
            command_now += 1;
        }
        if(parse[i][0] == '>' && parse[i].size() > 1)
        {
            int find = 0;
            int isuserpipe = stoi(parse[i].substr(1, parse[i].size() - 1));
            if(shmidTable[isuserpipe] <= 0)
            {
                string msg = "*** Error: user #" + to_string(isuserpipe) + " does not exist yet. ***\n";
                write(clientsock, msg.c_str(), msg.length());
                parse_userpipe[command_now].second = -1;
            }
            else
            {
                if(shmclientTable[isuserpipe].recv_from[id] == 1)
                {
                    string msg = "*** Error: the pipe #" + to_string(id) + "->#" + to_string(isuserpipe) + " already exists. ***\n";
                    write(clientsock, msg.c_str(), msg.length());
                    parse_userpipe[command_now].second = -1;
                }
                else
                {
                    parse_userpipe[command_now].second = isuserpipe;

                    sem_wait(&shmclientTable[isuserpipe].msg_sem);
                    shmclientTable[isuserpipe].sent_to_me = id;
                    shmclientTable[isuserpipe].recv_from[id] = 1;
                    string filename = "user_pipe/";
                    filename = filename + to_string(id) + "_" + to_string(isuserpipe) +  ".txt";
                    mkfifo(filename.c_str(), 0666);
                    kill(shmclientTable[isuserpipe].pid, SIGUSR2);
                    

                    string sender_name = shmclientTable[id].name;
                    string recv_name = shmclientTable[isuserpipe].name;
                    string msg = "*** " + sender_name + " (#" + to_string(id) + ") just piped '" + whole_command + "' to " + recv_name + " (#" + to_string(isuserpipe) + ") ***\n";
                    
                    for(int j=1; j<31; j++)
                    {
                        if(shmidTable[j] > 0)
                        {
                            int val; 
                            sem_getvalue(&shmclientTable[j].msg_sem, &val);
                            cout << "i am " << j << ", before out, my sem is " << val << " ininin " << ininin<< endl;
                            sem_wait(&shmclientTable[j].msg_sem);
                            sem_getvalue(&shmclientTable[j].msg_sem, &val);
                            cout << "i am " << j << ", after out, my sem is " << val << " ininin " << ininin<< endl;
                            // cout << "outoutoutoutout" <<endl;
                            strcpy(shmclientTable[j].msgbuffer, msg.c_str());
                            kill(shmclientTable[j].pid, SIGUSR1);
                            // sem_post(&shmclientTable[j].msg_sem);
                        }
                    }
                }
            }
            parse.erase(parse.begin()+i);
            parse_num --;
            i --;
        }
    }
    command_now = 0;
    for (int i = 0; i < parse_num; i++)
    {
        if (parse[i] == ">" && !redirect) // redirect
        {
            string str = parse[i + 1];
            strcpy(redir, str.c_str());
            file = fopen(redir, "w");
            redirect = fileno(file);
        }
        if (redirect)
        {
            parse.pop_back();
        }

        if (parse[i] == "|") // pipe
        {
            int *pipfd = new int[2];
            pipe(pipfd);
            nextpipe = 1;
            vector<string> command;
            for (int j = start; j < i; j++)
            {
                command.push_back(parse[j]);
            }
            start = i + 1;
            isuserpipein = parse_userpipe[command_now].first;
            isuserpipeout = parse_userpipe[command_now].second;
            decide_function(command, redirect, isnumpipe, nextpipe, isuserpipein, isuserpipeout, prepipe, pipfd, clientsock, id);
            nextpipe = 0;
            isuserpipein = 0;
            isuserpipeout = 0;
            prepipe = 1;
            command_now += 1;
        }

        if ((parse[i][0] == '|' || parse[i][0] == '!') && parse[i].size() > 1) // number pipe
        {
            int t = 0;
            vector<string> nums;
            if (parse[i][0] == '|')
                isnumpipe = 1;
            else
                isnumpipe = -1;
            string target_line = parse[i].substr(1, parse[i].size() - 1);
            nums = string_split(target_line, "+");
            for (int i = 0; i < nums.size(); i++)
            {
                t += stoi(nums[i]);
            }
            isnumpipe = isnumpipe * t;
            int *pipfd = new int[2];
            pipe(pipfd);
            vector<string> command;
            for (int j = start; j < i; j++)
            {
                command.push_back(parse[j]);
            }
            start = i + 1;
            isuserpipein = parse_userpipe[command_now].first;
            isuserpipeout = parse_userpipe[command_now].second;
            decide_function(command, redirect, isnumpipe, nextpipe, isuserpipein, isuserpipeout, prepipe, pipfd, clientsock, id);
            isnumpipe = 0;
            isuserpipein = 0;
            isuserpipeout = 0;
            now = (now + 1) % MAXPIPE;
            command_now += 1;
        }
    }

    vector<string> command;
    for (int j = start; j < parse.size(); j++)
    {
        command.push_back(parse[j]);
    }
    if (command.size() > 0)
    {
        isuserpipein = parse_userpipe[command_now].first;
        isuserpipeout = parse_userpipe[command_now].second;
        decide_function(command, redirect, isnumpipe, nextpipe, isuserpipein, isuserpipeout, prepipe, pipfd, clientsock, id);
        now = (now + 1) % MAXPIPE;
    }
    if (redirect)
        fclose(file);
}

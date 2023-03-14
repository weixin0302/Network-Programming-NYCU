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

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

using namespace std;

#define MAXLEN 15001
#define MAXPIPE 1001
#define LISTENQ 31

#define welcome_message "****************************************\n** Welcome to the information server. **\n****************************************\n"

int now = 0;
int wait_childid;

fd_set rfds, afds;

map<int, vector<int *>> allpipe;
unordered_map<int, map<int, int *>> allnumpipe;
unordered_map<int, map<int, int *> > alluserpipe;

vector<int> client(LISTENQ, -1);
vector<map<string, string>> env(LISTENQ);
vector<string> name(LISTENQ, "(no name)");
vector<string> address(LISTENQ, "");
vector<int> port(LISTENQ, 0);
vector<int> process_line(LISTENQ, 0);

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

int decide_function(vector<string> parse, int redirect, int isnumpipe, int nextpipe, int isuserpipein, int isuserpipeout, int prepipe, int *pipfd, int clientsock, int id)
{
    int parse_num = parse.size();
    int wait_status;
    char *argv[parse_num + 1];

    if (nextpipe)
    {
        allpipe[id].push_back(pipfd);
    }
    if (isnumpipe && allnumpipe[id].find((process_line[id] + abs(isnumpipe)) % MAXPIPE) == allnumpipe[id].end())
    {
        allnumpipe[id][(process_line[id] + abs(isnumpipe)) % MAXPIPE] = pipfd;
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
            close(allpipe[id][0][1]);
            dup2(allpipe[id][0][0], 0);
            close(allpipe[id][0][0]);
            allpipe[id].erase(allpipe[id].begin());
        }
        if (nextpipe)
        {
            close(allpipe[id][0][0]);
            dup2(allpipe[id][0][1], 1);
            close(allpipe[id][0][1]);
        }

        int target = (process_line[id] + abs(isnumpipe)) % MAXPIPE;
        if (allnumpipe[id].find(process_line[id]) != allnumpipe[id].end())   //numpipe receive
        {

            close(allnumpipe[id][process_line[id]][1]);
            dup2(allnumpipe[id][process_line[id]][0], 0);
            close(allnumpipe[id][process_line[id]][0]);
            allnumpipe[id].erase(process_line[id]);
        }
        if (isnumpipe) //numpipe send
        {
            if (isnumpipe < 0)
                dup2(allnumpipe[id][target][1], 2);
            close(allnumpipe[id][target][0]);
            dup2(allnumpipe[id][target][1], 1);
            close(allnumpipe[id][target][1]);
        }

        if(isuserpipein < 0)
        {
            FILE *fp = fopen("/dev/null","r");
            int readfd = fileno(fp);
            dup2(readfd, 0);
            close(readfd);
        }
    
        else if(isuserpipein > 0)
        {
            close(alluserpipe[id][isuserpipein][1]);
            dup2(alluserpipe[id][isuserpipein][0], 0);
            close(alluserpipe[id][isuserpipein][0]);
            alluserpipe[id].erase(isuserpipein);

        }
        if(isuserpipeout < 0)
        {
            FILE *fp = fopen("/dev/null","w");
            int writefd = fileno(fp);
            dup2(writefd, 1);
            close(writefd);
        }
        else if(isuserpipeout > 0)
        {
            close(alluserpipe[isuserpipeout][id][0]);
            dup2(alluserpipe[isuserpipeout][id][1], 1);
            close(alluserpipe[isuserpipeout][id][1]);
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
            close(allpipe[id][0][0]);
            close(allpipe[id][0][1]);
            allpipe[id].erase(allpipe[id].begin());
        }

        if (allnumpipe[id].find(process_line[id]) != allnumpipe[id].end())
        {
            close(allnumpipe[id][process_line[id]][0]);
            close(allnumpipe[id][process_line[id]][1]);
            allnumpipe[id].erase(process_line[id]);
        }

        if(isuserpipein > 0)
        {
            close(alluserpipe[id][isuserpipein][0]);
            close(alluserpipe[id][isuserpipein][1]);
            alluserpipe[id].erase(isuserpipein);
        }

        if (!nextpipe && !isnumpipe && !(isuserpipeout > 0))
            waitpid(pid, &wait_status, 0);
    }
    return 0;
}

int decided_built_in_function(vector<string> parse, int clientsock, int id, string whole_command)
{
    if (parse[0] == "exit"){
        string msg = "*** User '" + name[id]  + "' left. ***\n";
        for(int i=1; i<LISTENQ; i++)
        {
            if(client[i] > 0 && i !=id) 
            {
                write(client[i], msg.c_str(), msg.length());
            }
        }
        allpipe.erase(id);
        allnumpipe.erase(id);
        alluserpipe.erase(id);
        client[id] = -1;
        name[id] = "(no name)";
        address[id] = "";
        port[id] = 0;
        process_line[id] = 0;
        env[id].clear();
        env[id]["PATH"] = "bin:.";
        close(clientsock);
        FD_CLR(clientsock, &afds);
        return 0;
    }  

    else if (parse[0] == "printenv")
    {
        char enviroment[MAXLEN];
        strcpy(enviroment, parse[1].c_str());
        if (!getenv(enviroment))
            return 0;
        string msg = env[id][parse[1]] + "\n";
        write(clientsock, msg.c_str(), msg.length());
        return 0;
    }
    else if (parse[0] == "setenv")
    {
        env[id][parse[1]] = parse[2];
        return 0;
    }

    else if(parse[0] == "who")
    {
        string msg = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
        for(int i=1; i<LISTENQ; i++)
        {
            if(client[i] > 0)
            {
                msg = msg + to_string(i) + "\t" + name[i] + "\t" + address[i] + ":" + to_string(port[i]);
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
        if(client[tell_id] < 0)
        {
            string msg = "*** Error: user #" + to_string(tell_id) + " does not exist yet. ***\n";
            write(clientsock, msg.c_str(), msg.length());
        }
        else
        {
            string tell_msg = "";
            for(int i=2; i<parse.size()-1; i++)
            {
                tell_msg = tell_msg + parse[i] + " ";
            }
            tell_msg = tell_msg + parse[parse.size()-1] + "\n";

            string msg = "*** " + name[id] + " told you ***: " + tell_msg;
            write(client[tell_id], msg.c_str(), msg.length());
        }
        return 0;
    }

    else if(parse[0] == "yell")
    {
        string yell_msg = "";
        for(int i=1; i<parse.size()-1; i++)
        {
            yell_msg = yell_msg + parse[i] + " ";
        }
        yell_msg = yell_msg + parse[parse.size()-1] + "\n";
        string msg = "*** " + name[id] + " yelled ***: " + yell_msg;
        for(int i=1; i<LISTENQ; i++)
        {
            if(client[i] > 0)
            {
                write(client[i], msg.c_str(), msg.length());
            }
        }
        return 0;
    }

    else if(parse[0] == "name")
    {
        for(int i=1; i<LISTENQ; i++)
        {
            if(name[i] == parse[1])
            {
                string msg = "*** User '" + parse[1] + "' already exists. ***\n";
                write(clientsock, msg.c_str(), msg.length());
                return 0;
            }
        }
        name[id] = parse[1];
        string msg = "*** User from " + address[id] + ":" + to_string(port[id]) + " is named '" + parse[1] + "'. ***\n";
        for(int i=1; i<LISTENQ; i++)
        {
            if(client[i] > 0)
            {
                write(client[i], msg.c_str(), msg.length());
            }
        }
        return 0;
    }
    return 1;
}


void parse_preprocess(vector<string> parse, int clientsock, int id, string whole_command)
{
    signal(SIGCHLD, SIG_IGN);
    map<string, string>::iterator iter;
    for(iter=env[id].begin(); iter != env[id].end(); iter++)
        setenv(iter->first.c_str(), iter->second.c_str(), 1);
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
    for(int i = 0; i < parse_num; i++)
    {
        if(parse[i] == ">" || parse[i] == "|" || ((parse[i][0] == '|' || parse[i][0] == '!') && parse[i].size() > 1))
        {
            command_now += 1;
            parse_userpipe.push_back(make_pair(0,0));
        }
        if(parse[i][0] == '<' && parse[i].size() > 1)
        {
            int isuserpipe = stoi(parse[i].substr(1, parse[i].size() - 1));
            if(client[isuserpipe] < 0)
            {
                string msg = "*** Error: user #" + to_string(isuserpipe) + " does not exist yet. ***\n";
                write(clientsock, msg.c_str(), msg.length());
                parse_userpipe[command_now].first = -1;
            }
            else if(alluserpipe[id].find(abs(isuserpipe)) == alluserpipe[id].end())
            {
                string msg = "*** Error: the pipe #" + to_string(abs(isuserpipe)) + "->#" + to_string(id) + " does not exist yet. ***\n";
                write(clientsock, msg.c_str(), msg.length());
                parse_userpipe[command_now].first = -1;
            }
            else
            {
                parse_userpipe[command_now].first = isuserpipe;
                string msg = "*** " + name[id] + " (#" + to_string(id) + ") just received from " + name[abs(isuserpipe)] + " (#" + to_string(abs(isuserpipe)) + ") by '" + whole_command + "' ***\n";
                for(int j=1; j<LISTENQ; j++)
                {
                    if(client[j] > 0)
                    {
                        write(client[j], msg.c_str(), msg.length());
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
            int isuserpipe = stoi(parse[i].substr(1, parse[i].size() - 1));
            if(client[isuserpipe] < 0)
            {
                string msg = "*** Error: user #" + to_string(isuserpipe) + " does not exist yet. ***\n";
                write(clientsock, msg.c_str(), msg.length());
                parse_userpipe[command_now].second = -1;
            }
            else if(alluserpipe[isuserpipe].find(id) != alluserpipe[isuserpipe].end())
            {
                string msg = "*** Error: the pipe #" + to_string(id) + "->#" + to_string(isuserpipe) + " already exists. ***\n";
                write(clientsock, msg.c_str(), msg.length());
                parse_userpipe[command_now].second = -1;
            }
            else
            {
                parse_userpipe[command_now].second = isuserpipe;
                int *pipfd = new int[2];
                pipe(pipfd);
                alluserpipe[isuserpipe][id] = pipfd;
                string msg = "*** " + name[id] + " (#" + to_string(id) + ") just piped '" + whole_command + "' to " + name[isuserpipe] + " (#" + to_string(isuserpipe) + ") ***\n";
                for(int j=1; j<LISTENQ; j++)
                {
                    if(client[j] > 0)
                    {
                        write(client[j], msg.c_str(), msg.length());
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
            process_line[id] = (process_line[id] + 1) % MAXPIPE;
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
        process_line[id] = (process_line[id] + 1) % MAXPIPE;
    }
    if (redirect)
        fclose(file);
}

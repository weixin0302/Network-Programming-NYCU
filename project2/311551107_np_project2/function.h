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
#define LISTENQ 30

int now = 0;
int wait_childid;

vector<int *> allpipe;
unordered_map<int, int *> allnumpipe;

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

int decide_function(vector<string> parse, int redirect, int isnumpipe, int nextpipe, int prepipe, int *pipfd)
{
    if (parse[0] == "exit")
        return 1;

    else if (parse[0] == "printenv")
    {
        char env[MAXLEN];
        strcpy(env, parse[1].c_str());
        if (!getenv(env))
            return 0;
        cout << getenv(env) << endl;
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

    else
    {
        int parse_num = parse.size();
        int wait_status;
        char *argv[parse_num + 1];

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
            if (allnumpipe.find(now) != allnumpipe.end())
            {

                close(allnumpipe[now][1]);
                dup2(allnumpipe[now][0], 0);
                close(allnumpipe[now][0]);
                allnumpipe.erase(now);
            }
            if (isnumpipe)
            {
                if (isnumpipe < 0)
                    dup2(allnumpipe[target][1], 2);
                close(allnumpipe[target][0]);
                dup2(allnumpipe[target][1], 1);
                close(allnumpipe[target][1]);
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

            if (!nextpipe && !isnumpipe)
                waitpid(pid, &wait_status, 0);
        }
    }
    return 0;
}

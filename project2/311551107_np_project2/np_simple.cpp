#include "part1_function.h"

int main(int argc, char* argv[])
{
    int sockfd, newsockfd, childpid;
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
    signal(SIGCHLD, reaper);
    setbuf(stdout, NULL);

    while(1)
    {
        socklen_t clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);
    
        childpid = fork();
        if(childpid==0){
            close(sockfd);
            dup2(newsockfd, 0);
            dup2(newsockfd, 1);
            dup2(newsockfd, 2);

            string line;
            vector<string> parse;
            char redir[MAXLEN];
            setenv("PATH", "bin:.", 1);
            signal(SIGCHLD, SIG_IGN); // signal handler
            cout << "% ";
            while (getline(cin, line))
            {
                
                if (line.length() == 0)
                {
                    cout << "% ";
                    continue;
                }
                parse = string_split(line, WHITE_SPACE);
                int parse_num = parse.size();
                int redirect = 0;  // store redirect fd
                int isnumpipe = 0; // store target line
                int nextpipe = 0;  // has next pipe or not
                int prepipe = 0;   // has pre pipe or not
                int pipfd[2];
                int wait_status;
                int exit_signal = 0;
                FILE *file;
                int start = 0;
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
                        decide_function(command, redirect, isnumpipe, nextpipe, prepipe, pipfd);
                        nextpipe = 0;
                        prepipe = 1;
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
                        decide_function(command, redirect, isnumpipe, nextpipe, prepipe, pipfd);
                        isnumpipe = 0;
                        now = (now + 1) % MAXPIPE;
                    }
                }

                vector<string> command;
                for (int j = start; j < parse.size(); j++)
                {
                    command.push_back(parse[j]);
                }
                if (command.size() > 0)
                {
                    exit_signal = decide_function(command, redirect, isnumpipe, nextpipe, prepipe, pipfd);
                    now = (now + 1) % MAXPIPE;
                }
                if (redirect)
                    fclose(file);
                if (exit_signal){
                    close(0);
                    close(1);
                    close(2);
                    exit(0);
                }
                    
                cout << "% ";
            }
        }
        close(newsockfd);
    }
    return 0;
}
#include "part2_function.h"

int main(int argc, char* argv[])
{
    
    int sockfd, newsockfd, childpid;
    int flag=1;
    int nready;
    struct sockaddr_in serv_addr, cli_addr;
    

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[1]));
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));

    bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
    listen(sockfd, LISTENQ);
    int fdmax = sockfd;
    int max_id = 0;
    FD_ZERO(&afds);
    FD_SET(sockfd, &afds);
    for(int i=0; i<LISTENQ; i++){
        env[i]["PATH"] = "bin:.";
    }
    setbuf(stdout, NULL);
    signal(SIGCHLD, reaper);
    while(1)
    {
        int exit = 0;
        memcpy(&rfds, &afds, sizeof(rfds));
        nready = select(fdmax+1, &rfds, NULL, NULL, NULL);
        socklen_t clilen = sizeof(cli_addr);
        if(FD_ISSET(sockfd, &rfds))
        {
            newsockfd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);
            if(newsockfd > fdmax)  fdmax = newsockfd;
            int i, j;
            for(i=1; i<LISTENQ; i++){
                if(client[i]<0) break;
            }
            if(i>max_id) max_id = i;
            char addr[20];
            inet_ntop(AF_INET, &cli_addr.sin_addr.s_addr, addr, sizeof(cli_addr));
            client[i] = newsockfd;
            port[i] = ntohs(cli_addr.sin_port);
            address[i] = string(addr);

            string msg = "*** User '" + name[i]  + "' entered from " + address[i] + ":" + to_string(port[i]) + ". ***\n";
            for(j=1; j<LISTENQ; j++){      //broadcast login message
                if(j==i){
                    string newmsg = welcome_message + msg + "% ";
                    write(client[j], newmsg.c_str(), newmsg.length());
                }
                else write(client[j], msg.c_str(), msg.length());
            }
            cout << "*** User '" << name[i]  << "' entered from " << address[i] << ":" << port[i] << ". ***" << endl;
            FD_SET(newsockfd, &afds);
        }
        for(int i=0; i<=max_id; i++)
        {
            int clientsock = client[i];
            if(clientsock < 0) continue;
            if(FD_ISSET(clientsock, &rfds))
            {
                vector<string> parse;
                char input[MAXLEN] = "";
                read(clientsock, input, sizeof(input));
                string line = string(input);
                for(int j=0; j<line.length(); j++){
                    if(line[j] == '\r' || line[j] == '\n'){
                        line.erase(line.begin()+j);
                        j --;
                    }
                }
                cout <<  i << " " << line << "\n";
                parse = string_split(line, WHITE_SPACE);
                if(decided_built_in_function(parse, clientsock, i, line))
                {
                    parse_preprocess(parse, clientsock, i, line);
                }
                write(clientsock, "% ", strlen("% "));
            }
        }
    }
    return 0;
}
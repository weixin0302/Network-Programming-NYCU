#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <array>
#include <sys/wait.h>

using namespace std;
using namespace boost::asio;

boost::asio::io_service ioservice;

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


class session : public std::enable_shared_from_this<session>
{
private:
    ip::tcp::socket socket_;
    enum { max_length = 1024 };
    array<char, max_length> data_;
    string request_method, request_uri, query_string, server_protocol, http_host, server_addr, server_port, remote_addr, remote_port;

public:
  session(ip::tcp::socket socket) : socket_(std::move(socket)){}
  void start()
  {
    do_read();
  }

private:
  void do_read()
  {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_, max_length), [this, self](boost::system::error_code ec, std::size_t length)
    {
        if (!ec)
        {
            string request(data_.begin(), data_.end());
            vector<string> parse = string_split(request, "\r\n");
            vector<string> first = string_split(parse[0], " ");
            request_method = first[0];
            if(first[1].find("?") == string::npos)
            {
              request_uri = first[1];
            }
            else 
            {
              request_uri = first[1].substr(0, first[1].find("?"));
              query_string = first[1].substr(first[1].find("?")+1, first[1].length() - first[1].find('?'));
            }

            server_protocol = first[2];

            vector<string> second = string_split(parse[1], " ");
            http_host = second[1];

            server_addr = socket_.local_endpoint().address().to_string();
            server_port = to_string(socket_.local_endpoint().port());
            remote_addr = socket_.remote_endpoint().address().to_string();
            remote_port = to_string(socket_.remote_endpoint().port());

            do_cgi(request_uri);
        }
    });
  }
  void do_write() 
  {
    auto self(shared_from_this()); 
    socket_.async_send(buffer(data_, max_length),[this, self](boost::system::error_code ec, size_t length ) {});
  }

  void do_cgi(string filename)
  {
    ioservice.notify_fork(io_service::fork_prepare);
    pid_t pid = fork();
    if(pid == 0)
    {
        ioservice.notify_fork(boost::asio::io_service::fork_child);
        filename = "." + filename;
        setenv("REQUEST_METHOD", request_method.c_str(), 1);
        setenv("REQUEST_URI", request_uri.c_str(), 1);
        setenv("QUERY_STRING", query_string.c_str(), 1);
        setenv("SERVER_PROTOCOL", server_protocol.c_str(), 1);
        setenv("HTTP_HOST", http_host.c_str(), 1);
        setenv("SERVER_ADDR", server_addr.c_str(), 1);
        setenv("SERVER_PORT", server_port.c_str(), 1);
        setenv("REMOTE_ADDR", remote_addr.c_str(), 1);
        setenv("REMOTE_PORT", remote_port.c_str(), 1);


        socket_.send(buffer(string("HTTP/1.1 200 OK\r\n")));

        char *argv[2];
        argv[0] = strdup(filename.c_str());
        argv[1] = NULL;

        // cout << filename << endl;

        dup2(socket_.native_handle(), STDOUT_FILENO);
        if(execv(argv[0], argv) == -1)
        {
            cerr << "Unknown command: [" << argv[0] << "]. " << endl;
            exit(-1);
        }
        exit(0);
    }
    else
    {
        ioservice.notify_fork(boost::asio::io_service::fork_parent);
        socket_.close();
    }
  }
};


class server
{
private:
    ip::tcp::acceptor acceptor_;
    ip::tcp::socket socket_;
public:
  server(short port) : acceptor_(ioservice, ip::tcp::endpoint(ip::tcp::v4(), port)), socket_(ioservice)
  {
    do_accept();
  }

private:
  void do_accept()
  {
    acceptor_.async_accept(socket_, [this](boost::system::error_code ec)
    {
        if (!ec)
        {
            std::make_shared<session>(std::move(socket_))->start();
        }
        do_accept();
    });
  }
};

int main(int argc, char* argv[])
{    
    if (argc != 2)
    {
       std::cerr << "Usage:" << argv[0] << " [port]" << endl;
      return 1;
    }

    try{
        signal(SIGCHLD, SIG_IGN);
        setenv("PATH", "/usr/bin:.", 1);
        short port = atoi(argv[1]);
        server server(port);
        ioservice.run();
    } 
    catch (exception& e) {
        cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
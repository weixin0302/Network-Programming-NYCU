#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <string>
#include <vector>
#include <unistd.h>
#include <array>
#include <fstream>
#include <boost/algorithm/string/replace.hpp>

using namespace std;
using namespace boost::asio;
#define MAXSERVER 5

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

class shell : public std::enable_shared_from_this<shell>
{
private:
    ip::tcp::socket tcp_socket;
    shared_ptr<boost::asio::ip::tcp::socket> web_socket;
    ip::tcp::resolver resolver_;
    ip::tcp::resolver::query query_;
    enum { max_length = 1024 };
    array<char, max_length> data_;
    string server_id, server_host, server_port, server_file;
    vector<string> server_cmd;
    
public:
    shell(shared_ptr<boost::asio::ip::tcp::socket> socket, string i, string h, string p, vector<string> c) : 
    tcp_socket(ioservice), web_socket(socket), resolver_(ioservice), query_(ip::tcp::v4(), h, p), server_id(i), server_host(h), server_port(p), server_cmd(c){}
    void start()
    {
        do_connect();
    }

private:
    void do_shell()
    {
        auto self(shared_from_this());
        tcp_socket.async_read_some(boost::asio::buffer(data_), [this, self](boost::system::error_code ec, std::size_t length)
        {
            if (!ec)
            {
                string receive_msg;
                for (int i = 0; i < length; ++i) {
                    receive_msg += data_[i];
                }
                // cout << server_host + " " + receive_msg << endl;
                output_shell(server_id, receive_msg);
                if(server_cmd.size() != 0)
                {
                    if(receive_msg.find('%') != string::npos)
                    {
                        string command = server_cmd[0];
                        output_command(server_id, command);
                        tcp_socket.async_send(boost::asio::buffer(command), [this, self, command](boost::system::error_code ec, size_t){});
                        server_cmd.erase(server_cmd.begin());
                        do_shell();
                    }
                    else do_shell();
                }
            }
            else
            {
                // cout << server_host + ":" + server_port + " read fail" << endl;
                tcp_socket.close();
            }
        });
    }

    void do_connect()
    {
        auto self(shared_from_this());
        resolver_.async_resolve(query_, [this, self](const boost::system::error_code &ec, ip::tcp::resolver::iterator it){
                async_connect(tcp_socket, it, [this, self](const boost::system::error_code &ec, ip::tcp::resolver::iterator){
                    if(!ec)
                    {
                        // cout << server_host + ":" + server_port + " connect" << endl;
                        do_shell();
                    }
                });
            });
    }
        

    void output_shell(string session, string content)
    {
        auto self(shared_from_this());
        // cout << server_host << endl;
        boost::replace_all(content, "\r\n", "&NewLine;");
        boost::replace_all(content, "\n", "&NewLine;");
        boost::replace_all(content, "\\", "\\\\");
        boost::replace_all(content, "\'", "\\\'");
        boost::replace_all(content, "<", "&lt;");
        boost::replace_all(content, ">", "&gt;");
        string msg =  "<script>document.getElementById('" + session + "').innerHTML += '" + content + "';</script>" + "\n"; 
        web_socket->async_send(boost::asio::buffer(msg, msg.length()), [this, self](boost::system::error_code ec, size_t){});
        fflush(stdout);
    }

    void output_command(string session, string content)
    {
        auto self(shared_from_this());
        // cout << server_host << endl;
        boost::replace_all(content,"\r\n","&NewLine;");
        boost::replace_all(content,"\n","&NewLine;");
        boost::replace_all(content, "\\", "\\\\");
        boost::replace_all(content, "\'", "\\\'");
        boost::replace_all(content,"<","&lt;");
        boost::replace_all(content,">","&gt;");
        string msg = "<script>document.getElementById('" + session + "').innerHTML += '<font color=\"green\">" + content + "';</script>" + "\n";
        web_socket->async_send(boost::asio::buffer(msg, msg.length()), [this, self](boost::system::error_code ec, size_t){});
        fflush(stdout);
    }
};


class session : public std::enable_shared_from_this<session>
{
private:
    ip::tcp::socket socket_;
    shared_ptr<boost::asio::ip::tcp::socket> tcp_socket;
    enum { max_length = 1024 };
    array<char, max_length> data_;
    string request_method, request_uri, query_string, server_protocol, http_host, server_addr, server_port, remote_addr, remote_port;
    vector<string> id;
    vector<string> host;
    vector<string> port;
    vector<string> file;
    vector<vector<string>> all_cmd;

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

            socket_.send(boost::asio::buffer(string("HTTP/1.1 200 OK\r\n")));

            if(request_uri == "/panel.cgi")
            {
                do_panel();
            }
            else if(request_uri == "/console.cgi")
            {
                do_console();
            }
        }
    });
  }

  void do_panel()
  {
    string msg = "Content-type: text/html\r\n\r\n";
    msg += R"(
            <!DOCTYPE html>
            <html lang="en">
            <head>
                <title>NP Project 3 Panel</title>
                <link
                rel="stylesheet"
                href="https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css"
                integrity="sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO"
                crossorigin="anonymous"
                />
                <link
                href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
                rel="stylesheet"
                />
                <link
                rel="icon"
                type="image/png"
                href="https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png"
                />
                <style>
                * {
                    font-family: 'Source Code Pro', monospace;
                }
                </style>
            </head>
            <body class="bg-secondary pt-5">
                <form action="console.cgi" method="GET">
                <table class="table mx-auto bg-light" style="width: inherit">
                    <thead class="thead-dark">
                    <tr>
                        <th scope="col">#</th>
                        <th scope="col">Host</th>
                        <th scope="col">Port</th>
                        <th scope="col">Input File</th>
                    </tr>
                    </thead>
                    <tbody>)";
    for(int i = 0; i < MAXSERVER; i++)
    {
        msg += R"(
             <tr>
                <th scope="row" class="align-middle">Session )" + to_string(i+1) + R"(</th>
                <td>
                <div class="input-group">
                    <select name="h)" + to_string(i) + R"(" class="custom-select">
                    <option></option><option value="nplinux1.cs.nctu.edu.tw">nplinux1</option><option value="nplinux2.cs.nctu.edu.tw">nplinux2</option><option value="nplinux3.cs.nctu.edu.tw">nplinux3</option><option value="nplinux4.cs.nctu.edu.tw">nplinux4</option><option value="nplinux5.cs.nctu.edu.tw">nplinux5</option><option value="nplinux6.cs.nctu.edu.tw">nplinux6</option><option value="nplinux7.cs.nctu.edu.tw">nplinux7</option><option value="nplinux8.cs.nctu.edu.tw">nplinux8</option><option value="nplinux9.cs.nctu.edu.tw">nplinux9</option><option value="nplinux10.cs.nctu.edu.tw">nplinux10</option><option value="nplinux11.cs.nctu.edu.tw">nplinux11</option><option value="nplinux12.cs.nctu.edu.tw">nplinux12</option>
                    </select>
                    <div class="input-group-append">
                    <span class="input-group-text">.cs.nctu.edu.tw</span>
                    </div>
                </div>
                </td>
                <td>
                <input name="p)" + to_string(i) + R"(" type="text" class="form-control" size="5" />
                </td>
                <td>
                <select name="f)" + to_string(i) + R"(" class="custom-select">
                    <option></option>
                    <option value="t1.txt">t1.txt</option><option value="t2.txt">t2.txt</option><option value="t3.txt">t3.txt</option><option value="t4.txt">t4.txt</option><option value="t5.txt">t5.txt</option>
                </select>
                </td>
            </tr>)";
    }
    msg += R"(
        <tr>
              <td colspan="3"></td>
              <td>
                <button type="submit" class="btn btn-info btn-block">Run</button>
              </td>
            </tr>
          </tbody>
        </table>
      </form>
    </body>
  </html>)";

    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(msg, msg.size()),[this, self](boost::system::error_code ec, std::size_t)
    {
        if (!ec)
        {
            this->socket_.close();
        }
    });
  }

  void do_console()
  {
    auto self(shared_from_this());
    for(int i = 0; i < MAXSERVER; i++)
    {
        id.push_back(to_string(i));
        for(int j = 0; j < 3; j++)
        {
            if(query_string.find("&") != string::npos)
            {
                int index = query_string.find("&");
                string tmp = query_string.substr(3, index - 3);
                if(j==0) host.push_back(tmp);
                else if(j==1) port.push_back(tmp);
                else file.push_back(tmp);
                query_string = query_string.substr(index + 1);
            }
            else
            {
                string tmp = query_string.substr(3);
                file.push_back(tmp);
            }
        }
        if(host[host.size()-1] == "" || port[port.size()-1] == "" || file[file.size()-1] == "")
        {
            id.erase(id.end());
            host.erase(host.end());
            port.erase(port.end());
            file.erase(file.end());
        }
    }
    for(int i = 0; i < id.size(); i++)
    {
      ifstream ifs("./test_case/" + file[i]);
      string line;
      vector<string> cmd;
      while(getline(ifs,line)){
        cmd.push_back(line + "\n");
      }
      all_cmd.push_back(cmd);
      ifs.close();
    }

    send_console();
  }

  void send_console()
  {
    string head = R"(
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8" />
        <title>NP Project 3 Console</title>
        <link
        rel="stylesheet"
        href="https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css"
        integrity="sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO"
        crossorigin="anonymous"
        />
        <link
        href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
        rel="stylesheet"
        />
        <link
        rel="icon"
        type="image/png"
        href="https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png"
        />
        <style>
        * {
            font-family: 'Source Code Pro', monospace;
            font-size: 1rem !important;
        }
        body {
            background-color: #212529;
        }
        pre {
            color: #cccccc;
        }
        b {
            color: #ffffff;
        }
        </style>
    </head>
    <body>
        <table class="table table-dark table-bordered">
        <thead>
            <tr>)";

    for(int i = 0; i < id.size(); i++)
    {
        head += R"(            <th scope="col">)";
        head += host[i];
        head += R"(:)";
        head += port[i];
        head += R"(</th>)";
    }
    head += R"(        </tr>
      </thead>
      <tbody>
        <tr>)";

    for(int i = 0; i < id.size(); i++)
    {
        head += R"(            <td><pre id=")";
        head += id[i];
        head += R"(" class="mb-0"></pre></td>)";
    }

    head += R"(        </tr>
      </tbody>
    </table>
  </body>
</html>)";
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(head, head.size()),[this, self](boost::system::error_code ec, std::size_t)
    {
        shared_ptr<boost::asio::ip::tcp::socket> tcp_socket(make_shared<boost::asio::ip::tcp::socket>(move(socket_)));
        if(!ec){
            for(int i = 0; i < id.size(); i++)
            {
                make_shared<shell>(tcp_socket, id[i], host[i], port[i], all_cmd[i])->start();
            }
        }
    });
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
        short port = atoi(argv[1]);
        server server(port);
        ioservice.run();
    } 
    catch (exception& e) {
        cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}

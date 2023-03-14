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
#include <iostream>
#include <fstream>
#include <boost/algorithm/string/replace.hpp>

using namespace std;
using namespace boost::asio;
#define MAXSERVER 5

boost::asio::io_service ioservice;

vector<string> id;
vector<string> host;
vector<string> port;
vector<string> file;
vector<vector<string>> all_cmd;

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

class session : public std::enable_shared_from_this<session>
{
private:
    ip::tcp::socket socket_;
    ip::tcp::resolver resolver_;
    ip::tcp::resolver::query query_;
    enum { max_length = 1024 };
    array<char, max_length> data_;
    string server_id, server_host, server_port, server_file;
    vector<string> server_cmd;
    
public:
  session(string i, string h, string p, vector<string> c) : 
  socket_(ioservice), resolver_(ioservice), query_(ip::tcp::v4(), h, p), server_id(i), server_host(h), server_port(p), server_cmd(c){}
  void start()
  {
    do_connect();
  }

private:
  void do_shell()
  {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_), [this, self](boost::system::error_code ec, std::size_t length)
    {
      if (!ec)
      {
        string receive_msg;
        for (int i = 0; i < length; ++i) {
            receive_msg += data_[i];
        }
        output_shell(server_id, receive_msg);
        if(server_cmd.size() != 0)
        {
          if(receive_msg.find('%') != string::npos)
          {
            string command = server_cmd[0];
            output_command(server_id, command);
            socket_.async_send(boost::asio::buffer(command), [this, self, command](boost::system::error_code ec, size_t){});
            server_cmd.erase(server_cmd.begin());
            do_shell();
          }
          else do_shell();
        }
      }
      else
      {
        socket_.close();
      }
    });
  }

  void do_connect()
  {
    auto self(shared_from_this());
    resolver_.async_resolve(query_, [this, self](const boost::system::error_code &ec, ip::tcp::resolver::iterator it){
            async_connect(socket_, it, [this, self](const boost::system::error_code &ec, ip::tcp::resolver::iterator){
                if(!ec) do_shell();
                else cout << "<script>document.getElementById('" << server_id << "').innerHTML += '<font color=\"white\">" << "fail" << "</font>';</script>\n";
            });
        });
  }
    

  void output_shell(string session, string content)
  {
    boost::replace_all(content, "\r\n", "&NewLine;");
    boost::replace_all(content, "\n", "&NewLine;");
    boost::replace_all(content, "\\", "\\\\");
    boost::replace_all(content, "\'", "\\\'");
    boost::replace_all(content, "<", "&lt;");
    boost::replace_all(content, ">", "&gt;");
    cout << "<script>document.getElementById('" << session << "').innerHTML += '" << content << "';</script>" << endl; 
    fflush(stdout);
  }

  void output_command(string session, string content)
  {
    boost::replace_all(content,"\r\n","&NewLine;");
    boost::replace_all(content,"\n","&NewLine;");
    boost::replace_all(content, "\\", "\\\\");
    boost::replace_all(content, "\'", "\\\'");
    boost::replace_all(content,"<","&lt;");
    boost::replace_all(content,">","&gt;");
    cout << "<script>document.getElementById('" << session << "').innerHTML += '<font color=\"green\">" << content << "';</script>" << endl;
    fflush(stdout);
  }
};



int main()
{
    string QUERY_STRING = getenv("QUERY_STRING");
    for(int i = 0; i < MAXSERVER; i++)
    {
        id.push_back(to_string(i));
        for(int j = 0; j < 3; j++)
        {
            if(QUERY_STRING.find("&") != string::npos)
            {
                int index = QUERY_STRING.find("&");
                string tmp = QUERY_STRING.substr(3, index - 3);
                if(j==0) host.push_back(tmp);
                else if(j==1) port.push_back(tmp);
                else file.push_back(tmp);
                QUERY_STRING = QUERY_STRING.substr(index + 1);
            }
            else
            {
                string tmp = QUERY_STRING.substr(3);
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

    cout << "Content-type:text/html\r\n\r\n";
    cout << head;

    for(int i = 0; i < id.size(); i++){
        make_shared<session>(id[i], host[i], port[i], all_cmd[i])->start();
    }
    ioservice.run();
}
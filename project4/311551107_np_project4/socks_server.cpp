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
#include <fstream>
#include <sys/wait.h>

using namespace std;
using namespace boost::asio;

boost::asio::io_service ioservice;

class server
{
private:
    ip::tcp::acceptor acceptor_{ioservice};
    ip::tcp::socket socket_{ioservice};
    ip::tcp::socket client_socket_{ioservice};
    ip::tcp::socket server_socket_{ioservice};
    ip::tcp::resolver resolver_{ioservice};
    // ip::tcp::resolver::query query_;
    enum { max_length = 1024 };
    array<char, max_length> data_;
    char dst_msg[max_length];
    char sor_msg[max_length];
    struct request{
      char VN;
      char CD;
      unsigned char DSTPORT[2];
      unsigned short DST_PORT;
      unsigned char DSTIP[4];
      string DST_IP;
      string DOMAIN_NAME;
    };
    request sock_request;
    request sock_reply;
    int issocks4a = 0;
    string SRC_IP;
    unsigned short SRC_PORT;

public:
  server(uint16_t port) : acceptor_(ioservice, {ip::tcp::v4(), port})
  {
    do_accept();
  }

private:
  void do_accept()
  {
    acceptor_.async_accept([this](boost::system::error_code ec, ip::tcp::socket new_socket)
    {
        if (!ec)
        {
          client_socket_ = move(new_socket);
          ioservice.notify_fork(boost::asio::io_service::fork_prepare);
          if(fork() == 0){
              ioservice.notify_fork(boost::asio::io_service::fork_child);
              acceptor_.close();
              do_read();
          }
          else{
              ioservice.notify_fork(boost::asio::io_service::fork_parent);
              client_socket_.close();
              do_accept();
          }
        }
        else do_accept();
    });
  }
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

  string IPtoStr(unsigned char* iparr)
  {
      char buffer [16];
      sprintf(buffer, "%d.%d.%d.%d", iparr[0], iparr[1], iparr[2], iparr[3]);
      return string(buffer);
  }
  void do_read()
  {
    // auto self(shared_from_this());
    client_socket_.async_read_some(boost::asio::buffer(data_, max_length), [this](boost::system::error_code ec, std::size_t length)
    {
        if (!ec)
        {
          sock_request.VN = data_[0];
          sock_request.CD = data_[1];
          sock_request.DSTPORT[0] = data_[2];
          sock_request.DSTPORT[1] = data_[3];
          sock_request.DST_PORT = sock_request.DSTPORT[0] << 8 | sock_request.DSTPORT[1];
          sock_request.DSTIP[0] = data_[4];
          sock_request.DSTIP[1] = data_[5];
          sock_request.DSTIP[2] = data_[6];
          sock_request.DSTIP[3] = data_[7];
          sock_request.DST_IP = IPtoStr(sock_request.DSTIP);

          data_.fill('\0');
          
          if(sock_request.DSTIP[0] == 0 && sock_request.DSTIP[1] == 0 && sock_request.DSTIP[2] == 0 && sock_request.DSTIP[3] != 0) issocks4a = 1;
          
          if(issocks4a)
          {
            sock_request.DOMAIN_NAME = "";
            int begin;
            for(begin = 8; begin < length; begin++)
            {
              if(data_[begin] == 0)
              {
                begin++;
                break;
              }
            }
            for(int i = begin; i < length-1; i++)
            {
              sock_request.DOMAIN_NAME += data_[i];
            }
          }

          SRC_IP = client_socket_.remote_endpoint().address().to_string();
          SRC_PORT = client_socket_.remote_endpoint().port();

          cout << "<S_IP>: " << SRC_IP << endl;
          cout << "<S_PORT>: " << SRC_PORT << endl;
          cout << "<D_IP>: " << sock_request.DST_IP << endl;
          cout << "<D_PORT>: " << sock_request.DST_PORT << endl;
          cout << "<Command>: ";
          if(sock_request.CD == 1)
              cout << "CONNECT" << endl;
          else if(sock_request.CD == 2)
              cout << "BIND" << endl;

          if(sock_request.CD == 1)
          {
            if(issocks4a)
            {
              auto results = resolver_.resolve(sock_request.DOMAIN_NAME, to_string(sock_request.DST_PORT));
              for(auto entry : results)
              {
                  if(entry.endpoint().address().is_v4())
                  {
                    sock_request.DST_IP = entry.endpoint().address().to_string();
                  }
              }
              // cout << sock_request.DST_IP << endl;
            }
            // cout << sock_request.DST_IP << endl;
            do_connect_server();
          }
          else if(sock_request.CD == 2)
          {
            // cout << "CD = 2" << endl;
            do_accept_server();
          }
        }
    });
  }

  int pass_firewall()
  {
    // auto self(shared_from_this());
    ifstream ifs("socks.conf");
    string line;
    vector<string> test_ip;
    test_ip = string_split(sock_request.DST_IP, ".");

    string test_mode;
    if(sock_request.CD == 1) test_mode = "c";
    else test_mode = "b";
    
    while(getline(ifs,line))
    {
      vector<string> tmp = string_split(line, " ");
      string pass = tmp[0];
      string mode = tmp[1];
      string permit_ip_string = tmp[2];
      vector<string> permit_ip = string_split(permit_ip_string, ".");
      if(pass == "permit")
      {
        if(test_mode == mode)
        {
          for(int i = 0; i < 4; i++)
          {
            if(permit_ip[i] != test_ip[i] && permit_ip[i] != "*") break;
            if(i == 3) return 1;
          }
        }
      }
    }
    return 0;
  }

  void do_connect_server()
  {
    // auto self(shared_from_this());
    if(pass_firewall())
    {
      cout << "<Reply> Accept" << endl << endl;
      boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address::from_string(sock_request.DST_IP), sock_request.DST_PORT);
      server_socket_.async_connect(ep, [this](boost::system::error_code ec){
        if (!ec)
        {
          send_reply(1, 1);
          do_communication(1, 1);
        }
      });
    }
    else
    {
      cout << "<Reply> Reject" << endl << endl;
      send_reply(1, 0);
    }
  }

  void do_accept_server()
  {
    // auto self(shared_from_this());
    // cout << "accept............";
    if(pass_firewall())
    {
      cout << "<Reply> Accept" << endl << endl;
      boost::asio::ip::tcp::acceptor ftp_acceptor_(ioservice, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0));
      ftp_acceptor_.listen();
      unsigned short bind_port = ftp_acceptor_.local_endpoint().port();
      sock_request.DST_PORT = bind_port;
      sock_request.DSTPORT[0] = bind_port / 256;
      sock_request.DSTPORT[1] = bind_port % 256;
      send_reply(2, 1);

      ftp_acceptor_.accept(server_socket_);
      send_reply(2, 1);
      // read_from_server();
      // read_from_client();
      do_communication(1, 1);
    }
    else
    {
      cout << "<Reply> Reject" << endl << endl;
      send_reply(2, 0);
    }
  }

  void send_reply(int mode, int success)
  {
    // auto self(shared_from_this());
    vector<char> buffer_;
    buffer_.clear();
    buffer_.push_back(0);
    if(success) buffer_.push_back(90);
    else buffer_.push_back(91);
    if(mode == 1)
    {
      for(int i = 0; i < 6; i++)
      {
        buffer_.push_back(0);
      }
    }
    else if(mode == 2)
    {
      buffer_.push_back(sock_request.DSTPORT[0]);
      buffer_.push_back(sock_request.DSTPORT[1]);
      for(int i = 0; i < 4; i++)
      {
        buffer_.push_back(0);
      }
    }
    // cout << "ready to send reply" <<endl;
    boost::asio::async_write(client_socket_, boost::asio::buffer(buffer_, buffer_.size()), [this](boost::system::error_code ec, std::size_t ){
      // if(!ec) cout << "send reply sucess" <<endl;
      
    });
  }

  void do_communication(int server_to_client, int client_to_server)
  {
    // auto self(shared_from_this());
    if(server_to_client)
    {
      server_socket_.async_read_some(boost::asio::buffer(dst_msg, max_length-1), [this](boost::system::error_code ec, size_t length){
        if(!ec)
        {
          // cout << "pass to client" <<endl;
          boost::asio::async_write(client_socket_, boost::asio::buffer(dst_msg, length),[this](boost::system::error_code ec, std::size_t){
            if(!ec)
            {
              memset(dst_msg, '\0', max_length);
              do_communication(1, 0);
            }
          });
        }
      });
    }
    if(client_to_server)
    {
      client_socket_.async_read_some(boost::asio::buffer(sor_msg, max_length-1), [this](boost::system::error_code ec, size_t length){
        if(!ec)
        {
          // cout << "pass to server" <<endl;
          boost::asio::async_write(server_socket_, boost::asio::buffer(sor_msg, length),[this](boost::system::error_code ec, std::size_t){
            if(!ec)
            {
              memset(sor_msg, '\0', max_length);
              do_communication(0, 1);
            }
          });
        }
      });
    }
  }
};

int main(int argc, char* argv[])
{    
    if (argc != 2)
    {
       std::cerr << "Usage:" << argv[0] << " [port]" << endl;
      return 1;
    }
    signal(SIGCHLD, SIG_IGN);
    try{
        setenv("PATH", "/usr/bin:.", 1);
        // short port = atoi(argv[1]);
        server server(atoi(argv[1]));
        ioservice.run();
    } 
    catch (exception& e) {
        cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
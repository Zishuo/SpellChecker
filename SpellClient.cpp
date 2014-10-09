#include <arpa/inet.h>
#include <iostream>
#include <sstream>

//boost
#include <boost/progress.hpp>

//thrift
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include "./gen-cpp/SpellService.h"

//thrift namespace for convenience
using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;

int main(int argc, char* argv[])
{
    if(argc < 3)
    {
        std::cerr << "Usage : SpellClient {ip} {port} {\"word1 word2 ...\"}"<<std::endl;
        return 1;
    }

    //read server ip from command line and validate it.
    struct sockaddr_in sa;
    if(inet_pton(AF_INET, argv[1], &(sa.sin_addr)) == 0)
    {
        std::cerr << "invalid ip address." << std::endl;
        return 2;
    }

    //read port from command line and validate it
    int port = 0;
    try
    {
        port = std::stoi(argv[2]);
    }
    catch(const std::exception& e)
    {
        std::cerr << "invalid ip port." << std::endl;
        return 3;
    }

    boost::shared_ptr<Tsocket> socket(new TSocket(argv[1], port));
    boost::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
    boost::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));

    SpellServiceClient client(protocol);
    SpellRequest request;
    SpellResponse response;

    //read words from command line and split them by space.
    //put each word into vector as a string.
    std::istringstream iss(argv[3]);
    std::copy(std::istream_iterator<std::string>(iss),
              std::istream_iterator<std::string>(),back_inserter(request.to_check));
    try
    {
        //measure the response time
        boost::progress_timer t;
        //open thrift transport connection
        transport->open();
        //request
        client.spellcheck(response,request);
    }
    catch(const TException &e)
    {
        std::cerr << e.what() << std::endl;
        return 4;
    }

    //output response
    for(auto i = response.is_correct.begin(); i != response.is_correct.end(); ++i)
    {
        if(*i)
        {
            std::cout << "hit";
        }
        else
        {
            std::cout << "miss";
        }
        std::cout << "\t\t";
        std::cout << request.to_check[std::distance(response.is_correct.begin(),i)]
                  << std::endl;
    }
    return 0;
}

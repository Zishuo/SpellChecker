//Copyright (C) <2014>  <Zishuo Wang>
//
//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
//(at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <arpa/inet.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <random>
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
using namespace std;
int main(int argc, char* argv[])
{
    if(argc < 3)
    {
        cerr << "Usage : SpellClient {server list filename} {timeout(ms)} {\"word1 word2 ...\"}"<<std::endl;
        return 2;
    }

    //read server ips and ports from file and validate it.
    ifstream servers(argv[1]);
    if(!servers.is_open())
    {
        cerr << "Can not open servers list file." << std::endl;
        return 3;
    }

    vector<pair<string,unsigned short>> addresses;
    string ip;
    int port;
    while(servers >> ip)
    {
        bool valide = true;
        struct sockaddr_in sa;
        if(inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) == 0)
        {
            valide = false;
            cerr << "invalid ip address " << ip << endl;
        }
        servers >> port;
        if( port < 0 || port > 65535)
        {
            valide = false;
            cerr << "invalid tcp port : " << port << endl;
        }
        if(valide)
        {
            addresses.push_back(make_pair(ip,port));
            cerr << "server : " << ip << "\t" << port << endl;
        }
    }
    servers.close();

    //shuffle the address book.
    std::random_device seed;
    std::mt19937 random_generator(seed());
    shuffle(addresses.begin(),addresses.end(),random_generator);

    //read timeout
    unsigned int timeout = 2000;
    try
    {
        timeout = stoi(argv[2]);
    }
    catch(...)
    {
        cerr << "invalid timeout : " << argv[2] << endl;
        return 4;
    }

    //read words from command line and split them by space.
    //put each word into vector as a string.
    SpellRequest request;
    istringstream iss(argv[3]);
    copy(std::istream_iterator<std::string>(iss),
         istream_iterator<std::string>(),back_inserter(request.to_check));

    SpellResponse response;
    bool fail = true;
    for(auto ads = addresses.begin();  ads != addresses.end(); ++ads)
    {
        fail = false;
        try
        {
            cerr << "querying : " << ads->first << "\t" << ads->second << endl;
            boost::shared_ptr<TSocket> socket(new TSocket(ads->first, ads->second));
            boost::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
            boost::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
            SpellServiceClient client(protocol);

            //timeout
            socket->setConnTimeout(timeout);
            socket->setRecvTimeout(timeout);
            socket->setSendTimeout(timeout);
            //measure the response time
            boost::progress_timer t;
            //open thrift transport connection
            transport->open();
            //request
            client.spellcheck(response,request);
        }
        catch(...)
        {
            fail = true;
        }

        if(!fail)
        {
            break;
        }
    }

    if(fail)
    {
        cerr << "All servers down, cannot check spells." << endl;
        return 1;
    }
    else
    {
        //output response
        for(auto i = response.is_correct.begin(); i != response.is_correct.end(); ++i)
        {
            if(*i)
            {
                cout << "hit";
            }
            else
            {
                cout << "miss";
            }
            cout << "\t\t";
            cout << request.to_check[std::distance(response.is_correct.begin(),i)]
                 << endl;
        }

    }
    return 0;
}

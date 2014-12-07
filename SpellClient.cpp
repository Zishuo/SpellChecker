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
#include <thread>
//md5 hash
#include <openssl/md5.h>
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

//Create a Thrift Client to check words.
class WordChecker
{
public:
    WordChecker(const string& ipv4,unsigned short port):ipv4_(ipv4), port_(port),
        socket_(new TSocket(ipv4_, port_)),
        transport_(new TBufferedTransport(socket_)),
        protocol_(new TBinaryProtocol(transport_)),
        client_(protocol_)
    {
        socket_->setConnTimeout(timeout);
        socket_->setRecvTimeout(timeout);
        socket_->setSendTimeout(timeout);
    }

    int check(vector<bool>& result,const vector<string>& words)
    {
        SpellRequest request;
        request.to_check = words;
        SpellResponse response;
        try
        {
            transport_->open();
            client_.spellcheck(response,request);
        }
        catch(...)
        {
            return 1;
        }
        result = response.is_correct;
        return 0;
    }
public:
    static unsigned int timeout;
private:
    string ipv4_;
    unsigned short port_;
    boost::shared_ptr<TSocket> socket_;
    boost::shared_ptr<TTransport> transport_;
    boost::shared_ptr<TProtocol> protocol_;
    SpellServiceClient client_;
};
unsigned int WordChecker::timeout = 2000;

//Decide word which shard to go.
class WordDispatcher
{
public:
    WordDispatcher(unsigned int mod_space = 2):mod_space_(mod_space)
    {

    }

    unsigned int dispatch(const string & word)
    {
        MD5((unsigned char *)word.c_str(),word.size(),md5_val);
        return md5_val[15] % mod_space_;
    }
private:
    unsigned int mod_space_;
    unsigned char md5_val[16];

};

//For each Shard, do a fault tolerence query.
class ShardTask
{
public:
    ShardTask(const vector<pair<string,unsigned short>>& shard_list):shard_list_(shard_list)
    {

    }

    void Query(vector<bool>& is_correct,const vector<string>& to_check)
    {
        bool succss = false;
        auto add = shard_list_.begin();
        while(!succss && add != shard_list_.end())
        {
            WordChecker checker(add->first,add->second);
            if( checker.check(is_correct,to_check) == 0)
            {
                succss = true;
            }
            ++add;
        }
    }

private:
    const vector<pair<string, unsigned short>>& shard_list_;
};

//load server list
int load_server_list(vector<pair<string,unsigned short>>& list1 ,vector<pair<string,unsigned short>>& list2, char shard1_name[], char shard2_name[])
{

    //read server ips and ports from file and validate it.
    ifstream shard1_file(shard1_name);
    ifstream shard2_file(shard2_name);
    if(!shard1_file.is_open())
    {
        cerr << "Can not open shard1_file list file." << std::endl;
        exit(3);
    }
    if(!shard2_file.is_open())
    {
        cerr << "Can not open shard2_file list file." << std::endl;
        exit(3);
    }

    string ip;
    int port;
    while(shard1_file >> ip)
    {
        bool valide = true;
        struct sockaddr_in sa;
        if(inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) == 0)
        {
            valide = false;
            cerr << "invalid ip address " << ip << endl;
        }
        shard1_file >> port;
        if( port < 0 || port > 65535)
        {
            valide = false;
            cerr << "invalid tcp port : " << port << endl;
        }
        if(valide)
        {
            list1.push_back(make_pair(ip,port));
            cerr << "server : " << ip << "\t" << port << endl;
        }
    }
    shard1_file.close();

    while(shard2_file >> ip)
    {
        bool valide = true;
        struct sockaddr_in sa;
        if(inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) == 0)
        {
            valide = false;
            cerr << "invalid ip address " << ip << endl;
        }
        shard2_file >> port;
        if( port < 0 || port > 65535)
        {
            valide = false;
            cerr << "invalid tcp port : " << port << endl;
        }
        if(valide)
        {
            list2.push_back(make_pair(ip,port));
            cerr << "server : " << ip << "\t" << port << endl;
        }
    }
    shard2_file.close();
    return 0;
}


int main(int argc, char* argv[])
{
    if(argc < 3)
    {
        cerr << "Usage : SpellClient {shard1_file} {shard2} {timeout(ms)} {\"word1 word2 ...\"}"<<std::endl;
        return 2;
    }

    //read server list files
    vector<pair<string,unsigned short>> list1;
    vector<pair<string,unsigned short>> list2;
    load_server_list(list1,list2,argv[1],argv[2]);
    //shuffle the address book.
    std::random_device seed;
    std::mt19937 random_generator(seed());
    shuffle(list1.begin(),list1.end(),random_generator);
    shuffle(list2.begin(),list2.end(),random_generator);

    //read timeout
    try
    {
        WordChecker::timeout = stoi(argv[3]);
    }
    catch(...)
    {
        cerr << "invalid timeout : " << argv[3] << endl;
        return 4;
    }

    //read words from command line and split them by space.
    //put each word into vector as a string.
    vector<string> request;
    istringstream iss(argv[4]);
    copy(std::istream_iterator<std::string>(iss),
         istream_iterator<std::string>(),back_inserter(request));

    WordDispatcher dsp;
    //preserve original order.
    vector<pair<size_t,size_t>> mapper;
    
    //words to be sent to shards
    vector<string> shard1_words;
    vector<string> shard2_words;
    for(auto& i : request)
    {
        //no need to lowercase 
        
        //dispatch word to shard
        unsigned int shard = dsp.dispatch(i);
        if(shard == 0)
        {
            mapper.push_back(make_pair(0,shard1_words.size()));
            shard1_words.push_back(i);
        }
        if(shard == 1)
        {
            mapper.push_back(make_pair(1,shard2_words.size()));
            shard2_words.push_back(i);
        }
    }

    ShardTask task1(list1);
    ShardTask task2(list2);
    vector<bool> shard1_result;
    vector<bool> shard2_result;
   
    //do concurrently queries. 
    thread worker1(&ShardTask::Query,task1,ref(shard1_result),ref(shard1_words));
    thread worker2(&ShardTask::Query,task2,ref(shard2_result),ref(shard2_words));
    
    //wait all task to complete.
    worker1.join();
    worker2.join();

    //print all words true/false in original order.
    for(size_t i = 0; i != request.size(); ++i)
    {
        if(mapper[i].first == 0)
        {
            cout << request[i] << " : " << shard1_result[mapper[i].second] << endl;
        }

        if(mapper[i].first == 1)
        {
            cout << request[i] << " : " << shard2_result[mapper[i].second] << endl;
        }
    }

    return 0;
}

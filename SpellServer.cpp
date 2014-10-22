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

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <unordered_set>

//boost log
#include <boost/smart_ptr.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/progress.hpp>

//transport layer
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TTransportUtils.h>

//server and alternatives
#include <thrift/server/TSimpleServer.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/server/TNonblockingServer.h>
//protocol
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>

#include "./gen-cpp/SpellService.h"

//thrift namespace for convenience
using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::server;

//inherit from SpellServiceIf, overload the pure virtual function to
//implement business logic.
class SpellCheckHandler : public SpellServiceIf
{
public:
    SpellCheckHandler(std::unordered_set<std::string> & dic):dic_(dic) {}

    //handle spell check request.
    void spellcheck(SpellResponse& _return, const SpellRequest& request)
    {
        BOOST_LOG_TRIVIAL(debug) << "recieve spell check request";
        BOOST_LOG_TRIVIAL(trace) << "size " << request.to_check.size();

        const std::vector<std::string> &to_check = request.to_check;
        std::vector<bool> is_correct;
        //travesal requested words
        for(auto &word : to_check)
        {
            //found in dic
            if(dic_.find(word) != dic_.end())
            {
                is_correct.push_back(true);
                BOOST_LOG_TRIVIAL(trace) <<"hit\t"<<word;
            }
            else
            {
                is_correct.push_back(false);
                BOOST_LOG_TRIVIAL(trace) <<"miss\t"<<word;
            }
        }

        BOOST_LOG_TRIVIAL(debug) << "response done.";
        _return.is_correct = std::move(is_correct);
    }
private:
    std::unordered_set<std::string> dic_;
};


void boost_log_init(int severity)
{
    namespace logging = boost::log;
    namespace keywords = boost::log::keywords;
    namespace sinks = boost::log::sinks;
    logging::core::get()->set_filter
    (
        logging::trivial::severity >= severity
    );
}

int main(int argc, char * argv[])
{
    //check parameter number
    if(argc < 3)
    {
        BOOST_LOG_TRIVIAL(fatal) << "input parameter number is not correct.";
        std::cerr << "Usage : " << "{port} " << " {dictionary file} " <<" [log severity level = 2]"<<std::endl;
        return 1;
    }

    //read port and validate
    int port = 0;
    try
    {
        port = std::stoi(argv[1]);
    }
    catch(const std::exception& e)
    {
        BOOST_LOG_TRIVIAL(fatal) << "invalid port number : " << argv[1];
        return 2;
    }

    //open dictionary file
    std::ifstream dic_file(argv[2]);
    if(!dic_file.is_open())
    {
        BOOST_LOG_TRIVIAL(fatal) << argv[2] <<" can not be opened.";
        std::cerr << "dictionary file can not be opened." <<std::endl;
    }

    //set log severity
    int severity = 2;
    if(argc == 4)
    {
        try
        {
            severity = std::stoi(argv[3]);
        }
        catch(const std::exception& e)
        {
            BOOST_LOG_TRIVIAL(warning) <<"input log severity : " <<e.what();
        }
    }
    boost_log_init(severity);

    //initialize dictionary,store in a hash set.
    std::unordered_set<std::string> dic;
    std::string word;
    while(dic_file >> word)
    {
        dic.insert(word);
    }
    dic_file.close();
    BOOST_LOG_TRIVIAL(info) << "load dictionary done.";

    boost::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());
    boost::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
    boost::shared_ptr<SpellCheckHandler> handler(new SpellCheckHandler(dic));
    boost::shared_ptr<TProcessor> processor(new SpellServiceProcessor(handler));
    boost::shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));

    //alternatives : TThreadedServer,TThreadPoolServer
    try
    {
        TSimpleServer server(processor,
                             serverTransport,
                             transportFactory,
                             protocolFactory);
        BOOST_LOG_TRIVIAL(info) << "server running...";
    }
    catch(const std::exception& e)
    {
        BOOST_LOG_TRIVIAL(info) << e.what();
        BOOST_LOG_TRIVIAL(info) << "server stoped.";
    }

    return 0;
}
